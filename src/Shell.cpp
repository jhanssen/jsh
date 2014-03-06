#include "Shell.h"
#include <rct/Path.h>
#include <histedit.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <atomic>

static std::atomic<int> gotsig;
static int continuation;
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

static Path homeDirectory()
{
    if (char* homeEnv = getenv("HOME"))
        return Path(homeEnv);

    struct passwd pwent;
    struct passwd* pwentp;
    char buf[8192];

    if (getpwuid_r(getuid(), &pwent, buf, sizeof(buf), &pwentp))
        return Path();
    return Path(pwent.pw_dir);
}

int Shell::exec()
{
    setlocale(LC_ALL, "");

    (void)signal(SIGINT,  sig);
    (void)signal(SIGQUIT, sig);
    (void)signal(SIGHUP,  sig);
    (void)signal(SIGTERM, sig);

    EditLine* el = nullptr;
    int numc, ncontinuation;
    const wchar_t* line;
    HistoryW* hist;
    HistEventW ev;
    TokenizerW* tok;

    const Path elFile = homeDirectory() + "/.jshel";
    const Path histFile = homeDirectory() + "/.jshist";

    hist = history_winit();
    history_w(hist, &ev, H_SETSIZE, 100);
    history_w(hist, &ev, H_LOAD, histFile.constData());

    tok = tok_winit(NULL);

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
        int ac, cc, co, rc;
        const wchar_t** av;

        const LineInfoW* li = el_wline(el);
        //fwprintf(stderr, L"got %d %ls\n", numc, line);

        if (int s = gotsig.load()) {
            fprintf(stderr, "got signal %d\n", s);
            gotsig.store(0);
            el_reset(el);
        }

        if (!continuation && numc == 1)
            continue;	/* Only got a linefeed */

        ac = cc = co = 0;
        ncontinuation = tok_wline(tok, li, &ac, &av, &cc, &co);
        if (ncontinuation < 0) {
            fprintf(stderr, "error in tok_wline %d\n", ncontinuation);
            continuation = 0;
            continue;
        }

        history_w(hist, &ev, continuation ? H_APPEND : H_ENTER, line);

        continuation = ncontinuation;
        ncontinuation = 0;
        if (continuation)
            continue;

        if (el_wparse(el, ac, av) == -1) {
            // do stuff?
            for (int i = 0; i < ac; ++i) {
                fwprintf(stderr, L"arg %d '%ls', ", i, av[i]);
            }
            fprintf(stderr, "\n");
        }

        tok_wreset(tok);
    }

    el_end(el);
    tok_wend(tok);
    history_w(hist, &ev, H_SAVE, histFile.constData());
    history_wend(hist);

    fprintf(stdout, "\n");

    return 0;
}
