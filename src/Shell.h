#ifndef SHELL_H
#define SHELL_H

#include <rct/String.h>
#include <rct/Hash.h>
#include <rct/EventLoop.h>

class Interpreter;
class Input;
class Shell
{
public:
    Shell(int argc, char** argv)
        : mArgc(argc), mArgv(argv)
    {
    }

    int exec();
    Interpreter* interpreter() const { return mInterpreter; }

private:
    struct Token {
        enum Type {
            Javascript,
            Command,
            Pipe,
            Operator
        } type;
        static const char *typeName(Type type);

        String string;
        List<String> args;
    };

    int mArgc;
    char** mArgv;
    Interpreter *mInterpreter;
    Input* mInput;
    EventLoop::SharedPtr mEventLoop;

private:
    friend class Input;
};

#endif
