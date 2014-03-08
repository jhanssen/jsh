#include "Input.h"
#include "ChainProcess.h"
#include "Interpreter.h"
#include "Util.h"
#include <rct/EventLoop.h>
#include <rct/Path.h>
#include <rct/Process.h>
#include <rct/Thread.h>
#include <rct/Log.h>
#include <rct/List.h>
#include <rct/Value.h>
#include <histedit.h>
#include <locale.h>
#include <langinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

extern char **environ;

static std::atomic<int> gotsig;
static bool continuation = false;

static void sig(int i)
{
    gotsig.store(i);
}

static wchar_t* prompt(EditLine* el)
{
    static wchar_t a[] = L"\1\033[7m\1Edit$\1\033[0m\1 ";
    static wchar_t b[] = L"Edit> ";

    return continuation ? b : a;
}

class InputLogOutput : public LogOutput
{
public:
    InputLogOutput(Input* in)
        : LogOutput(0), input(in), inputThreadId(std::this_thread::get_id())
    {}

    virtual void log(const char *msg, int)
    {
        if (inputThreadId == std::this_thread::get_id())
            fwprintf(stdout, L"%ls\n", util::utf8ToWChar(msg).c_str());
        else
            input->write(util::utf8ToWChar(msg));
    }

private:
    Input* input;
    std::thread::id inputThreadId;
};

unsigned char Input::elComplete(EditLine *el, int)
{
    Input *input = 0;
    el_wget(el, EL_CLIENTDATA, &input);
    assert(input);
    String insert;
    const LineInfoW *lineInfo = el_wline(el);
    const String line = util::wcharToUtf8(lineInfo->buffer);
    const String rest = util::wcharToUtf8(lineInfo->cursor);
    const int len = util::utf8CharacterCount(line);
    const int cursorPos = len - util::utf8CharacterCount(rest);

    const Input::CompletionResult res = input->complete(line, cursorPos, insert);
    if (!insert.isEmpty()) {
        el_winsertstr(el, util::utf8ToWChar(insert).c_str());
    }

    switch (res) {
    case Input::Completion_Refresh: return CC_REFRESH;
    case Input::Completion_Redisplay: return CC_REDISPLAY;
    case Input::Completion_Error: return CC_ERROR;
    }
}

void Input::write(const std::wstring& data)
{
    const int r = ::write(mPipe[1], data.c_str(), data.size() * sizeof(wchar_t));
    if (r == -1)
        fprintf(stderr, "Unable to write to input pipe: %d\n", errno);
}

void Input::write(const wchar_t* data, ssize_t len)
{
    if (len == -1)
        len = static_cast<ssize_t>(wcslen(data));
    const int r = ::write(mPipe[1], data, len * sizeof(wchar_t));
    if (r == -1)
        fprintf(stderr, "Unable to write to input pipe: %d\n", errno);
}

int Input::getChar(EditLine *el, wchar_t *ch)
{
    Input *input = 0;
    el_wget(el, EL_CLIENTDATA, &input);
    assert(input);

    int& readPipe = input->mPipe[0];

    if (readPipe == -1)
        return -1;

    fd_set rset;
    char out[4];
    int outpos;
    wchar_t buf[8192];
    int r;
    const int max = std::max(readPipe, STDIN_FILENO) + 1;
    for (;;) {
        FD_ZERO(&rset);
        FD_SET(readPipe, &rset);
        FD_SET(STDIN_FILENO, &rset);
        r = ::select(max, &rset, 0, 0, 0);
        if (r <= 0) {
            fprintf(stderr, "Select returned <= 0\n");
            return -1;
        }
        if (FD_ISSET(readPipe, &rset)) {
            for (;;) {
                r = ::read(readPipe, buf, sizeof(buf) - sizeof(wchar_t));
                if (r > 0) {
                    *(buf + (r / sizeof(wchar_t))) = L'\0';
                    fwprintf(stdout, L"%ls\n", buf);
                } else {
                    if (!r || (r == -1 && errno != EINTR && errno != EAGAIN)) {
                        ::close(readPipe);
                        readPipe = -1;
                        fprintf(stderr, "Read from pipe returned %d (%d)\n", r, errno);
                        return -1;
                    }
                    if (errno == EAGAIN) {
                        fflush(stdout);
                        if (!FD_ISSET(STDIN_FILENO, &rset)) {
                            //printf("refreshing\n");
                            el_wset(el, EL_REFRESH);
                        }
                        break;
                    }
                }
            }
        }
        if (FD_ISSET(STDIN_FILENO, &rset)) {
            int r;
            if (input->isUtf8()) {
                outpos = 0;
                for (;;) {
                    r = ::read(STDIN_FILENO, out + outpos++, 1);
                    if (r <= 0) {
                        fprintf(stderr, "Failed to read (utf8)\n");
                        return -1;
                    }
                    r = mbtowc(ch, out, outpos);
                    if (r > 0) {
                        return 1;
                    }
                    if (outpos == 4) {
                        // bad
                        fprintf(stderr, "Invalid utf8 sequence\n");
                        return -1;
                    }
                    mbtowc(0, 0, 0);
                }
            } else {
                // Go for ascii
                r = ::read(STDIN_FILENO, out, 1);
                if (r <= 0) {
                    fprintf(stderr, "Failed to read (ascii)\n");
                    return -1;
                }
                *ch = out[0];
                return 1;
            }
            fprintf(stderr, "balle\n");
            return -1;
        }
    }
    fprintf(stderr, "Out of getChar\n");
    return -1;
}

void Input::run()
{
    setlocale(LC_ALL, "");
    if (!strcmp(nl_langinfo(CODESET), "UTF-8"))
        mIsUtf8 = true;

    new InputLogOutput(this);

    int flags, ret;
    ret = ::pipe(mPipe);
    if (ret != -1) {
        do {
            int flags = fcntl(mPipe[0], F_GETFL, 0);
        } while (flags == -1 && errno == EINTR);
        if (flags != -1) {
            do {
                ret = fcntl(mPipe[0], F_SETFL, flags | O_NONBLOCK);
            } while (ret == -1 && errno == EINTR);
        }
    }

    for (int i=0; environ[i]; ++i) {
        char *eq = strchr(environ[i], '=');
        if (eq) {
            mEnviron[String(environ[i], eq)] = eq + 1;
        } else {
            mEnviron[environ[i]] = String();
        }
    }
    (void)signal(SIGINT,  sig);
    (void)signal(SIGQUIT, sig);
    (void)signal(SIGHUP,  sig);
    (void)signal(SIGTERM, sig);

    const Path home = util::homeDirectory();
    const Path elFile = home + "/.jshel";
    const Path histFile = home + "/.jshist";

    EditLine* el = nullptr;
    int numc;
    const wchar_t* line;
    HistoryW* hist;
    HistEventW ev;

    hist = history_winit();
    history_w(hist, &ev, H_SETSIZE, 100);
    history_w(hist, &ev, H_LOAD, histFile.constData());

    el = el_init(mArgv[0], stdin, stdout, stderr);
    el_wset(el, EL_CLIENTDATA, this);

    el_wset(el, EL_EDITOR, L"emacs");
    el_wset(el, EL_SIGNAL, 1);
    el_wset(el, EL_GETCFN, &Input::getChar);
    el_wset(el, EL_PROMPT_ESC, prompt, '\1');
    if (const char *home = getenv("HOME")) {
        Path rc = home;
        rc += "/.editrc";
        if (rc.exists())
            el_source(el, rc.constData());
    }

    el_wset(el, EL_HIST, history_w, hist);

    // complete
    el_wset(el, EL_ADDFN, L"ed-complete", L"Complete argument", &Input::elComplete);
    // bind tab
    el_wset(el, EL_BIND, L"^I", L"ed-complete", NULL);

    el_source(el, elFile.constData());

    while ((line = el_wgets(el, &numc)) && numc) {
        if (int s = gotsig.load()) {
            fprintf(stderr, "got signal %d\n", s);
            gotsig.store(0);
            el_reset(el);
        }

        String l = util::wcharToUtf8(line);
        String copy = l;
        copy.chomp(" \t\n");
        if (copy.endsWith('\\')) {
            mBuffer += copy;
            continuation = true;
            history_w(hist, &ev, H_APPEND, line);
            continue;
        }
        history_w(hist, &ev, H_ENTER, line);
        continuation = false;
        if (!mBuffer.isEmpty()) {
            l = mBuffer + l;
            mBuffer.clear();
        }

        if (!l.isEmpty()) {
            String err;
            if (l.endsWith('\n'))
                l.chop(1);

            const List<Shell::Token> tokens = tokenize(l, Tokenize_CollapseWhitespace|Tokenize_ExpandEnvironmentVariables, err);
            if (!err.isEmpty()) {
                error() << "Got error" << err;
            } else if (!tokens.isEmpty()) {
                process(tokens);
            }
        }
    }

    el_end(el);
    history_w(hist, &ev, H_SAVE, histFile.constData());
    history_wend(hist);

    fprintf(stdout, "\n");

    if (mPipe[0] != -1)
        ::close(mPipe[0]);
    ::close(mPipe[1]);

    EventLoop::mainEventLoop()->quit();
}

static inline const char *findUnescaped(const char *str)
{
    const char ch = *str;
    int escapes = 0;
    while (*++str) {
        if (*str == ch && escapes % 2 == 0) {
            return str;
        }
        if (*str == '\\') {
            ++escapes;
        } else {
            escapes = 0;
        }
    }
    return 0;
}

static inline const char *findEndBrace(const char *str)
{
    // ### need to support comments
    int braces = 1;
    while (*str) {
        switch (*str) {
        case '}':
            if (!--braces)
                return str;
            break;
        case '{':
            ++braces;
            break;
        case '"':
        case '\'':
            str = findUnescaped(str);
            if (!str)
                return 0;
            break;
        }
        ++str;
    }
    return 0;
}

static inline void eatEscapes(String &string)
{
    int i = 0;
    while (i < string.size()) {
        if (string.at(i) == '\\')
            string.remove(i, 1);
        ++i;
    }
}

static inline String stripBraces(String&& string)
{
    String str = std::move(string);
    if (str.isEmpty())
        return str;
    switch (str.first()) {
    case '\'':
    case '"':
    case '{':
        assert(str.size() >= 2);
        return str.mid(1, str.size() - 2);
    }
    return str;
}

void Input::addPrev(List<Shell::Token> &tokens, const char *&last, const char *str, unsigned int flags)
{
    if (last && last < str) {
        tokens.append({Shell::Token::Command, stripBraces(String(last, str - last + 1))});
        if (flags & Tokenize_CollapseWhitespace) {
            eatEscapes(tokens.last().string);
            tokens.last().string.chomp(' ');
        }
    }
    last = 0;
}

void Input::addArg(List<Shell::Token> &tokens, const char *&last, const char *str, unsigned int flags)
{
    if (last && last < str) {
        tokens.last().args.append(stripBraces(String(last, str - last + 1)));
        if (flags & Tokenize_CollapseWhitespace) {
            eatEscapes(tokens.last().args.last());
            tokens.last().args.last().chomp(' ');
        }
    }
    last = 0;
}

List<Shell::Token> Input::tokenize(String line, unsigned int flags, String &err) const
{
    assert(err.isEmpty());
    if (flags & Tokenize_ExpandEnvironmentVariables) {
        int max = 10;
        while (max--) {
            if (!expandEnvironment(line, err)) {
                break;
            }
        }
        if (!err.isEmpty()) {
            return List<Shell::Token>();
        }

        if (max < 0) {
            err = "Too many recursive environment variable expansions";
            return List<Shell::Token>();
        }
    }


    List<Shell::Token> tokens;
    const char *start = line.constData();
    const char *str = start;
    const char *last = str;
    int escapes = 0;
    while (*str) {
        // error() << "checking" << *str;
        // ### isspace needs to be utf8-aware
        if (!last && (!(flags & Tokenize_CollapseWhitespace) || !isspace(static_cast<unsigned char>(*str))))
            last = str;
        if (*str == '\\') {
            ++escapes;
            ++str;
            continue;
        }
        // if (last && str)
        //     printf("last %c %p, str %c %p\n", *last, last, *str, str);

        switch (*str) {
        case '{': {
            addPrev(tokens, last, str, flags);
            const char *end = findEndBrace(str + 1);
            if (!end) {
                err = String::format<128>("Can't find end of curly brace that starts at position %d", str - start);
                return List<Shell::Token>();
            }
            tokens.append({Shell::Token::Javascript, String(str, end - str + 1)});
            str = end;
            break; }
        case '\"':
        case '\'': {
            if (escapes % 2 == 0) {
                const char *end = findUnescaped(str);
                if (end) {
                    str = end;
                } else {
                    err = String::format<128>("Can't find end of quote that starts at position %d", str - start);
                    return List<Shell::Token>();
                }
            }
            break; }
        case '|':
            if (escapes % 2 == 0) {
                addPrev(tokens, last, str, flags);
                if (str[1] == '|') {
                    tokens.append({Shell::Token::Operator, String(str, 2)});
                    ++str;
                } else {
                    tokens.append({Shell::Token::Pipe, String(str, 1)});
                }
            }
            break;
        case '&':
            if (escapes % 2 == 0) {
                addPrev(tokens, last, str, flags);
                if (str[1] == '&') {
                    tokens.append({Shell::Token::Operator, String(str, 2)});
                    ++str;
                } else {
                    tokens.append({Shell::Token::Operator, String(str, 1)});
                }
            }
            break;
        case ';':
        case '<':
        case '>':
        case '(':
        case ')':
        case '!':
            if (escapes % 2 == 0) {
                addPrev(tokens, last, str, flags);
                tokens.append({Shell::Token::Operator, String(str, 1)});
            }
            break;
        case ' ':
            if (escapes % 2 == 0) {
                if (!tokens.isEmpty() && tokens.last().type == Shell::Token::Command) {
                    addArg(tokens, last, str, flags);
                } else {
                    addPrev(tokens, last, str, flags);
                }
            }
            break;
        default:
            break;
        }
        escapes = 0;
        ++str;
    }
    if (last && last + 1 < str) {
        if (!tokens.isEmpty() && tokens.last().type == Shell::Token::Command) {
            tokens.last().args.append(stripBraces(String(last, str - last)));
            if (flags & Tokenize_CollapseWhitespace)
                eatEscapes(tokens.last().args.last());
        } else {
            tokens.append({Shell::Token::Command, stripBraces(String(last, str - last))});
            if (flags & Tokenize_CollapseWhitespace)
                eatEscapes(tokens.last().string);
        }
    }
    return tokens;
}

void Input::runCommand(const String& command, const List<String>& arguments)
{
    // mEventLoop->callLaterMove([](const String& cmd, const List<String>& args) {
    //         Process* proc = new Process;
    //         ChainProcess* chain = new ChainProcess(proc);
    //         chain->finishedStdOut().connect<EventLoop::Move>(std::bind([](String&& str) {
    //                     fprintf(stdout, "%s", str.constData());
    //                 }, std::placeholders::_1));
    //         chain->finishedStdErr().connect<EventLoop::Move>(std::bind([](String&& str) {
    //                     fprintf(stderr, "%s", str.constData());
    //                 }, std::placeholders::_1));
    //         chain->exec();
    //         proc->start(cmd, args);
    //     }, std::move(command), std::move(arguments));
}

void Input::process(const List<Shell::Token> &tokens)
{
    for (const auto& token : tokens) {
        String args;
        for (const String& arg : token.args) {
            args += arg + "-";
        }
        if (!args.isEmpty())
            args.chop(1);
        error() << String::format<128>("[%s] %s (%s)", token.string.constData(), Shell::Token::typeName(token.type), args.constData());
        switch (token.type) {
        case Shell::Token::Command:
            runCommand(token.string, token.args);
            break;
        }
    }
}

enum EnvironmentCharFlag {
    Invalid,
    Valid,
    ValidNonStart
};

static inline bool environmentVarChar(unsigned char ch)
{
    if (isdigit(ch))
        return ValidNonStart;
    return (isalpha(ch) || ch == '_' ? Valid : Invalid);
}

bool Input::expandEnvironment(String &string, String &err) const
{
    int escapes = 0;
    for (int i=0; i<string.size() - 1; ++i) {
        switch (string.at(i)) {
        case '$':
            if (escapes % 2 == 0) {
                if (string.at(i + 1) == '{') {
                    for (int j=i + 2; j<string.size(); ++j) {
                        if (string.at(j) == '}') {
                            const String env = string.mid(i + 2, j - (i + 2));
                            const String sub = mEnviron.value(env);
                            string.replace(i, j - i + 1, sub);
                            // error() << "Got sub" << string.mid(i + 2, j - 2);
                        } else if (environmentVarChar(string.at(j)) == Invalid) {
                            err = "Bad substitution";
                            return false;
                        }
                    }

                } else if (string.at(i + 1) == '$') {
                    string.remove(i + 1, 1);
                } else if (environmentVarChar(string.at(i + 1)) == Valid) {
                    int j=i + 2;
                    while (j<string.size() && environmentVarChar(string.at(j)) != Invalid)
                        ++j;
                    const String sub = mEnviron.value(string.mid(i + 1, j - 1));
                    string.replace(i, j - i + 1, sub);
                } else {
                    err = "Bad substitution";
                    return false;
                }
                // if (
                // if (isstring.at(i + 1)
            }
            escapes = 0;
            break;
        case '\\':
            ++escapes;
            break;
        default:
            escapes = 0;
            break;
        }
    }
}

static Value jsCall(Shell* shell, const String &object, const String &function,
                    const List<Value> &args, bool *ok = 0)
{
    return shell->runAndWait<Value>([&]() -> Value {
            return shell->interpreter()->call(object, function, args, ok);
        });
}

static Value jsCall(Shell* shell, const String &function, const List<Value> &args, bool *ok = 0)
{
    return jsCall(shell, String(), function, args, ok);
}

// cursor is the unicode character position, line is in utf8
Input::CompletionResult Input::complete(const String &line, int cursor, String &insert)
{
    String err;
    const List<Shell::Token> tokens = tokenize(line, Tokenize_None, err);
    if (!err.isEmpty()) {
        return Completion_Error;
    } else if (tokens.isEmpty()) {
        return Completion_Refresh;
    }

    List<Value> args;
    args.append(line);
    args.append(cursor);
    List<Value> toks;
    for (auto token : tokens) {
        Map<String, Value> val;
        val["type"] = Shell::Token::typeName(token.type);
        val["string"] = token.string;
        toks.append(val);
    }
    args.append(toks);
    bool ok;
    //const Value value = mInterpreter->call("complete", args, &ok);
    const Value value = jsCall(mShell, "complete", args, &ok);
    if (!ok)
        return Completion_Error;
    const String result = value.value<String>("result");
    insert = value.value<String>("insert");
    if (result == "refresh") {
        return Completion_Refresh;
    } else if (result == "redisplay") {
        return Completion_Redisplay;
    } else {
        insert.clear();
        return Completion_Error;
    }
}
