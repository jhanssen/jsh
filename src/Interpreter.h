#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <rct/Path.h>
#include <rct/String.h>
#include <rct/Value.h>
#include <v8.h>

class Interpreter
{
public:
    Interpreter();
    ~Interpreter();

    Value load(const Path& path);
    Value eval(const String& script, const String& name = String());

private:
    v8::Handle<v8::String> toJSON(v8::Handle<v8::Value> object);
    Value v8ValueToValue(const v8::Handle<v8::Value>& value);

private:
    v8::UniquePersistent<v8::Context> mContext;
};

#endif
