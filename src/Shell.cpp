#include "Shell.h"
#include "Interpreter.h"
#include "Util.h"
#include <rct/Path.h>
#include <histedit.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <atomic>

static std::atomic<int> gotsig;
static bool continuation = false;
static wchar_t* prompt(EditLine* el)
{
    static wchar_t a[] = L"\1\033[7m\1Edit$\1\033[0m\1 ";
    static wchar_t b[] = L"Edit> ";

    return continuation ? b : a;
}

static unsigned char complete(EditLine *el, int ch)
{
    fprintf(stderr, "complete\n");
    return 0; // CC_ERROR, CC_REFRESH
}

const char* my_wcstombs(const wchar_t *wstr)
{
    static struct {
        char *str;
        int len;
    } buf;

    int needed = wcstombs(0, wstr, 0) + 1;
    if (needed > buf.len) {
        buf.str = static_cast<char*>(malloc(needed));
        buf.len = needed;
    }
    wcstombs(buf.str, wstr, needed);
    buf.str[needed - 1] = 0;

    return buf.str;
}

static void sig(int i)
{
    gotsig.store(i);
}

int Shell::exec()
{
    setlocale(LC_ALL, "");
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
    const Path rcFile = home + "/.jshrc.js";
    const Path histFile = home + "/.jshist";

    Interpreter interpreter;
    interpreter.load(rcFile);

    EditLine* el = nullptr;
    int numc;
    const wchar_t* line;
    HistoryW* hist;
    HistEventW ev;

    hist = history_winit();
    history_w(hist, &ev, H_SETSIZE, 100);
    history_w(hist, &ev, H_LOAD, histFile.constData());

    el = el_init(mArgv[0], stdin, stdout, stderr);

    el_wset(el, EL_EDITOR, L"emacs");
    el_wset(el, EL_SIGNAL, 1);
    el_wset(el, EL_PROMPT_ESC, prompt, '\1');

    el_wset(el, EL_HIST, history_w, hist);

    // complete
    el_wset(el, EL_ADDFN, L"ed-complete", L"Complete argument", complete);
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

            int max = 10;
            while (max--) {
                if (!expandEnvironment(l, &err)) {
                    break;
                }
            }
            if (!err.isEmpty()) {
                error() << err;
                continue;
            }

            if (max < 0) {
                error() << "Too many recursive environment variable expansions";
                continue;
            }

            const List<Token> tokens = tokenize(l, &err);
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

    return 0;
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

static void addPrev(List<Shell::Token> &tokens, const char *&last, const char *str)
{
    if (last && last < str) {
        tokens.append({Shell::Token::Command, String(last, str - last)});
        eatEscapes(tokens.last().string);
        last = 0;
    }
}

List<Shell::Token> Shell::tokenize(const String &line, String *err) const
{
    List<Token> tokens;
    const char *start = line.constData();
    const char *str = start;
    const char *last = str;
    int escapes = 0;
    while (*str) {
        // error() << "checking" << *str;
        if (!last && !isspace(static_cast<unsigned char>(*str)))
            last = str;
        if (*str == '\\') {
            ++escapes;
            ++str;
            continue;
        }

        switch (*str) {
        case '{': {
            addPrev(tokens, last, str);
            const char *end = findEndBrace(str + 1);
            if (!end) {
                if (err)
                    *err = String::format<128>("Can't find end of curly brace that starts at position %d", str - start);
                return List<Token>();
            }
            tokens.append({Token::Javascript, String(str, end - str + 1)});
            str = end;
            break; }
        case '\"':
        case '\'': {
            if (escapes % 2 == 0) {
                const char *end = findUnescaped(str);
                if (end) {
                    str = ++end;
                } else {
                    if (err)
                        *err = String::format<128>("Can't find end of quote that starts at position %d", str - start);
                    return List<Token>();
                }
            }
            break; }
        case '|':
            if (escapes % 2 == 0) {
                addPrev(tokens, last, str);
                if (str[1] == '|') {
                    tokens.append({Token::Operator, String(str, 2)});
                    ++str;
                } else {
                    tokens.append({Token::Pipe, String(str, 1)});
                }
            }
            break;
        case '&':
            if (escapes % 2 == 0) {
                addPrev(tokens, last, str);
                if (str[1] == '&') {
                    tokens.append({Token::Operator, String(str, 2)});
                    ++str;
                } else {
                    tokens.append({Token::Operator, String(str, 1)});
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
                addPrev(tokens, last, str);
                tokens.append({Token::Operator, String(str, 1)});
            }
            break;
        default:
            break;
        }
        escapes = 0;
        ++str;
    }
    if (last && last + 1 < str) {
        tokens.append({Token::Command, String(last, str - last)});
        eatEscapes(tokens.last().string);
    }
    return tokens;
}

void Shell::process(const List<Token> &tokens)
{
    const char *names[] = {
        "Javascript",
        "Command",
        "Pipe",
        "Operator"
    };

    for (auto token : tokens) {
        error() << String::format<128>("[%s] %s", token.string.constData(), names[token.type]);
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

bool Shell::expandEnvironment(String &string, String *err) const
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
                            if (err)
                                *err = "Bad substitution";
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
                    if (err)
                        *err = "Bad substitution";
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
