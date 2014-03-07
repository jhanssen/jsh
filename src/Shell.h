#ifndef SHELL_H
#define SHELL_H

#include <rct/String.h>
#include <rct/Hash.h>

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
        String string;
    };

    List<Token> tokenize(const String &line, String *error = 0) const;
    String env(const String &var) const { return mEnviron.value(var); }
private:
    bool expandEnvironment(String &string, String *err) const;
    void process(const List<Token> &tokens);
    Hash<String, String> mEnviron;
    String mBuffer;
    int mArgc;
    char** mArgv;
};

#endif
