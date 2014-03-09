#ifndef CHAINJAVASCRIPT_H
#define CHAINJAVASCRIPT_H

#include "Chain.h"
#include "Interpreter.h"
#include <rct/Process.h>

class ChainJavaScript : public Chain
{
public:
    typedef std::shared_ptr<ChainJavaScript> SharedPtr;
    typedef std::weak_ptr<ChainJavaScript> WeakPtr;

    ChainJavaScript(Interpreter::InterpreterScope::SharedPtr scope);
    virtual ~ChainJavaScript();

    bool parse() { return mScope->parse(); }

protected:
    virtual void init(Chain* previous);
    virtual void notifyIsFirst() { mScope->notify(Interpreter::InterpreterScope::StdInClosed); }
    virtual void exec() { return mScope->exec(); }

private:
    void jsStdout(String&& out);
    void jsStderr(String&& err);
    void jsClosed();

    void previousStdout(String&& stdout);
    void previousStderr(String&& stderr);
    void previousClosed(Chain* chain);

private:
    Interpreter::InterpreterScope::SharedPtr mScope;
};

#endif
