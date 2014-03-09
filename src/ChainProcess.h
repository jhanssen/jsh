#ifndef CHAINPROCESS_H
#define CHAINPROCESS_H

#include "Chain.h"
#include <rct/Process.h>

class ChainProcess : public Chain, private Process
{
public:
    typedef std::shared_ptr<ChainProcess> SharedPtr;
    typedef std::weak_ptr<ChainProcess> WeakPtr;

    ChainProcess();
    virtual ~ChainProcess();

    using Process::start;

protected:
    virtual void init(Chain* previous);

private:
    void processStdout(Process*);
    void processStderr(Process*);
    void processFinished(Process*);

    void previousStdout(String&& stdout);
    void previousStderr(String&& stderr);
    void previousClosed(Chain* chain);
};

#endif
