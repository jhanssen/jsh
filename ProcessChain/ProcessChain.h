#ifndef PROCESSCHAIN_HPP
#define PROCESSCHAIN_HPP

#include <node.h>
#include <string>
#include <vector>

class ProcessChain : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> target);

    struct Entry {
        std::string program;
        std::vector<std::string> arguments;
    };

private:
    ProcessChain();
    ~ProcessChain();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> chain(const v8::Arguments& args);
    static v8::Handle<v8::Value> exec(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> constructor;

    std::vector<Entry> mEntries;
};

#endif
