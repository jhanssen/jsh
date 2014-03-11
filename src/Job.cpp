#include "Job.h"
#include <stdio.h>
#include <errno.h>

Job::Job(int stdout)
    : mStdout(stdout), mInPipe(STDIN_FILENO)
{
}

Job::~Job()
{
}

bool Job::addProcess(const Path& command, int flags,
                     const List<String>& arguments,
                     const List<String>& environ)
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
    std::unique_lock<std::mutex> locker(mMutex);
    // fork + exec
    const pid_t pid = fork();
    switch (pid) {
    case -1:
        // error!
        fprintf(stderr, "fork failed %d (%s)\n", errno, strerror(errno));
        break;
    case 0: {
        // child
        const char **args = new const char*[arguments.size() + 2];
        // const char* args[arguments.size() + 2];
        args[arguments.size() + 1] = 0;
        args[0] = cmd.nullTerminated();
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
            //printf("fork, about to exec '%s'\n", cmd.nullTerminated());
            for (List<String>::const_iterator it = environ.begin(); it != environ.end(); ++it) {
                env[pos] = it->nullTerminated();
                //printf("env: '%s'\n", env[pos]);
                ++pos;
            }
        }

        // dup
        if (mInPipe != STDIN_FILENO) {
            dup2(mInPipe, STDIN_FILENO);
            close(mInPipe);
        }
        if (stdoutPipe[1] != STDOUT_FILENO) {
            dup2(stdoutPipe[1], STDOUT_FILENO);
            close(stdoutPipe[0]);
        }

        // exec
        int ret;
        if (hasEnviron)
            ret = ::execve(cmd.nullTerminated(), const_cast<char* const*>(args), const_cast<char* const*>(env));
        else
            ret = ::execv(cmd.nullTerminated(), const_cast<char* const*>(args));
        exit(1);
        (void)ret;
        break; }
    default: {
        // parent
        mEntries.append({ pid });
        if (mInPipe != STDIN_FILENO)
            close(mInPipe);
        if (stdoutPipe[1] != STDOUT_FILENO)
            close(stdoutPipe[1]);
        mInPipe = stdoutPipe[0];
        break; }
    }
}

bool Job::addPipe(int& stdout, int flags)
{
    std::unique_lock<std::mutex> locker(mMutex);
    stdout = mEntries.isEmpty() ? STDOUT_FILENO : mInPipe;
    if (!(flags & Last)) {

    }
}

void Job::exec()
{
    {
        std::unique_lock<std::mutex> locker(mMutex);
        if (mEntries.isEmpty()) {
            return;
        }
    }
    start();
    join();
}

void Job::run()
{
}
