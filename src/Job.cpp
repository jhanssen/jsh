#include <rct/SocketClient.h>
#include "Job.h"
#include "NodeConnection.h"
#include "Shell.h"
#include "Splicer.h"
#include "Util.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

Job::Job(int stdout)
    : mStdout(stdout), mInPipe(STDIN_FILENO), mInIsJS(false)
{
    mClosedSignal = Splicer::addCloseCallback(std::bind(&Job::onClosed, this, std::placeholders::_1));
    mErrorSignal = Splicer::addErrorCallback(std::bind(&Job::onError, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

Job::~Job()
{
    Splicer::removeCloseCallback(mClosedSignal);
    Splicer::removeErrorCallback(mErrorSignal);
}

void Job::onClosed(int fd)
{
    std::unique_lock<std::mutex> locker(mMutex);
    if (mPendingJSJobs.remove(fd))
        mCond.notify_one();
}

void Job::onError(Splicer::ErrorType type, int fd, int err)
{
    std::unique_lock<std::mutex> locker(mMutex);
    if (mPendingJSJobs.remove(fd))
        mCond.notify_one();
}

bool Job::addProcess(const Path& command, const List<String>& arguments,
                     const List<String>& environ, int flags)
{
    assert(mInPipe != -1);

    int stdoutPipe[2];
    if (!(flags & Last)) {
        if (pipe(stdoutPipe)) {
            fprintf(stderr, "pipe failed %d (%s)\n", errno, strerror(errno));
            return false;
        }
    } else {
        stdoutPipe[0] = -1;
        stdoutPipe[1] = STDOUT_FILENO;
    }

    int stdinPipe[] = { -1, -1 };
    if (mInIsJS) {
        if (pipe(stdinPipe)) {
            fprintf(stderr, "stdin pipe failed %d (%s)\n", errno, strerror(errno));
            return false;
        }
        Splicer::splice(mInPipe, stdinPipe[1]);
    }

    // fork + exec
    const pid_t pid = fork();
    switch (pid) {
    case -1:
        // error!
        fprintf(stderr, "fork failed %d (%s)\n", errno, strerror(errno));
        return false;
    case 0: {
        // child
        const char **args = new const char*[arguments.size() + 2];
        // const char* args[arguments.size() + 2];
        args[arguments.size() + 1] = 0;
        args[0] = command.nullTerminated();
        int pos = 1;
        for (List<String>::const_iterator it = arguments.begin(); it != arguments.end(); ++it) {
            args[pos] = it->nullTerminated();
            //printf("arg: '%s'\n", args[pos]);
            ++pos;
        }

        const bool hasEnviron = !environ.empty();
        const char **env = new const char*[environ.size() + 1];
        env[environ.size()] = 0;

        if (hasEnviron) {
            pos = 0;
            //printf("fork, about to exec '%s'\n", command.nullTerminated());
            for (List<String>::const_iterator it = environ.begin(); it != environ.end(); ++it) {
                env[pos] = it->nullTerminated();
                //printf("env: '%s'\n", env[pos]);
                ++pos;
            }
        }

        // dup
        if (mInIsJS) {
            dup2(stdinPipe[0], STDIN_FILENO);
            close(stdinPipe[0]);
            close(stdinPipe[1]);
        } else if (mInPipe != STDIN_FILENO) {
            dup2(mInPipe, STDIN_FILENO);
            close(mInPipe);
        }
        if (stdoutPipe[1] != STDOUT_FILENO) {
            dup2(stdoutPipe[1], STDOUT_FILENO);
            close(stdoutPipe[1]);
            close(stdoutPipe[0]);
        }

        // exec
        int ret;
        if (hasEnviron)
            ret = ::execve(command.nullTerminated(), const_cast<char* const*>(args), const_cast<char* const*>(env));
        else
            ret = ::execv(command.nullTerminated(), const_cast<char* const*>(args));
        exit(1);
        (void)ret;
        break; }
    default: {
        // parent
        const Entry entry = { Entry::Process, pid };
        mEntries.append(entry);
        if (!mInIsJS && mInPipe != STDIN_FILENO)
            close(mInPipe);
        if (stdoutPipe[1] != STDOUT_FILENO)
            close(stdoutPipe[1]);
        if (stdinPipe[0] != -1)
            close(stdinPipe[0]);
        mInPipe = stdoutPipe[0];
        mInIsJS = false;
        break; }
    }
    return true;
}

bool Job::addNodeJS(const String& script, int fd, int flags)
{
    {
        std::unique_lock<std::mutex> locker(mMutex);
        mPendingJSJobs.insert(fd);
    }

    NodeConnection connection(fd);
    if (!connection.send(fd) || !connection.send(script)) {
        fprintf(stderr, "unable to write script to node (%d)\n", script.size());
        return false;
    }
    if (!mEntries.isEmpty())
        Splicer::splice(mInPipe, fd);
    if (flags & Last) {
        Splicer::splice(fd, STDOUT_FILENO);
    } else {
        mInPipe = fd;
        mInIsJS = true;
    }

    return true;
}

void Job::wait()
{
    Set<pid_t> processes;
    {
        std::unique_lock<std::mutex> locker(mMutex);
        for (const auto& entry : mEntries) {
            if (entry.type == Entry::Process)
                processes.insert(entry.pid);
        }
    }

    int status;
    while (!processes.isEmpty()) {
        const pid_t pid = waitpid(WAIT_ANY, &status, WUNTRACED);
        assert(pid);
        if (pid > 0) {
            processes.erase(pid);
        } else {
            fprintf(stderr, "waitpid returned < 0\n");
        }
    }

    std::unique_lock<std::mutex> locker(mMutex);
    while (!mPendingJSJobs.isEmpty()) {
        mCond.wait(locker);
    }
}
