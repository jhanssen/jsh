#include "Shell.h"
#include <rct/Log.h>
#include <rct/Rct.h>

int main(int argc, char** argv)
{
    Rct::findExecutablePath(argv[0]);

    const char *logFile = 0;
    unsigned logFlags = 0;
    int logLevel = 0;

    if (!initLogging(argv[0], 0, logLevel, logFile, logFlags)) {
        fprintf(stderr, "Can't initialize logging with %s %d %d %s 0x%0x\n",
                argv[0], LogStderr, logLevel, logFile ? logFile : "", logFlags);
        return 1;
    }

    Shell shell(argc, argv);
    return shell.exec();
}
