#include "ProcessChain.h"
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
    NODE_SET_PROTOTYPE_METHOD(constructor, "write", write);
    NODE_SET_PROTOTYPE_METHOD(constructor, "end", end);

    target->Set(name, constructor->GetFunction());
}

ProcessChain::ProcessChain()
    : ObjectWrap(), mLaunched(false)
{
    mFinalPipe[0] = mFinalPipe[1] -1;
    mInPipe[0] = mInPipe[1] -1;
}

static inline void closePipe(int* pipe)
{
    if (*pipe != -1)
        ::close(*pipe);
    if (*(pipe + 1) != -1)
        ::close(*(pipe + 1));
}

ProcessChain::~ProcessChain()
{
    closePipe(mFinalPipe);
    closePipe(mInPipe);
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

bool ProcessChain::launch()
{
    if (mLaunched)
        return true;

    HandleScope scope;

    if (::pipe(mFinalPipe)) {
        return false;
    }
    if (::pipe(mInPipe)) {
        return false;
    }

    int stdoutPipe[2];
    int stdinFd = mInPipe[0];
    auto entry = mEntries.cbegin();
    const auto end = mEntries.cend();
    while (entry != end) {
        const bool last = (entry + 1 == end);
        if (!last) {
            ::pipe(stdoutPipe);
        } else {
            stdoutPipe[0] = mFinalPipe[0];
            stdoutPipe[1] = mFinalPipe[1];
        }

        const pid_t pid = ::fork();
        switch (pid) {
        case -1:
            // something horrible has happened
            return false;
        case 0: {
            // child
            const size_t asz = entry->arguments.size();
            const char* args[asz + 2];
            args[0] = entry->program.c_str();
            args[asz + 1] = 0;
            for (size_t i = 1; i <= asz; ++i) {
                args[i] = entry->arguments[i - 1].c_str();
            }

            const size_t esz = entry->environment.size();
            const char* env[esz + 1];
            env[esz] = 0;
            for (size_t i = 0; i < esz; ++i) {
                env[i] = entry->environment[i].c_str();
            }

            // dups
            ::dup2(stdinFd, STDIN_FILENO);
            if (stdinFd != mInPipe[0]) {
                ::close(mInPipe[0]);
            }
            ::close(mInPipe[1]);
            ::close(stdinFd);

            ::close(stdoutPipe[0]);
            ::dup2(stdoutPipe[1], STDOUT_FILENO);
            ::close(stdoutPipe[1]);

            if (!entry->cwd.empty()) {
                ::chdir(entry->cwd.c_str());
            }

            if (entry->environment.empty())
                ::execv(entry->program.c_str(), const_cast<char* const*>(args));
            else
                ::execve(entry->program.c_str(), const_cast<char* const*>(args), const_cast<char* const*>(env));
            _exit(1);
            break; }
        default: {
            // parent
            ::close(stdoutPipe[1]);
            stdinFd = stdoutPipe[0];

            mPids.insert(pid);

            break; }
        }
        ++entry;
    }

    ::close(mInPipe[0]);
    ::close(mFinalPipe[1]);
    mInPipe[0] = -1;
    mFinalPipe[1] = -1;
    mLaunched = true;
    return true;
}

Handle<Value> ProcessChain::write(const Arguments& args)
{
    HandleScope scope;

    ProcessChain* obj = ObjectWrap::Unwrap<ProcessChain>(args.This());

    if (args.Length() == 0) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.write requires at least one string argument.")));
    }

    if (!obj->mLaunched && !obj->launch()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.write launch failed.")));
    }

    for (int i = 0; i < args.Length(); ++i) {
        if (args[i].IsEmpty() || !args[i]->IsString()) {
            return ThrowException(Exception::TypeError(String::New("ProcessChain.write only takes string arguments.")));
        }
        String::Utf8Value val(args[i]);
        int rem = val.length();
        int pos = 0;
        while (rem) {
            const int w = ::write(obj->mInPipe[1], *val + pos, rem);
            if (w <= 0) {
                return ThrowException(Exception::TypeError(String::New("ProcessChain.write ::write failed.")));
            }
            pos += w;
            rem -= w;
        }
    }

    return args.Holder();
}

Handle<Value> ProcessChain::end(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.end takes a callback argument")));
    }
    if (args[0].IsEmpty() || !args[0]->IsFunction()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.end takes a callback argument")));
    }
    Handle<Function> callback = Handle<Function>::Cast(args[0]);

    ProcessChain* obj = ObjectWrap::Unwrap<ProcessChain>(args.This());
    if (!obj->mLaunched && !obj->launch()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.write launch failed.")));
    }
    obj->mLaunched = false;
    ::close(obj->mInPipe[1]);
    obj->mInPipe[1] = -1;

    if (obj->mPids.empty()) {
        // no processes started
        return Undefined();
    }

    const int outfd = obj->mFinalPipe[0];

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

    ::close(obj->mFinalPipe[0]);
    obj->mFinalPipe[0] = -1;

    // wait for pids and join
    for (;;) {
        int status;
        pid_t pid;
        eintrwrap(pid, waitpid(WAIT_ANY, &status, WUNTRACED));
        if (pid > 0) {
            obj->mPids.erase(pid);
            if (obj->mPids.empty())
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
    Handle<Value> environment = arg->Get(String::New("environment"));
    Handle<Value> cwd = arg->Get(String::New("cwd"));
    if (program.IsEmpty() || !program->IsString()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() requires a program argument.")));
    }
    if (!arguments.IsEmpty() && !arguments->IsUndefined() && !arguments->IsArray()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() arguments needs to be an array")));
    }
    if (!environment.IsEmpty() && !environment->IsUndefined() && !environment->IsArray()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() environment needs to be an array")));
    }
    if (!cwd.IsEmpty() && !cwd->IsUndefined() && !cwd->IsString()) {
        return ThrowException(Exception::TypeError(String::New("ProcessChain.chain() cwd needs to be a string")));
    }

    obj->mEntries.push_back(Entry());
    Entry& entry = obj->mEntries.back();

    {
        String::Utf8Value prog(program);
        if (prog.length() > 0)
            entry.program = *prog;
    }
    if (!cwd.IsEmpty() && !cwd->IsUndefined() && !cwd.IsEmpty()) {
        String::Utf8Value dir(cwd);
        if (dir.length() > 0)
            entry.cwd = *dir;
    }
    if (!arguments.IsEmpty() && arguments->IsArray()) {
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
    }
    if (!environment.IsEmpty() && environment->IsArray()) {
        Handle<Array> envarray = Handle<Array>::Cast(environment);
        for (uint32_t i = 0; i < envarray->Length(); ++i) {
            Handle<Value> arg = envarray->Get(i);
            if (arg.IsEmpty() || !arg->IsString()) {
                return ThrowException(Exception::TypeError(String::New("All environment variables in ProcessChain.chain() need to be strings.")));
            }
            String::Utf8Value a(arg);
            if (a.length() > 0)
                entry.environment.push_back(*a);
        }
    }

    //return scope.Close(Integer::New(value));
    return args.Holder();
}

void RegisterModule(Handle<Object> target)
{
    ProcessChain::init(target);
}

NODE_MODULE(ProcessChain, RegisterModule);
