#include "ProcessChain.h"
#include <set>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

using namespace v8;

Persistent<FunctionTemplate> ProcessChain::constructor;

void ProcessChain::init(Handle<Object> target)
{
    HandleScope scope;

    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    Local<String> name = String::NewSymbol("ProcessChain");

    constructor = Persistent<FunctionTemplate>::New(tpl);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(name);

    NODE_SET_PROTOTYPE_METHOD(constructor, "chain", chain);
    NODE_SET_PROTOTYPE_METHOD(constructor, "exec", exec);

    target->Set(name, constructor->GetFunction());
}

ProcessChain::ProcessChain()
    : ObjectWrap()
{
}

ProcessChain::~ProcessChain()
{
}

Handle<Value> ProcessChain::New(const Arguments& args)
{
    HandleScope scope;

    if (!args.IsConstructCall()) {
        return ThrowException(Exception::TypeError(String::New("Use the new operator to create instances of this object.")));
    }

    if (args.Length()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain takes no arguments")));
    }

    ProcessChain* obj = new ProcessChain;
    obj->Wrap(args.This());

    return args.This();
}

Handle<Value> ProcessChain::exec(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.exec takes a callback argument")));
    }

    Handle<Value> execArg = args[0];
    if (execArg.IsEmpty() || !execArg->IsFunction()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.exec takes a callback argument")));
    }

    Handle<Function> callback = Handle<Function>::Cast(execArg);

    ProcessChain* obj = ObjectWrap::Unwrap<ProcessChain>(args.This());

    // set up an stdout pipe
    int finalPipe[2];
    if (::pipe(finalPipe)) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.exec pipe 1 failed")));
    }

    std::set<pid_t> pids;
    int stdoutPipe[2];
    int stdinFd = STDIN_FILENO;
    auto entry = obj->mEntries.cbegin();
    const auto end = obj->mEntries.cend();
    while (entry != end) {
        const bool last = (entry + 1 == end);
        if (!last) {
            ::pipe(stdoutPipe);
        } else {
            stdoutPipe[0] = finalPipe[0];
            stdoutPipe[1] = finalPipe[1];
        }

        const pid_t pid = ::fork();
        switch (pid) {
        case -1:
            // something horrible has happened
            return ThrowException(Exception::TypeError(String::New("ProcessChain.exec fork failed")));
        case 0: {
            // child
            const size_t asz = entry->arguments.size();
            const char* args[asz + 2];
            args[0] = entry->program.c_str();
            args[asz + 1] = 0;
            for (size_t i = 1; i <= asz; ++i) {
                args[i] = entry->arguments[i - 1].c_str();
            }

            // dups
            if (stdinFd != STDIN_FILENO) {
                ::dup2(stdinFd, STDIN_FILENO);
                ::close(stdinFd);
            }
            ::close(stdoutPipe[0]);
            ::dup2(stdoutPipe[1], STDOUT_FILENO);
            ::close(stdoutPipe[1]);

            ::execv(entry->program.c_str(), const_cast<char* const*>(args));
            _exit(1);
            break; }
        default: {
            // parent
            ::close(stdoutPipe[1]);
            stdinFd = stdoutPipe[0];

            pids.insert(pid);

            break; }
        }
        ++entry;
    }

    if (pids.empty()) {
        // no processes started
        return Undefined();
    }

    const int outfd = finalPipe[0];

    // select
    fd_set rd;
    for (;;) {
        FD_ZERO(&rd);
        FD_SET(outfd, &rd);
        int s;
        eintrwrap(s, ::select(outfd + 1, &rd, 0, 0, 0));
        if (s <= 0) {
            // bad
            return ThrowException(Exception::TypeError(String::New("ProcessChain.exec select failed")));
        }
        if (FD_ISSET(outfd, &rd)) {
            char buf[8192];
            int r;
            eintrwrap(r, ::read(outfd, buf, sizeof(buf)));
            if (r < 0) {
                // bad
                return ThrowException(Exception::TypeError(String::New("ProcessChain.exec read failed")));
            } else if (r == 0) {
                // done
                break;
            } else {
                // got data
                Handle<Value> str = String::New(buf, r);
                callback->Call(Context::GetCurrent()->Global(), 1, &str);
            }
        }
    }

    // wait for pids and join
    for (;;) {
        int status;
        pid_t pid;
        eintrwrap(pid, waitpid(WAIT_ANY, &status, WUNTRACED));
        if (pid > 0) {
            pids.erase(pid);
            if (pids.empty())
                break;
        } else {
            return ThrowException(Exception::TypeError(String::New("ProcessChain.exec waitpid failed")));
        }
    }

    return Undefined();
}

Handle<Value> ProcessChain::chain(const Arguments& args)
{
    HandleScope scope;

    ProcessChain* obj = ObjectWrap::Unwrap<ProcessChain>(args.This());

    if (args.Length() != 1 || args[0].IsEmpty() || !args[0]->IsObject()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() requires an object argument.")));
    }
    Handle<Object> arg = Handle<Object>::Cast(args[0]);
    Handle<Value> program = arg->Get(String::New("program"));
    Handle<Value> arguments = arg->Get(String::New("arguments"));
    if (program.IsEmpty() || !program->IsString()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() requires a program argument.")));
    }
    if (arguments.IsEmpty() || !arguments->IsArray()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() requires an arguments argument.")));
    }

    obj->mEntries.push_back(Entry());
    Entry& entry = obj->mEntries.back();

    {
        String::Utf8Value prog(program);
        if (prog.length() > 0)
            entry.program = *prog;
    }
    Handle<Array> argarray = Handle<Array>::Cast(arguments);
    for (uint32_t i = 0; i < argarray->Length(); ++i) {
        Handle<Value> arg = argarray->Get(i);
        if (arg.IsEmpty() || !arg->IsString()) {
            return ThrowException(Exception::TypeError(String::New("All arguments in ProcessChain.chain() need to be strings.")));
        }
        String::Utf8Value a(arg);
        if (a.length() > 0)
            entry.arguments.push_back(*a);
    }

    //return scope.Close(Integer::New(value));
    return args.Holder();
}

void RegisterModule(Handle<Object> target)
{
    ProcessChain::init(target);
}

NODE_MODULE(ProcessChain, RegisterModule);
