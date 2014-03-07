#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <rct/Path.h>
#include <rct/String.h>
#include <rct/Value.h>

class InterpreterData;

class Interpreter
{
public:
    Interpreter();
    ~Interpreter();

    Value load(const Path &path);
    Value eval(const String &script, const String &name = String());

    Value call(const String &object, const String &function, const List<Value> &args, bool *ok = 0);
private:
    InterpreterData* mData;
};

#endif
