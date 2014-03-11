#include "Job.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

Job::Job(int stdout)
    : mStdout(stdout), mInPipe(STDIN_FILENO)
{
}

Job::~Job()
{
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
            ret = ::execve(command.nullTerminated(), const_cast<char* const*>(args), const_cast<char* const*>(env));
        else
            ret = ::execv(command.nullTerminated(), const_cast<char* const*>(args));
        exit(1);
        (void)ret;
        break; }
    default: {
        // parent
        mEntries.append({ Entry::Process, pid });
        if (mInPipe != STDIN_FILENO)
            close(mInPipe);
        if (stdoutPipe[1] != STDOUT_FILENO)
            close(stdoutPipe[1]);
        mInPipe = stdoutPipe[0];
        break; }
    }
    return true;
}

bool Job::addNode(const String& script, const String& socketFile, int flags)
{
    int stdoutPipe = mEntries.isEmpty() ? STDOUT_FILENO : mInPipe;
    if (!(flags & Last)) {

    }
    return false;
}

List<Job::Entry>::iterator Job::wait(List<Entry>::iterator entry)
{
    for (;;) {
        switch (entry->type) {
        case Job::Entry::Process: {
            int status;
            const pid_t pid = waitpid(WAIT_ANY, &status, WUNTRACED);
            if (pid == -1) {
                if (errno == EINTR)
                    break;
                fprintf(stderr, "waitpid error %d (%s)\n", errno, strerror(errno));
                return mEntries.end();
            } else if (pid == entry->pid) {
                return mEntries.erase(entry);
            }
            entry = mEntries.erase(entry);
            break; }
        case Job::Entry::Node:
            break;
        }
    }
}

void Job::wait()
{
    List<Entry>::iterator entry = mEntries.begin();
    while (entry != mEntries.end()) {
        entry = wait(entry);
    }
}
