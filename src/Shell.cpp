#include "Shell.h"
#include "NodeJS.h"
#include "Input.h"
#include "Util.h"
#include <rct/EventLoop.h>
#include <rct/Rct.h>
#include <getopt.h>

extern char **environ;

Shell* Shell::sInstance;

static inline void usage(FILE *f)
{
    fprintf(f,
            "jsh [...options...]\n"
            "  --help|-h                                  Display this page.\n"
            "  --socket-file|-s [arg]                     Use this file for the server socket.\n"
            "  --socket-file-template|-T [arg]            Use this template for mkstemp(3) when generating a socket-file name.\n"
            "  --no-autostart-node-js|-a [arg]            Don't start node.js in the background.\n"
            "  --verbose|-v                               Be more verbose.\n"
            "  --silent|-S                                Be silent.\n"
            "  --edit-rc|-x [arg]                         Use this file instead of ~/.editrc for libedit initialization.\n"
            "  --history-file|-H [arg]                    Use this file for command history (default ~/.jsh_history).\n"
            "  --logfile|-l [arg]                         Log to this file.\n");
}

int Shell::exec()
{
    for (int i=0; environ[i]; ++i) {
        char *eq = strchr(environ[i], '=');
        if (eq) {
            mEnviron[String(environ[i], eq)] = eq + 1;
        } else {
            mEnviron[environ[i]] = String();
        }
    }

    const Path home = util::homeDirectory();
    Path socketFile;
    String socketFileTemplate = "/tmp/jsh.XXXXXX";
    unsigned int nodeFlags = NodeJS::Autostart;

    option opts[] = {
        { "help", no_argument, 0, 'h' },
        { "socket-file", required_argument, 0, 's' },
        { "socket-file-template", required_argument, 0, 'T' },
        { "no-autostart-node-js", no_argument, 0, 'a' },
        { "verbose", no_argument, 0, 'v' },
        { "history-file", required_argument, 0, 'H' },
        { "edit-rc", required_argument, 0, 'x' },
        { "silent", no_argument, 0, 'S' },
        { "log-file", required_argument, 0, 'l' },
        { 0, 0, 0, 0 }
    };

    const String shortOptions = Rct::shortOptions(opts);
    if (getenv("JSH_DUMP_UNUSED")) {
        String unused;
        for (int i=0; i<26; ++i) {
            if (!shortOptions.contains('a' + i))
                unused.append('a' + i);
            if (!shortOptions.contains('A' + i))
                unused.append('A' + i);
        }
        printf("Unused: %s\n", unused.constData());
        for (int i=0; opts[i].name; ++i) {
            if (opts[i].name) {
                if (!opts[i].val) {
                    printf("No shortoption for %s\n", opts[i].name);
                } else if (opts[i].name[0] != opts[i].val) {
                    printf("Not ideal option for %s|%c\n", opts[i].name, opts[i].val);
                }
            }
        }
        return 0;
    }

    int logLevel = Error;
    Path logFile;
    Path histFile = home + "/.jsh_history";
    List<Path> editRcFiles;

    while (true) {
        const int c = getopt_long(mArgc, mArgv, shortOptions.constData(), opts, 0);
        if (c == -1)
            break;
        switch (c) {
        case 'h':
            usage(stdout);
            return 0;
        case 's':
            socketFile = optarg;
            break;
        case 'T':
            socketFileTemplate = optarg;
            break;
        case 'a':
            nodeFlags &= ~NodeJS::Autostart;
            break;
        case 'H':
            histFile = optarg;
            break;
        case 'x': {
            Path p = optarg;
            if (p.isFile()) {
                editRcFiles << optarg;
            } else {
                error() << "Can't open" << optarg << "for reading";
                return 1;
            }
            break; }
        case 'v':
            if (logLevel >= 0)
                ++logLevel;
            break;
        case 'S':
            logLevel = -1;
            break;
        case 'l':
            logFile = optarg;
            break;
        case '?': {
            fprintf(stderr, "Run jsh --help for help\n");
            return 1; }
        }
    }

    if (editRcFiles.isEmpty()) {
        const Path rcFile = (home + "/.editrc");
        if (rcFile.isFile()) {
            editRcFiles << rcFile;
        }
    }

    if (!initLogging(mArgv[0], 0, logLevel, logFile, 0)) {
        fprintf(stderr, "Can't initialize logging with %d %s\n",
                logLevel, logFile.constData());
        return 1;
    }

    if (socketFile.isEmpty()) {
        const int f = mkstemp(socketFileTemplate.data());
        if (f == -1) {
            error() << "Error generating temp file" << strerror(errno);
            return 1;
        }
        close(f);
        socketFile = socketFileTemplate;
    }

    mEventLoop = std::make_shared<EventLoop>();
    mEventLoop->init(EventLoop::MainEventLoop);

    const Input::Options options = {
        mArgv[0],
        histFile,
        editRcFiles,
        logLevel,
        socketFile,
        nodeFlags
    };

    mInput = std::make_shared<Input>(options);
    mInput->start();

    mNodeJS = std::make_shared<NodeJS>();
    mNodeJS->init(socketFile, nodeFlags);

    mEventLoop->exec();
    mInput->join();

    return 0;
}

static const char *typeNames[] = {
    "Javascript",
    "Command",
    "Pipe",
    "Operator"
};

const char * Shell::Token::typeName(Type type)
{
    return typeNames[type];
}
