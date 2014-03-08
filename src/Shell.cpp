#include "Shell.h"
#include "Interpreter.h"
#include "ChainProcess.h"
#include "Input.h"
#include "Util.h"
#include <rct/EventLoop.h>

int Shell::exec()
{
    mEventLoop = std::make_shared<EventLoop>();
    mEventLoop->init(EventLoop::MainEventLoop);

    mInput = new Input(this, mArgc, mArgv);
    mInput->start();

    const Path home = util::homeDirectory();
    const Path elFile = home + "/.jshel";
    const Path rcFile = home + "/.jshrc.js";
    const Path histFile = home + "/.jshist";

    Interpreter interpreter;
    mInterpreter = &interpreter;
    interpreter.load(rcFile);

    mInterpreter = 0;

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
