#include "Splicer.h"
#include <rct/Hash.h>
#include <rct/List.h>
#include <rct/Thread.h>
#include <rct/SignalSlot.h>
#include <mutex>
#include <condition_variable>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/sendfile.h>

class SplicerThread : public Thread
{
public:
    SplicerThread()
    {
        ::pipe(msgPipe);
    }
    ~SplicerThread()
    {
        ::close(msgPipe[0]);
        ::close(msgPipe[1]);
    }

    virtual void run();

    void splice(int from, int to);
    unsigned int addCloseCallback(std::function<void(int)>&& cb);
    unsigned int addErrorCallback(std::function<void(Splicer::ErrorType, int, int)>&& cb);
    void removeCloseCallback(unsigned int id);
    void removeErrorCallback(unsigned int id);

private:
    std::mutex mutex;
    int msgPipe[2];
    Hash<int, int> fds;

    Signal<std::function<void(int)> > closed;
    Signal<std::function<void(Splicer::ErrorType, int, int)> > error;
};

unsigned int SplicerThread::addCloseCallback(std::function<void(int)>&& cb)
{
    std::unique_lock<std::mutex> locker(mutex);
    return closed.connect(std::move(cb));
}

unsigned int SplicerThread::addErrorCallback(std::function<void(Splicer::ErrorType, int, int)>&& cb)
{
    std::unique_lock<std::mutex> locker(mutex);
    return error.connect(std::move(cb));
}

void SplicerThread::removeCloseCallback(unsigned int id)
{
    std::unique_lock<std::mutex> locker(mutex);
    closed.disconnect(id);
}

void SplicerThread::removeErrorCallback(unsigned int id)
{
    std::unique_lock<std::mutex> locker(mutex);
    error.disconnect(id);
}

void SplicerThread::run()
{
    fd_set rd;
    int max;
    Hash<int, int> local;
    for (;;) {
        {
            std::unique_lock<std::mutex> locker(mutex);
            local = fds;
        }
        FD_ZERO(&rd);
        FD_SET(msgPipe[0], &rd);
        max = msgPipe[0];
        for (const auto& it : local) {
            FD_SET(it.first, &rd);
            max = std::max(it.first, max);
        }
        const int s = ::select(max + 1, &rd, 0, 0, 0);
        if (s <= 0) {
            fprintf(stderr, "splice thread select failed %d %s\n", errno, strerror(errno));
            return;
        }
        if (FD_ISSET(msgPipe[0], &rd)) {
            char c;
            ::read(msgPipe[0], &c, 1);
        }

        char buf[8192];
        for (const auto& it : local) {
            if (FD_ISSET(it.first, &rd)) {
                //const ssize_t spliced = ::splice(it.first, 0, it.second, 0, 32768, 0);
                // const ssize_t spliced = ::sendfile(it.second, it.first, 0, 32768);
                // if (spliced <= 0) {
                //     printf("splice error %d (%d %s)\n", spliced, errno, strerror(errno));
                //     std::unique_lock<std::mutex> locker(mutex);
                //     fds.erase(it.first);
                // } else {
                //     printf("spliced %d bytes\n", spliced);
                // }
                const ssize_t r = ::read(it.first, buf, sizeof(buf));
                //fprintf(stdout, "read!! %d\n", r);
                if (r > 0) {
                    ssize_t wpos = 0;
                    while (wpos < r) {
                        const ssize_t w = ::write(it.second, buf + wpos, r - wpos);
                        if (w >= 0) {
                            wpos += w;
                        } else {
                            fprintf(stderr, "splice write failed %d (%d %s)\n", it.first, errno, strerror(errno));
                            std::unique_lock<std::mutex> locker(mutex);
                            error(Splicer::ErrorTo, it.second, errno);
                            fds.erase(it.first);
                            break;
                        }
                    }
                } else {
                    if (it.second != STDOUT_FILENO)
                        ::close(it.second);
                    std::unique_lock<std::mutex> locker(mutex);
                    if (r < 0) {
                        fprintf(stderr, "splice read failed %d (%d %s)\n", it.first, errno, strerror(errno));
                        error(Splicer::ErrorFrom, it.first, errno);
                    } else {
                        assert(!r);
                        closed(it.first);
                    }
                    fds.erase(it.first);
                }
            }
        }
    }
}

void SplicerThread::splice(int from, int to)
{
    std::unique_lock<std::mutex> locker(mutex);
    fds[from] = to;
    for (;;) {
        const int w = ::write(msgPipe[1], "q", 1);
        assert(w);
        if (w > 0 || errno != EINTR)
            break;
    }
}

static SplicerThread* sThread = 0;

static void init()
{
    sThread = new SplicerThread;
    sThread->start();
}

static std::once_flag spliceFlag;

void Splicer::splice(int from, int to)
{
    std::call_once(spliceFlag, init);
    sThread->splice(from, to);
}

unsigned int Splicer::addCloseCallback(std::function<void(int)>&& cb)
{
    std::call_once(spliceFlag, init);
    return sThread->addCloseCallback(std::move(cb));
}

unsigned int Splicer::addErrorCallback(std::function<void(ErrorType, int, int)>&& cb)
{
    std::call_once(spliceFlag, init);
    return sThread->addErrorCallback(std::move(cb));
}

void Splicer::removeCloseCallback(unsigned int id)
{
    std::call_once(spliceFlag, init);
    sThread->removeCloseCallback(id);
}

void Splicer::removeErrorCallback(unsigned int id)
{
    std::call_once(spliceFlag, init);
    sThread->removeErrorCallback(id);
}
