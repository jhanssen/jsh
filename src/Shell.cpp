#include "Shell.h"
#include "NodeJS.h"
#include "ChainProcess.h"
#include "Input.h"
#include "Util.h"
#include <rct/EventLoop.h>

extern char **environ;

Shell* Shell::sInstance;

int Shell::exec()
{
    for (int i=0; environ[i]; ++i) {
        char *eq = strchr(environ[i], '=');
        if (eq) {
            mEnviron[String(environ[i], eq)] = eq + 1;
        } else {
            mEnviron[environ[i]] = String();
        }
    }

    mEventLoop = std::make_shared<EventLoop>();
    mEventLoop->init(EventLoop::MainEventLoop);

    mInput = std::make_shared<Input>(mArgc, mArgv);
    mInput->start();

    const Path home = util::homeDirectory();
    const Path rcFile = home + "/.jshrc.js";
    const Path socketFile = home + "/.jsh-socket";

    mNodeJS = std::make_shared<NodeJS>();
    mNodeJS->init(rcFile, socketFile);

    mEventLoop->exec();
    mInput->join();

    return 0;
}

static const char *typeNames[] = {
    "Javascript",
    "Command",
    "Pipe",
    "Operator"
};

const char * Shell::Token::typeName(Type type)
{
    return typeNames[type];
}
