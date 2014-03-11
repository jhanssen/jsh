#ifndef SHELL_H
#define SHELL_H

#include <rct/String.h>
#include <rct/Hash.h>
#include <rct/EventLoop.h>
#include <functional>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <assert.h>

class NodeJS;
class Input;
class Shell
{
public:
    Shell(int argc, char** argv)
        : mArgc(argc), mArgv(argv)
    {
        assert(!sInstance);
        sInstance = this;
    }
    ~Shell() { assert(sInstance == this); sInstance = 0; }

    int exec();

    template<typename T>
    T runAndWait(std::function<T()>&& func);

    std::shared_ptr<NodeJS> nodeJS() { return mNodeJS; }

    static Shell* instance() { return sInstance; }

    Hash<String, String> environment() { std::unique_lock<std::mutex>(mMutex); return mEnviron; }
    String environment(const String &env) { std::unique_lock<std::mutex>(mMutex); return mEnviron.value(env);; }

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
        String raw;
    };

    std::mutex mMutex;

    int mArgc;
    char** mArgv;
    std::shared_ptr<Input> mInput;
    EventLoop::SharedPtr mEventLoop;
    Hash<String, String> mEnviron;
    std::shared_ptr<NodeJS> mNodeJS;

    static Shell* sInstance;

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
