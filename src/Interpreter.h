#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <rct/Path.h>
#include <rct/String.h>
#include <v8.h>

class Interpreter
{
public:
    Interpreter();
    ~Interpreter();

    void load(const Path& path);
    void eval(const String& script, const String& name = String());

private:
    v8::UniquePersistent<v8::Context> mContext;
};

#endif
