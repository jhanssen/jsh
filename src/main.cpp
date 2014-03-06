#include "Shell.h"

int main(int argc, char** argv)
{
    Shell shell(argc, argv);
    return shell.exec();
}
