#include "Interpreter.h"
#include <mutex>

static inline const char* ToCString(const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

static void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    bool first = true;
    v8::Isolate* isolate = args.GetIsolate();
    Interpreter* interpreter = static_cast<Interpreter*>(isolate->GetData(0));
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope(isolate);
        if (first) {
            first = false;
        } else {
            printf(" ");
        }
        if (args[i]->IsObject()) {
            v8::String::Utf8Value str(interpreter->toJSON(args[i], true));
            const char* cstr = ToCString(str);
            printf("%s", cstr);
        } else {
            v8::String::Utf8Value str(args[i]);
            const char* cstr = ToCString(str);
            printf("%s", cstr);
        }
    }
    printf("\n");
    fflush(stdout);
}

static v8::Handle<v8::Context> createContext(v8::Isolate* isolate)
{
    // Create a template for the global object.
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    // Bind the global 'print' function to the C++ Print callback.
    global->Set(v8::String::NewFromUtf8(isolate, "print"),
                v8::FunctionTemplate::New(isolate, Print));
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
{
    static std::once_flag v8Once;
    std::call_once(v8Once, []() { v8::V8::InitializeICU(); });

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    isolate->SetData(0, this);

    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::Context> context = createContext(isolate);
    if (context.IsEmpty()) {
        fprintf(stderr, "Error creating context\n");
        return;
    }
    mContext.Reset(isolate, context);
}

Interpreter::~Interpreter()
{
}

v8::Handle<v8::String> Interpreter::toJSON(v8::Handle<v8::Value> object, bool pretty)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, mContext);
    v8::Handle<v8::Object> global = context->Global();

    v8::Handle<v8::Object> JSON = global->Get(v8::String::NewFromUtf8(isolate, "JSON"))->ToObject();
    v8::Handle<v8::Function> stringify = v8::Handle<v8::Function>::Cast(JSON->Get(v8::String::NewFromUtf8(isolate, "stringify")));

    if (pretty) {
        v8::Handle<v8::Value> args[3] = { object, v8::Null(isolate), v8::Integer::New(isolate, 4) };
        return handle_scope.Escape(stringify->Call(JSON, 3, args)->ToString());
    }
    return handle_scope.Escape(stringify->Call(JSON, 1, &object)->ToString());
}

inline Value Interpreter::v8ValueToValue(const v8::Handle<v8::Value>& value)
{
    if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
        return Value();
    else if (value->IsTrue())
        return Value(true);
    else if (value->IsFalse())
        return Value(false);
    else if (value->IsInt32())
        return Value(value->ToInt32()->Value());
    else if (value->IsNumber())
        return Value(value->ToNumber()->Value());
    else if (value->IsString()) {
        const v8::String::Utf8Value str(value);
        return Value(ToCString(str));
    } else if (value->IsObject()) {
        const v8::String::Utf8Value str(toJSON(value));
        return Value::fromJSON(ToCString(str));
    } else {
        error() << "Unknown v8 value in Interpreter::v8ValueToValue";
    }
    return Value();
}

Value Interpreter::load(const Path& path)
{
    if (!path.isFile()) {
        error() << path << "is not a file";
        return Value();
    }
    return eval(path.readAll(), path.fileName());
}

Value Interpreter::eval(const String& data, const String& name)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, mContext);
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::String> fileName = v8::String::NewFromUtf8(isolate, name.constData());
    v8::Handle<v8::String> source = v8::String::NewFromUtf8(isolate, data.constData());

    v8::TryCatch try_catch;
    v8::Handle<v8::Script> script = v8::Script::Compile(source, fileName);
    if (script.IsEmpty()) {
        error() << "script" << name << "didn't compile";
        return Value();
    }

    const v8::Handle<v8::Value> result = script->Run();
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
        return Value();
    }

    return v8ValueToValue(result);
}
