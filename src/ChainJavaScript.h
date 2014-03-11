#ifndef CHAINJAVASCRIPT_H
#define CHAINJAVASCRIPT_H

#include "Chain.h"
#include "NodeJS.h"
#include <rct/Process.h>

class ChainJavaScript : public Chain
{
public:
    typedef std::shared_ptr<ChainJavaScript> SharedPtr;
    typedef std::weak_ptr<ChainJavaScript> WeakPtr;

    ChainJavaScript(const String &code);
    virtual ~ChainJavaScript();

protected:
    virtual void init(Chain* previous);
    virtual void notifyIsFirst();
    virtual void exec();

private:
    void jsStdout(String&& out);
    void jsStderr(String&& err);
    void jsClosed();

    void previousStdout(String&& stdout);
    void previousStderr(String&& stderr);
    void previousClosed(Chain* chain);

private:
    const String mScript;
};

#endif
