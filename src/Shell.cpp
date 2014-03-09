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

    mInput = std::make_shared<Input>(this, mArgc, mArgv);
    mInput->start();

    const Path home = util::homeDirectory();
    const Path rcFile = home + "/.jshrc.js";

    mInterpreter = std::make_shared<Interpreter>();
    mInterpreter->load(rcFile);

    mEventLoop->exec();
    mInput->join();

    mInterpreter.reset();

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
