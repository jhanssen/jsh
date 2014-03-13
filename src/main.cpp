#include "Shell.h"
#include <rct/Log.h>
#include <rct/Rct.h>

int main(int argc, char** argv)
{
    Rct::findExecutablePath(argv[0]);
    Shell shell(argc, argv);
    return shell.exec();
}
