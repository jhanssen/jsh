#ifndef CHAINPROCESS_H
#define CHAINPROCESS_H

#include "Chain.h"
#include <rct/Process.h>

class ChainProcess : public Chain
{
public:
    ChainProcess(Process* process);
    virtual ~ChainProcess();

protected:
    virtual void init(Chain* previous);

private:
    void processStdout(Process*);
    void processStderr(Process*);
    void processFinished(Process*);

    void previousStdout(String&& stdout);
    void previousStderr(String&& stderr);
    void previousClosed(Chain* chain);

private:
    Process* mProcess;
};

#endif
