#include "ChainJavaScript.h"
#include <stdio.h>
#include <assert.h>

ChainJavaScript::ChainJavaScript(const String &js)
    : mScript(js)
{
}

ChainJavaScript::~ChainJavaScript()
{
}

void ChainJavaScript::init(Chain* previous)
{
    previous->stdout().connect(std::bind(&ChainJavaScript::previousStdout, this, std::placeholders::_1));
    previous->stderr().connect(std::bind(&ChainJavaScript::previousStderr, this, std::placeholders::_1));
    previous->closed().connect(std::bind(&ChainJavaScript::previousClosed, this, std::placeholders::_1));
}

void ChainJavaScript::jsStdout(String&& data)
{
    stdout()(std::move(data));
}

void ChainJavaScript::jsStderr(String&& data)
{
    if (errIsOut())
        stdout()(std::move(data));
    else
        stderr()(std::move(data));
}

void ChainJavaScript::jsClosed()
{
    if (mPrevComplete)
        closed()(this);
    else
        mIsComplete = true;
}

void ChainJavaScript::previousStdout(String&& stdout)
{
    // mScope->notify(NodeJS::Scope::StdInData, stdout);
}

void ChainJavaScript::previousStderr(String&& stderr)
{
    fprintf(::stderr, "%s", stderr.constData());
}

void ChainJavaScript::previousClosed(Chain* chain)
{
    // assert(chain != this);
    // (void)chain;
    // mScope->notify(NodeJS::Scope::StdInClosed);

    // if (mIsComplete)
    //     closed()(this);
    // else
    //     mPrevComplete = true;
}

void ChainJavaScript::notifyIsFirst()
{
    Chain::notifyIsFirst();
    //mScope->notify(NodeJS::Scope::StdInClosed);//
}
void ChainJavaScript::exec()
{

}
