#include "Job.h"
#include "NodeConnection.h"
#include "Shell.h"
#include "Splicer.h"
#include "Util.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

class JSWaiter
{
public:
    static void registerJob(int fd, Job* job);
    static void unregisterJob(int fd);

private:
    static std::mutex sMutex;
    static Hash<int, Job*> sJobs;
};

std::mutex JSWaiter::sMutex;
Hash<int, Job*> JSWaiter::sJobs;

static SocketClient::SharedPtr commandSocket;
static std::once_flag waiterFlag;

static void initCommand()
{
    const bool ok = Shell::instance()->runAndWait<int>([]() -> bool {
            commandSocket = std::make_shared<SocketClient>();
            const bool connected = commandSocket->connect(util::homeify("~/.jsh-socket"));
            if (connected) {
                commandSocket->disconnected().connect([](const SocketClient::SharedPtr&) {
                        fprintf(stderr, "command socket disconnected\n");
                    });
                commandSocket->readyRead().connect([](const SocketClient::SharedPtr&, Buffer&& buffer) {
                        char id[21];
                        enum { Bytes = 20 };
                        id[20] = '\0';
                        int taken = 0, fd;
                        const Buffer buf = std::move(buffer);
                        int pos = 0;
                        while (buf.size() - pos >= Bytes) {
                            memcpy(id + taken, buf.data() + pos, Bytes - taken);
                            fd = atoi(id);
                            JSWaiter::unregisterJob(fd);
                            pos += Bytes;
                            taken = 0;
                        }
                        if (buf.size() - pos) {
                            const int rem = buf.size() - pos;
                            assert(rem > 0);
                            if (taken + rem >= Bytes) {
                                memcpy(id + taken, buf.data() + pos, Bytes - taken);
                                fd = atoi(id);
                                JSWaiter::unregisterJob(fd);
                                pos += (Bytes - taken);
                                taken = 0;
                            }
                            if (buf.size() - pos) {
                                const int rem = buf.size() - pos;
                                assert(rem < Bytes);
                                memcpy(id + taken, buf.data() + pos, rem);
                                taken += rem;
                            }
                        }
                    });
                const int cmd = -1;
                commandSocket->write(reinterpret_cast<const unsigned char*>(&cmd), sizeof(int));
            }
            return connected;
        });
    if (!ok)
        fprintf(stderr, "unable to connect to node\n");
}

void JSWaiter::registerJob(int fd, Job* job)
{
    std::unique_lock<std::mutex>(sMutex);
    assert(!sJobs.contains(fd));
    sJobs[fd] = job;
}

void JSWaiter::unregisterJob(int fd)
{
    std::unique_lock<std::mutex>(sMutex);
    Job* job = sJobs.value(fd);
    assert(job);
    job->unregister(fd);
    sJobs.erase(fd);
}

Job::Job(int stdout)
    : mStdout(stdout), mInPipe(STDIN_FILENO)
{
}

Job::~Job()
{
}

void Job::unregister(int /*fd*/)
{
    std::unique_lock<std::mutex> locker(mMutex);
    assert(mPendingJSJobs > 0);
    --mPendingJSJobs;
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

bool Job::addNodeJS(const String& script, int fd, int flags)
{
    std::call_once(waiterFlag, initCommand);

    {
        std::unique_lock<std::mutex> locker(mMutex);
        ++mPendingJSJobs;
        JSWaiter::registerJob(fd, this);
    }

    int stdinPipe = mEntries.isEmpty() ? STDIN_FILENO : mInPipe;
    NodeConnection connection(fd);
    if (!connection.send(fd) || !connection.send(script)) {
        fprintf(stderr, "unable to write script to node (%d)\n", script.size());
        return false;
    }
    Splicer::splice(stdinPipe, fd);
    if (flags & Last) {
        Splicer::splice(fd, STDOUT_FILENO);
    } else {
        mInPipe = fd;
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
    assert(mPendingJSJobs >= 0);
    while (mPendingJSJobs) {
        mCond.wait(locker);
    }
}
