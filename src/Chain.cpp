#include "Chain.h"

void Chain::finalize()
{
    Chain* cur = this;
    while (cur->mNext) {
        cur = cur->mNext;
    }
    cur->stdout().connect(std::bind(&Chain::lastStdOut, this, std::placeholders::_1));
    cur->stderr().connect(std::bind(&Chain::lastStdErr, this, std::placeholders::_1));
    cur->closed().connect(std::bind(&Chain::lastClosed, this, std::placeholders::_1));
}

void Chain::lastStdOut(String&& stdout)
{
    mFinishedStdOut(std::move(stdout));
}

void Chain::lastStdErr(String&& stderr)
{
    mFinishedStdErr(std::move(stderr));
}

void Chain::lastClosed(Chain* chain)
{
    if (chain != this)
        delete chain;
    mComplete();
}
