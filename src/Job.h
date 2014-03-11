#ifndef JOB_H
#define JOB_H

#include <rct/Hash.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <rct/Thread.h>
#include <sys/types.h>
#include <mutex>

class Job : private Thread
{
public:
    Job(int stdout);
    ~Job();

    enum AddFlags {
        None = 0x0,
        Last = 0x1
    };

    bool addProcess(const Path& command, int flags = None,
                    const List<String>& arguments = List<String>(),
                    const List<String>& environ = List<String>());
    bool addPipe(int& stdout, int flags = None);

    void exec();

private:
    virtual void run();

private:
    struct Entry
    {
        pid_t pid;
    };
    int mStdout, mInPipe;
    Set<Entry> mEntries;
    std::mutex mMutex;
};

#endif
