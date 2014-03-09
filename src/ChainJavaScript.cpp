#include "ChainJavaScript.h"
#include <stdio.h>
#include <assert.h>

ChainJavaScript::ChainJavaScript(Interpreter::InterpreterScope::SharedPtr scope)
    : mScope(std::move(scope))
{
    mScope->stdout().connect(std::bind(&ChainJavaScript::jsStdout, this, std::placeholders::_1));
    mScope->stderr().connect(std::bind(&ChainJavaScript::jsStderr, this, std::placeholders::_1));
    mScope->closed().connect(std::bind(&ChainJavaScript::jsClosed, this));
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
    closed()(this);
}

void ChainJavaScript::previousStdout(String&& stdout)
{
    mScope->notify(Interpreter::InterpreterScope::StdInData, stdout);
}

void ChainJavaScript::previousStderr(String&& stderr)
{
    fprintf(::stderr, "%s", stderr.constData());
}

void ChainJavaScript::previousClosed(Chain* chain)
{
    assert(chain != this);
    (void)chain;
    mScope->notify(Interpreter::InterpreterScope::StdInClosed);
}
