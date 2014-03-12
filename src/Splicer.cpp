#include "Splicer.h"
#include <rct/Hash.h>
#include <rct/List.h>
#include <rct/Thread.h>
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
    virtual void run();

    void splice(int from, int to);

private:
    std::mutex mutex;
    int msgPipe[2];
    Hash<int, int> fds;
};

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
            fprintf(stderr, "splice thread select failed\n");
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
                if (r > 0) {
                    ssize_t wpos = 0;
                    while (wpos < r) {
                        const ssize_t w = ::write(it.second, buf + wpos, sizeof(buf) - wpos);
                        if (w >= 0) {
                            wpos += w;
                        } else {
                            fprintf(stderr, "splice write failed\n");
                            fds.erase(it.first);
                            break;
                        }
                    }
                } else {
                    fprintf(stderr, "splice read failed\n");
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
