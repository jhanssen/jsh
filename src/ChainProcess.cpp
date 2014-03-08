#include "ChainProcess.h"
#include <stdio.h>

ChainProcess::ChainProcess(Process* process)
    : mProcess(process)
{
    mProcess->readyReadStdOut().connect(std::bind(&ChainProcess::processStdout, this, std::placeholders::_1));
    mProcess->readyReadStdErr().connect(std::bind(&ChainProcess::processStderr, this, std::placeholders::_1));
    mProcess->finished().connect(std::bind(&ChainProcess::processFinished, this, std::placeholders::_1));
}

ChainProcess::~ChainProcess()
{
    delete mProcess;
}

void ChainProcess::init(Chain* previous)
{
    previous->stdout().connect(std::bind(&ChainProcess::previousStdout, this, std::placeholders::_1));
    previous->stderr().connect(std::bind(&ChainProcess::previousStderr, this, std::placeholders::_1));
    previous->closed().connect(std::bind(&ChainProcess::previousClosed, this));
}

void ChainProcess::processStdout(Process*)
{
    String data = mProcess->readAllStdOut();
    stdout()(std::move(data));
}

void ChainProcess::processStderr(Process*)
{
    String data = mProcess->readAllStdErr();
    if (errIsOut())
        stdout()(std::move(data));
    else
        stderr()(std::move(data));
}

void ChainProcess::processFinished(Process*)
{
    closed()();
}

void ChainProcess::previousStdout(String&& stdout)
{
    mProcess->write(stdout);
}

void ChainProcess::previousStderr(String&& stderr)
{
    fprintf(::stderr, "%s", stderr.constData());
}

void ChainProcess::previousClosed()
{
}
