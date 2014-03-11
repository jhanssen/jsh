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
    bool addNode(const String& script, const String& socketFile, int flags = None);

    void wait();

private:
    struct Entry
    {
        enum { Process, Node } type;
        pid_t pid;
        NodeJS::SharedPtr node;
    };

    List<Entry>::iterator wait(List<Entry>::iterator entry);

    int mStdout, mInPipe;
    List<Entry> mEntries;
};

#endif
