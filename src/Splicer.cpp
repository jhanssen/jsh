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

class SplicerThread : public Thread
{
public:
    virtual void run();

    void splice(int from, int to, Splicer::ClosedCallback callback, void* userdata);

private:
    std::mutex mutex;
    int msgPipe[2];

    struct Data
    {
        int to;
        Splicer::ClosedCallback callback;
        void* userdata;
    };
    Hash<int, Data> fds;
};

void SplicerThread::run()
{
    fd_set rd;
    int max;
    Hash<int, Data> local;
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

        for (const auto& it : local) {
            if (FD_ISSET(it.first, &rd)) {
                if (::splice(it.first, 0, it.second.to, 0, 32768, 0) <= 0) {
                    if (it.second.callback)
                        it.second.callback(it.second.userdata, it.first, it.second.to);
                    std::unique_lock<std::mutex> locker(mutex);
                    fds.erase(it.first);
                }
            }
        }
    }
}

void SplicerThread::splice(int from, int to, Splicer::ClosedCallback callback, void* userdata)
{
    std::unique_lock<std::mutex> locker(mutex);
    fds[from] = { to, callback, userdata };
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

void Splicer::splice(int from, int to, ClosedCallback callback, void* userdata)
{
    std::call_once(spliceFlag, init);
    sThread->splice(from, to, callback, userdata);
}
