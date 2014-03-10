#include "Interpreter.h"
#include "Util.h"
#include <mutex>
#include <v8.h>

static inline const char* ToCString(const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

class InterpreterData
{
public:
    v8::Handle<v8::String> toJSON(v8::Handle<v8::Value> object, bool pretty = false);
    v8::Handle<v8::Value> loadJSModule(v8::Isolate* isolate, const char* name);
    v8::Handle<v8::Value> loadNativeModule(v8::Isolate* isolate, const char* name);
    Value v8ValueToValue(const v8::Handle<v8::Value>& value);
    v8::Handle<v8::Value> valueToV8Value(const Value& value);

    v8::UniquePersistent<v8::Context> context;
};

class InterpreterScopeData
{
public:
    InterpreterScopeData(const Interpreter::SharedPtr& inter, Interpreter::InterpreterScope* sco, const String& scr, const String& nam)
        : interpreter(inter), scope(sco), source(scr), name(nam)
    {
    }
    ~InterpreterScopeData();

    void exec();
    bool parse(const String& script, const String& name);
    void sendPendingStdin(v8::Isolate* isolate);

    Interpreter::SharedPtr interpreter;
    Interpreter::InterpreterScope* scope;

    struct Listener
    {
        v8::UniquePersistent<v8::Value> recv;
        v8::UniquePersistent<v8::Function> func;
    };
    List<Listener*> dataListeners, closeListeners;
    v8::UniquePersistent<v8::Script> script;
    String pendingStdin;

    String source, name;
};

static inline String Print(const v8::FunctionCallbackInfo<v8::Value>& args, bool newline = false)
{
    bool first = true;
    v8::Isolate* isolate = args.GetIsolate();
    InterpreterData* interpreter = static_cast<InterpreterData*>(isolate->GetData(0));
    String out;
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope(isolate);
        if (first) {
            first = false;
        } else {
            out += " ";
        }
        if (args[i]->IsObject()) {
            v8::String::Utf8Value str(interpreter->toJSON(args[i], true));
            const char* cstr = ToCString(str);
            out += cstr;
        } else {
            v8::String::Utf8Value str(args[i]);
            const char* cstr = ToCString(str);
            out += cstr;
        }
    }
    if (newline)
        out += "\n";
    return std::move(out);
}

static void PrintStdout(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    const String& out = Print(args);
    if (!out.isEmpty()) {
        fprintf(stdout, "%s\n", out.constData());
    }
}

static void PrintStderr(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    const String& err = Print(args);
    if (!err.isEmpty()) {
        fprintf(stdout, "%s\n", err.constData());
    }
}

static inline InterpreterScopeData* scopeDataFromObject(v8::Isolate* isolate, const v8::Handle<v8::Object>& holder)
{
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> data = holder->GetHiddenValue(v8::String::NewFromUtf8(isolate, "jsh::scope"));
    if (!data.IsEmpty() && data->IsExternal()) {
        v8::Local<v8::External> ext = v8::Local<v8::External>::Cast(data);
        return static_cast<InterpreterScopeData*>(ext->Value());
    }
    return 0;
}

static void EmitStdout(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    String out = Print(args, true);
    if (!out.isEmpty()) {
        v8::Isolate* isolate = args.GetIsolate();
        InterpreterScopeData* scope = scopeDataFromObject(isolate, args.Holder());
        assert(scope);
        scope->scope->stdout()(std::move(out));
    }
}

static void EmitStderr(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    String err = Print(args, true);
    if (!err.isEmpty()) {
        fprintf(stdout, "%s\n", err.constData());
        v8::Isolate* isolate = args.GetIsolate();
        InterpreterScopeData* scope = scopeDataFromObject(isolate, args.Holder());
        assert(scope);
        scope->scope->stderr()(std::move(err));
    }
}

static void StdinOn(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    if (args.Length() >= 2) {
        v8::Local<v8::Value> type = args[0];
        v8::Local<v8::Value> func = args[1];
        if (type->IsString() && func->IsFunction()) {
            InterpreterScopeData* scope = scopeDataFromObject(isolate, args.Holder());
            assert(scope);
            InterpreterScopeData::Listener* listener = new InterpreterScopeData::Listener;
            listener->recv = v8::UniquePersistent<v8::Value>(isolate, args.Callee());
            listener->func = v8::UniquePersistent<v8::Function>(isolate, v8::Local<v8::Function>::Cast(func));
            const v8::String::Utf8Value str(type);
            if (!strcmp(ToCString(str), "data")) {
                scope->dataListeners.append(listener);
            } else if (!strcmp(ToCString(str), "close")) {
                scope->closeListeners.append(listener);
            } else {
                isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Unrecognized type to stdin.on"));
                delete listener;
                return;
            }
            args.GetReturnValue().Set(args.Holder());
        } else {
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Invalid arguments to stdin.on"));
        }
    } else {
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "stdin.on needs at least two arguments"));
    }
}

v8::Handle<v8::Value> InterpreterData::loadJSModule(v8::Isolate* isolate, const char* name)
{
    v8::EscapableHandleScope handle_scope(isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
    Path file = util::findFile("~/.jshmodules", name);
    switch (file.type()) {
    case Path::File:
        break;
    case Path::Directory:
        file += "/module.js";
        break;
    default:
        break;
    }
    if (!file.isFile())
        return handle_scope.Escape(v8::Local<v8::Value>());

    v8::Context::Scope contextScope(ctx);

    v8::Handle<v8::String> fileName = v8::String::NewFromUtf8(isolate, name);
    v8::Handle<v8::String> source = v8::String::NewFromUtf8(isolate, file.readAll().constData());

    v8::TryCatch try_catch;
    v8::Handle<v8::Script> script = v8::Script::Compile(source, fileName);
    if (script.IsEmpty()) {
        error() << "script" << name << "in" << file << "didn't compile";
        return handle_scope.Escape(v8::Local<v8::Value>());
    }

    const v8::Local<v8::Value> result = script->Run();
    if (try_catch.HasCaught()) {
        const v8::Handle<v8::Message> msg = try_catch.Message();
        {
            const v8::String::Utf8Value str(msg->Get());
            error() << ToCString(str);
        }
        {
            const v8::String::Utf8Value str(msg->GetScriptResourceName());
            error() << String::format<64>("At %s:%d", ToCString(str), msg->GetLineNumber());
        }
        return handle_scope.Escape(v8::Local<v8::Value>());
    }
    return handle_scope.Escape(result);
}

v8::Handle<v8::Value> InterpreterData::loadNativeModule(v8::Isolate* isolate, const char* name)
{
    v8::EscapableHandleScope handle_scope(isolate);
    return handle_scope.Escape(v8::Local<v8::Value>());
}

static void Require(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    InterpreterData* interpreter = static_cast<InterpreterData*>(isolate->GetData(0));
    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::Value> ret;
    if (args.Length() == 1 && args[0]->IsString()) {
        v8::String::Utf8Value str(args[0]);
        ret = interpreter->loadJSModule(isolate, ToCString(str));
        if (!ret.IsEmpty()) {
            args.GetReturnValue().Set(ret);
            return;
        }
        ret = interpreter->loadNativeModule(isolate, ToCString(str));
        if (!ret.IsEmpty()) {
            args.GetReturnValue().Set(ret);
            return;
        }
    }
}

static v8::Handle<v8::Context> createContext(v8::Isolate* isolate)
{
    // Create a template for the global object.
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    // Bind the global 'print' function to the C++ Print callback.
    global->Set(v8::String::NewFromUtf8(isolate, "print"),
                v8::FunctionTemplate::New(isolate, PrintStdout));
    global->Set(v8::String::NewFromUtf8(isolate, "require"),
                v8::FunctionTemplate::New(isolate, Require));
    // Bind the global 'read' function to the C++ Read callback.
    // global->Set(v8::String::NewFromUtf8(isolate, "read"),
    //             v8::FunctionTemplate::New(isolate, Read));
    // Bind the global 'load' function to the C++ Load callback.
    // global->Set(v8::String::NewFromUtf8(isolate, "load"),
    //             v8::FunctionTemplate::New(isolate, Load));
    // Bind the 'quit' function
    // global->Set(v8::String::NewFromUtf8(isolate, "quit"),
    //             v8::FunctionTemplate::New(isolate, Quit));
    // Bind the 'version' function
    // global->Set(v8::String::NewFromUtf8(isolate, "version"),
    //             v8::FunctionTemplate::New(isolate, Version));

    return v8::Context::New(isolate, NULL, global);
}

Interpreter::Interpreter()
    : mData(new InterpreterData)
{
    static std::once_flag v8Once;
    std::call_once(v8Once, []() { v8::V8::InitializeICU(); });

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    isolate->SetData(0, mData);

    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::Context> context = createContext(isolate);
    if (context.IsEmpty()) {
        error() << "Error creating context";
        return;
    }
    mData->context.Reset(isolate, context);
}

Interpreter::~Interpreter()
{
    delete mData;
}

v8::Handle<v8::String> InterpreterData::toJSON(v8::Handle<v8::Value> object, bool pretty)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope handle_scope(isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
    v8::Handle<v8::Object> global = ctx->Global();

    v8::Handle<v8::Object> JSON = global->Get(v8::String::NewFromUtf8(isolate, "JSON"))->ToObject();
    v8::Handle<v8::Function> stringify = v8::Handle<v8::Function>::Cast(JSON->Get(v8::String::NewFromUtf8(isolate, "stringify")));

    if (pretty) {
        v8::Handle<v8::Value> args[3] = { object, v8::Null(isolate), v8::Integer::New(isolate, 4) };
        return handle_scope.Escape(stringify->Call(JSON, 3, args)->ToString());
    }
    return handle_scope.Escape(stringify->Call(JSON, 1, &object)->ToString());
}

static Value v8ValueToValue_helper(v8::Isolate *isolate, const v8::Handle<v8::Value> &value)
{
    if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
        return Value();
    } else if (value->IsTrue()) {
        return Value(true);
    } else if (value->IsFalse()) {
        return Value(false);
    } else if (value->IsInt32()) {
        return Value(value->ToInt32()->Value());
    } else if (value->IsNumber()) {
        return Value(value->ToNumber()->Value());
    } else if (value->IsString()) {
        const v8::String::Utf8Value str(value);
        return Value(ToCString(str));
    } else if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
        const int len = array->Length();
        List<Value> list(len);
        for (int i = 0; i < len; ++i)
            list[i] = v8ValueToValue_helper(isolate, array->Get(i));
        return list;
    } else if (value->IsObject()) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        v8::Local<v8::Array> properties = object->GetOwnPropertyNames();
        Map<String, Value> map;
        for(size_t i = 0; i < properties->Length(); ++i) {
            const v8::Handle<v8::Value> key = properties->Get(i);
            const v8::String::Utf8Value str(key);
            map[*str] = v8ValueToValue_helper(isolate, object->Get(key));
        }
        return map;
    } else {
        error() << "Unknown v8 value in Interpreter::v8ValueToValue";
    }
    return Value();
}

inline Value InterpreterData::v8ValueToValue(const v8::Handle<v8::Value>& value)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    return v8ValueToValue_helper(isolate, value);
}

static v8::Handle<v8::Value> valueToV8Value_helper(v8::Isolate *isolate, const Value& value)
{
    v8::Handle<v8::Value> result;
    switch (value.type()) {
    case Value::Type_String:
        result = v8::String::NewFromUtf8(isolate, value.toString().constData());
        break;
    case Value::Type_List: {
        const int sz = value.count();
        v8::Handle<v8::Array> array = v8::Array::New(isolate, sz);
        auto it = value.listBegin();
        for (int i=0; i<sz; ++i) {
            array->Set(i, valueToV8Value_helper(isolate, *it++));
        }
        result = array;
        break; }
    case Value::Type_Map: {
        v8::Handle<v8::Object> object = v8::Object::New(isolate);
        for (auto it = value.begin(); it != value.end(); ++it)
            object->Set(v8::String::NewFromUtf8(isolate, it->first.constData()), valueToV8Value_helper(isolate, it->second));
        result = object;
        break; }
    case Value::Type_Integer:
        result = v8::Integer::New(isolate, value.toInteger());
        break;
    case Value::Type_Double:
        result = v8::Number::New(isolate, value.toDouble());
        break;
    case Value::Type_Boolean:
        result = v8::Boolean::New(isolate, value.toBool());
        break;
    case Value::Type_Invalid:
        result = v8::Undefined(isolate);
        break;
    }
    return result;
}

inline v8::Handle<v8::Value> InterpreterData::valueToV8Value(const Value& value)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope handle_scope(isolate);
    v8::Local<v8::Value> local(valueToV8Value_helper(isolate, value));
    return handle_scope.Escape(local);
}

Value Interpreter::load(const Path& path)
{
    if (!path.isFile()) {
        error() << path << "is not a file";
        return Value();
    }
    return eval(path.readAll(), path.fileName());
}

Value Interpreter::eval(const String& data, const String& name, bool* ok)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, mData->context);
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::String> fileName = v8::String::NewFromUtf8(isolate, name.constData());
    v8::Handle<v8::String> source = v8::String::NewFromUtf8(isolate, data.constData());

    v8::TryCatch try_catch;
    v8::Handle<v8::Script> script = v8::Script::Compile(source, fileName);
    if (script.IsEmpty()) {
        if (ok)
            *ok = false;
        error() << "script" << name << "didn't compile";
        return Value();
    }

    const v8::Handle<v8::Value> result = script->Run();
    if (try_catch.HasCaught()) {
        if (ok)
            *ok = false;
        const v8::Handle<v8::Message> msg = try_catch.Message();
        {
            const v8::String::Utf8Value str(msg->Get());
            error() << ToCString(str);
        }
        {
            const v8::String::Utf8Value str(msg->GetScriptResourceName());
            error() << String::format<64>("At %s:%d", ToCString(str), msg->GetLineNumber());
        }
        return Value();
    }

    if (ok)
        *ok = true;
    return mData->v8ValueToValue(result);
}

Value Interpreter::call(const String &object, const String &function, const List<Value> &args, bool *ok)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, mData->context);
    v8::Context::Scope contextScope(ctx);
    v8::Handle<v8::Object> global = ctx->Global();
    v8::Handle<v8::Object> obj = global;

    v8::Handle<v8::Function> func;
    if (!object.isEmpty()) {
        obj = global->Get(v8::String::NewFromUtf8(isolate, object.constData()))->ToObject();
        if (obj.IsEmpty())
            func = v8::Handle<v8::Function>::Cast(obj->Get(v8::String::NewFromUtf8(isolate, function.constData())));
    } else {
        func = v8::Handle<v8::Function>::Cast(global->Get(v8::String::NewFromUtf8(isolate, function.constData())));
    }
    if (func.IsEmpty()) {
        if (!ok)
            *ok = false;
        return Value();
    }

    if (ok)
        *ok = true;

    std::vector<v8::Handle<v8::Value> > arguments(args.size());
    for (int i=0; i<args.size(); ++i) {
        arguments[i] = mData->valueToV8Value(args.at(i));
    }

    return mData->v8ValueToValue(func->Call(obj, arguments.size(), arguments.data()));
}

Interpreter::InterpreterScope::SharedPtr Interpreter::createScope(const String& script, const String& name)
{
    return InterpreterScope::SharedPtr(new InterpreterScope(shared_from_this(), script, name));
}

Interpreter::InterpreterScope::InterpreterScope(const Interpreter::SharedPtr& interpreter, const String& script, const String& name)
    : mData(new InterpreterScopeData(interpreter, this, script, name))
{
}

Interpreter::InterpreterScope::~InterpreterScope()
{
    delete mData;
}

bool Interpreter::InterpreterScope::parse()
{
    return mData->parse(mData->source, mData->name);
}

void Interpreter::InterpreterScope::exec()
{
    mData->exec();
}

void Interpreter::InterpreterScope::notify(NotifyType type, const String& data)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    switch (type) {
    case StdInClosed: {
        for (auto& func : mData->closeListeners) {
            v8::Local<v8::Function> f = v8::Local<v8::Function>::New(isolate, func->func);
            v8::Local<v8::Value> r = v8::Local<v8::Value>::New(isolate, func->recv);
            f->Call(r, 0, 0);
        }
#warning need to check if there are pending timers here before emitting this signal when the time comes
        mClosed();
        break; }
    case StdInData: {
        if (mData->dataListeners.isEmpty()) {
            mData->pendingStdin += data;
        } else {
            v8::Local<v8::Value> s = v8::String::NewFromUtf8(isolate, data.constData());
            for (auto& func : mData->dataListeners) {
                v8::Local<v8::Function> f = v8::Local<v8::Function>::New(isolate, func->func);
                v8::Local<v8::Value> r = v8::Local<v8::Value>::New(isolate, func->recv);
                f->Call(r, 1, &s);
            }
        }
        break; }
    }
}

InterpreterScopeData::~InterpreterScopeData()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, interpreter->mData->context);
    v8::Context::Scope contextScope(context);
    v8::Local<v8::Object> global = context->Global();
    // global->Set(v8::String::NewFromUtf8(isolate, "stdout"), v8::Undefined(isolate));
    // global->Set(v8::String::NewFromUtf8(isolate, "stderr"), v8::Undefined(isolate));
    // global->Set(v8::String::NewFromUtf8(isolate, "stdin"), v8::Undefined(isolate));

    for (auto& func : dataListeners) {
        delete func;
    }
    for (auto& func : closeListeners) {
        delete func;
    }
}

bool InterpreterScopeData::parse(const String& source, const String& name)
{
    // set up the output functions
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, interpreter->mData->context);
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Object> global = context->Global();

    // v8::Local<v8::Function> stdoutTemplate = v8::Function::New(isolate, EmitStdout);
    // v8::Local<v8::Function> stderrTemplate = v8::Function::New(isolate, EmitStderr);
    // global->Set(v8::String::NewFromUtf8(isolate, "stdout"), stdoutTemplate);
    // global->Set(v8::String::NewFromUtf8(isolate, "stderr"), stderrTemplate);
    // stdout.Reset(isolate, stdoutTemplate);
    // stderr.Reset(isolate, stderrTemplate);

    // v8::Local<v8::Object> stdinTemplate = v8::Object::New(isolate);
    // v8::Local<v8::Object> stdinOnTemplate = v8::Function::New(isolate, StdinOn);
    // stdinTemplate->Set(v8::String::NewFromUtf8(isolate, "on"), stdinOnTemplate);
    // global->Set(v8::String::NewFromUtf8(isolate, "stdin"), stdinTemplate);
    // stdin.Reset(isolate, stdinTemplate);

    const String func = "(function(io) {" + source + "})";

    v8::Handle<v8::String> fileName = v8::String::NewFromUtf8(isolate, name.constData());
    v8::Handle<v8::String> sourceString = v8::String::NewFromUtf8(isolate, func.constData());

    v8::Handle<v8::Script> scriptTemplate = v8::Script::Compile(sourceString, fileName);
    if (scriptTemplate.IsEmpty()) {
        error() << "script" << name << "didn't compile";
        return false;
    }

    script.Reset(isolate, scriptTemplate);
    return true;
}

static inline void logError(const v8::Handle<v8::Message>& msg)
{
    {
        const v8::String::Utf8Value str(msg->Get());
        error() << ToCString(str);
    }
    {
        const v8::String::Utf8Value str(msg->GetScriptResourceName());
        error() << String::format<64>("At %s:%d", ToCString(str), msg->GetLineNumber());
    }
}

void InterpreterScopeData::sendPendingStdin(v8::Isolate* isolate)
{
    if (pendingStdin.isEmpty())
        return;

    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> s = v8::String::NewFromUtf8(isolate, pendingStdin.constData());
    for (auto& func : dataListeners) {
        v8::Local<v8::Function> f = v8::Local<v8::Function>::New(isolate, func->func);
        v8::Local<v8::Value> r = v8::Local<v8::Value>::New(isolate, func->recv);
        f->Call(r, 1, &s);
    }
    pendingStdin.clear();
}

void InterpreterScopeData::exec()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, interpreter->mData->context);
    v8::Context::Scope contextScope(context);

    v8::TryCatch try_catch;

    assert(!script.IsEmpty());
    v8::Handle<v8::Script> localScript = v8::Local<v8::Script>::New(isolate, script);
    assert(!script.IsEmpty());

    const v8::Handle<v8::Value> result = localScript->Run();
    if (try_catch.HasCaught()) {
        logError(try_catch.Message());
    } else if (!result.IsEmpty() && result->IsFunction()) {
        v8::Handle<v8::Object> io = v8::Object::New(isolate);
        v8::Handle<v8::External> scopeData = v8::External::New(isolate, this);
        io->SetHiddenValue(v8::String::NewFromUtf8(isolate, "jsh::scope"), scopeData);

        // ### Use an object template for this
        v8::Handle<v8::Object> stdin = v8::Object::New(isolate);
        v8::Handle<v8::Function> on = v8::Function::New(isolate, StdinOn);
        stdin->SetHiddenValue(v8::String::NewFromUtf8(isolate, "jsh::scope"), scopeData);
        stdin->Set(v8::String::NewFromUtf8(isolate, "on"), on);
        io->Set(v8::String::NewFromUtf8(isolate, "stdin"), stdin);
        v8::Handle<v8::Function> stdout = v8::Function::New(isolate, EmitStdout);
        io->Set(v8::String::NewFromUtf8(isolate, "stdout"), stdout);
        v8::Handle<v8::Function> stderr = v8::Function::New(isolate, EmitStderr);
        io->Set(v8::String::NewFromUtf8(isolate, "stderr"), stderr);

        v8::Handle<v8::Value> arg = io;
        v8::Handle<v8::Function>::Cast(result)->Call(context->Global(), 1, &arg);
        if (try_catch.HasCaught()) {
            logError(try_catch.Message());
        } else {
            sendPendingStdin(isolate);
        }
    }
}
