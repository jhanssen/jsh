#ifndef SHELL_H
#define SHELL_H

#include <rct/String.h>
#include <rct/Hash.h>
#include <rct/EventLoop.h>
#include <functional>
#include <mutex>
#include <memory>
#include <condition_variable>

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

    template<typename T>
    T runAndWait(std::function<T()>&& func);

    static std::shared_ptr<Interpreter> interpreter() { return sInterpreter; }

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
    std::shared_ptr<Input> mInput;
    EventLoop::SharedPtr mEventLoop;
    static std::shared_ptr<Interpreter> sInterpreter;

private:
    friend class Input;
};

template<typename T>
inline T Shell::runAndWait(std::function<T()>&& func)
{
    std::mutex mtx;
    std::condition_variable cond;
    bool done = false;
    T ret;
    EventLoop::mainEventLoop()->callLater([&]() {
            ret = func();
            std::unique_lock<std::mutex> lock(mtx);
            done = true;
            cond.notify_one();
        });
    std::unique_lock<std::mutex> lock(mtx);
    while (!done) {
        cond.wait(lock);
    }
    return ret;
}

#endif
