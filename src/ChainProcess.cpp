#include "ChainProcess.h"
#include <stdio.h>
#include <assert.h>

ChainProcess::ChainProcess()
{
    Process::readyReadStdOut().connect(std::bind(&ChainProcess::processStdout, this, std::placeholders::_1));
    Process::readyReadStdErr().connect(std::bind(&ChainProcess::processStderr, this, std::placeholders::_1));
    Process::finished().connect(std::bind(&ChainProcess::processFinished, this, std::placeholders::_1));
}

ChainProcess::~ChainProcess()
{
}

void ChainProcess::init(Chain* previous)
{
    previous->stdout().connect(std::bind(&ChainProcess::previousStdout, this, std::placeholders::_1));
    previous->stderr().connect(std::bind(&ChainProcess::previousStderr, this, std::placeholders::_1));
    previous->closed().connect(std::bind(&ChainProcess::previousClosed, this, std::placeholders::_1));
}

void ChainProcess::processStdout(Process*)
{
    String data = Process::readAllStdOut();
    stdout()(std::move(data));
}

void ChainProcess::processStderr(Process*)
{
    String data = Process::readAllStdErr();
    if (errIsOut())
        stdout()(std::move(data));
    else
        stderr()(std::move(data));
}

void ChainProcess::processFinished(Process*)
{
    closed()(this);
}

void ChainProcess::previousStdout(String&& stdout)
{
    Process::write(stdout);
}

void ChainProcess::previousStderr(String&& stderr)
{
    fprintf(::stderr, "%s", stderr.constData());
}

void ChainProcess::previousClosed(Chain* chain)
{
    assert(chain != this);
    (void)chain;
    Process::closeStdIn();
}
