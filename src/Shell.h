#ifndef SHELL_H
#define SHELL_H

#include <rct/String.h>
#include <rct/Hash.h>

class Interpreter;
class Shell
{
public:
    Shell(int argc, char** argv)
        : mArgc(argc), mArgv(argv)
    {
    }

    int exec();

    struct Token {
        enum Type {
            Javascript,
            Command,
            Pipe,
            Operator
        } type;
        static const char *typeName(Type type);

        String string;
    };

    enum TokenizeFlag {
        Tokenize_None = 0x0,
        Tokenize_CollapseWhitespace = 0x1,
        Tokenize_ExpandEnvironmentVariables = 0x2
    };
    List<Token> tokenize(String line, unsigned int flags, String &error) const;
    String env(const String &var) const { return mEnviron.value(var); }
    enum CompletionResult {
        Completion_Refresh,
        Completion_Redisplay,
        Completion_Error
    };
    CompletionResult complete(const String &line, int cursor, String &insert);
private:
    bool expandEnvironment(String &string, String &err) const;
    void process(const List<Token> &tokens);
    Hash<String, String> mEnviron;
    String mBuffer;
    int mArgc;
    char** mArgv;
    Interpreter *mInterpreter;
};

#endif
