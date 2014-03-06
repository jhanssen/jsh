#include "Interpreter.h"

static const char* ToCString(const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

static void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    bool first = true;
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope(args.GetIsolate());
        if (first) {
            first = false;
        } else {
            printf(" ");
        }
        v8::String::Utf8Value str(args[i]);
        const char* cstr = ToCString(str);
        printf("%s", cstr);
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
    v8::V8::InitializeICU();
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::Context> context = createContext(isolate);
    if (context.IsEmpty()) {
        fprintf(stderr, "Error creating context\n");
        return;
    }
    context->Enter();
    mContext.Reset(isolate, context);
}

Interpreter::~Interpreter()
{
    if (!mContext.IsEmpty()) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, mContext);
        context->Exit();
    }
    mContext.Reset();
    v8::V8::Dispose();
}

void Interpreter::load(const Path& path)
{
    eval(path.readAll());
}

void Interpreter::eval(const String& script, const String& name)
{
}
