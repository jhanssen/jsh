#include "Shell.h"
#include "NodeJS.h"
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

    const Path home = util::homeDirectory();
    Path socketFile = home + "/.jsh-socket";
    unsigned int nodeFlags = NodeJS::Autostart;
    for (int i=1; i<mArgc; ++i) {
        const String arg(mArgv[i]);
        if (arg == "-s" || arg == "--socket-file") {
            if (i + 1 == mArgc) {
                error("%s requires an argument", arg.constData());
                return 1;
            }
            socketFile = mArgv[++i];
        } else if (arg.startsWith("-s")) {
            socketFile = arg.mid(2);
        } else if (arg.size() > 14 && arg.startsWith("--socket-file=")) {
            socketFile = arg.mid(14);
        } else if (arg == "-a" || arg == "--no-autostart-node-js") {
            nodeFlags &= ~NodeJS::Autostart;
        } else {
            error("Unknown argument: %s", arg.constData());
        }
    }

    mEventLoop = std::make_shared<EventLoop>();
    mEventLoop->init(EventLoop::MainEventLoop);

    mInput = std::make_shared<Input>(mArgc, mArgv);
    mInput->start();

    mNodeJS = std::make_shared<NodeJS>();
    mNodeJS->init(socketFile, nodeFlags);

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
