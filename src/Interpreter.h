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

    Value load(const Path& path);
    Value eval(const String& script, const String& name = String());

private:
    InterpreterData* mData;
};

#endif
