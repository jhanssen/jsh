#include "Chain.h"

void Chain::exec()
{
    Chain* cur = this;
    while (cur->mNext) {
        cur = cur->mNext;
    }
    cur->stdout().connect(std::bind(&Chain::lastStdOut, this, std::placeholders::_1));
    cur->stderr().connect(std::bind(&Chain::lastStdErr, this, std::placeholders::_1));
    cur->closed().connect(std::bind(&Chain::lastClosed, this));
}

void Chain::lastStdOut(String&& stdout)
{
    mFinishedStdOut(std::move(stdout));
}

void Chain::lastStdErr(String&& stderr)
{
    mFinishedStdErr(std::move(stderr));
}

void Chain::lastClosed()
{
    mComplete();
}
