#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <rct/Path.h>
#include <rct/String.h>
#include <rct/Value.h>
#include <rct/SignalSlot.h>
#include <memory>

class InterpreterData;
class InterpreterScopeData;

class Interpreter : public std::enable_shared_from_this<Interpreter>
{
public:
    typedef std::shared_ptr<Interpreter> SharedPtr;
    typedef std::weak_ptr<Interpreter> WeakPtr;

    Interpreter();
    ~Interpreter();

    Value load(const Path &path);
    Value eval(const String &script, const String &name = String());

    Value call(const String &object, const String &function, const List<Value> &args, bool *ok = 0);
    Value call(const String &function, const List<Value> &args, bool *ok = 0) { return call(String(), function, args, ok); }

    class InterpreterScope
    {
    public:
        InterpreterScope(InterpreterScope&& other);
        InterpreterScope& operator=(InterpreterScope&& other);
        ~InterpreterScope();

        enum NotifyType {
            StdInData,
            StdInClosed
        };
        void notify(NotifyType type, const String& data = String());

        Signal<std::function<void(const String& data)> >& stdout() { return mStdin; }
        Signal<std::function<void(const String& data)> >& stderr() { return mStdout; }
        Signal<std::function<void()> >& closed() { return mClosed; }

        void exec();

    private:
        InterpreterScope(const Interpreter::SharedPtr& interpreter, const String& script, const String& name);
        InterpreterScope(const InterpreterScope&) = delete;
        InterpreterScope& operator=(const InterpreterScope&) = delete;

        Signal<std::function<void(const String&)> > mStdin, mStdout;
        Signal<std::function<void()> > mClosed;

    private:
        InterpreterScopeData* mData;

        friend class Interpreter;
    };

    InterpreterScope&& createScope(const String& script, const String& name = String());

private:
    InterpreterData* mData;

    friend class InterpreterScopeData;
};

#endif
