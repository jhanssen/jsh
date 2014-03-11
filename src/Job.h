#ifndef JOB_H
#define JOB_H

#include "NodeJS.h"
#include <rct/Hash.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <rct/Thread.h>
#include <sys/types.h>
#include <memory>
#include <mutex>
#include <condition_variable>

class Job
{
public:
    typedef std::shared_ptr<Job> SharedPtr;
    typedef std::weak_ptr<Job> WeakPtr;

    Job(int stdout);
    ~Job();

    enum AddFlags {
        None = 0x0,
        Last = 0x1
    };

    bool addProcess(const Path& command, const List<String>& arguments,
                    const List<String>& environ, int flags = None);
    bool addNodeJS(const String& script, int fd, int flags = None);

    void wait();

private:
    static void closedCallback(void* userdata, int from, int to);

private:
    struct Entry
    {
        enum { Process, Node } type;
        union {
            pid_t pid;
            int fd;
        };
    };

    List<Entry>::iterator wait(List<Entry>::iterator entry);

    int mStdout, mInPipe;
    List<Entry> mEntries;

    Set<pid_t> processes;

    std::mutex mMutex;
    std::condition_variable mCond;
    int mPendingJSJobs;
};

#endif
