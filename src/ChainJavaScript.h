#ifndef CHAINPROCESS_H
#define CHAINPROCESS_H

#include "Chain.h"
#include "Interpreter.h"
#include <rct/Process.h>

class ChainJavaScript : public Chain
{
public:
    typedef std::shared_ptr<ChainJavaScript> SharedPtr;
    typedef std::weak_ptr<ChainJavaScript> WeakPtr;

    ChainJavaScript(Interpreter::InterpreterScope&& scope);
    virtual ~ChainJavaScript();

    void exec() { mScope.exec(); }

protected:
    virtual void init(Chain* previous);

private:
    void jsStdout(String&& out);
    void jsStderr(String&& err);
    void jsClosed();

    void previousStdout(String&& stdout);
    void previousStderr(String&& stderr);
    void previousClosed(Chain* chain);

private:
    Interpreter::InterpreterScope mScope;
};

#endif
