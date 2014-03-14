#ifndef PROCESSCHAIN_HPP
#define PROCESSCHAIN_HPP

#include <node.h>
#include <string>
#include <vector>
#include <set>

class ProcessChain : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> target);

    struct Entry {
        std::string program, cwd;
        std::vector<std::string> arguments, environment;
    };

private:
    ProcessChain();
    ~ProcessChain();

    bool launch();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> chain(const v8::Arguments& args);
    static v8::Handle<v8::Value> write(const v8::Arguments& args);
    static v8::Handle<v8::Value> end(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> constructor;

    std::vector<Entry> mEntries;
    int mFinalPipe[2], mInPipe[2];
    std::set<pid_t> mPids;
    bool mLaunched;
};

#endif
