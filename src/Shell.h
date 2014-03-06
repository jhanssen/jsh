#ifndef SHELL_H
#define SHELL_H

class Shell
{
public:
    Shell(int argc, char** argv)
        : mArgc(argc), mArgv(argv)
    {
    }

    int exec();

private:
    int mArgc;
    char** mArgv;
};

#endif
