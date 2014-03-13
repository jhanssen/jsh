#ifndef INPUT_H
#define INPUT_H

#include "Shell.h"
#include <rct/Thread.h>
#include <rct/String.h>
#include <rct/SocketClient.h>
#include <rct/Timer.h>
#include <rct/Process.h>
#include <rct/Path.h>
#include <string>
#include <memory>

struct editline;
typedef struct editline EditLine;

class Input : public Thread, public std::enable_shared_from_this<Input>
{
public:
    typedef std::shared_ptr<Input> SharedPtr;
    typedef std::weak_ptr<Input> WeakPtr;

    enum Flag {
        None = 0x0,
        AutostartNodeJS = 0x1
    };

    struct Options
    {
        Path argv0;
        Path histFile;
        List<Path> editRcFiles;
        int verbosity;
        Path socketFile;
        unsigned int flags;
    };
    Input(const Options &options)
        : mOptions(options), mEl(0), mIsUtf8(false), mState(Normal), mNodeProcess(0)
    {
        if (options.flags & AutostartNodeJS)
            launchNode();
    }

    ~Input()
    {
        if (mNodeProcess) {
            mNodeProcess->kill();
            delete mNodeProcess;
        }
        Path::rm(mOptions.socketFile);
    }

    // Assumes multi-byte encoding
    void write(const String& data);
    void write(const char* data, ssize_t len = -1);

    enum Message {
        Resume
    };
    void sendMessage(Message msg);

private:
    enum TokenizeFlag {
        Tokenize_None = 0x0,
        Tokenize_CollapseWhitespace = 0x1,
        Tokenize_ExpandEnvironmentVariables = 0x2
    };
    List<Shell::Token> tokenize(String line, unsigned int flags, String &error) const;
    bool expandEnvironment(String &string, String &err) const;
    void process(const List<Shell::Token> &tokens);
    void processTokens(const List<Shell::Token>& tokens);
    void handleMessage(Message msg);
    enum CompletionResult {
        Completion_Refresh,
        Completion_Redisplay,
        Completion_Error
    };
    CompletionResult complete(const String &line, int cursor, String &insert);

    static void addPrev(List<Shell::Token> &tokens, const char *&last, const char *str, unsigned int flags);
    static void addArg(List<Shell::Token> &tokens, const char *&last, const char *str, unsigned int flags);
    static unsigned char elComplete(EditLine *el, int);
    static int getChar(EditLine *el, wchar_t *ch);
    static bool tokensAsJavaScript(List<Shell::Token>::const_iterator& token, const List<Shell::Token>::const_iterator& end, String& out);
    bool isUtf8() const { return mIsUtf8; }

    enum ProcessMode {
        ProcessStdin = 0x1
    };
    int processFiledescriptors(int mode = 0, wchar_t* ch = 0);

    void launchNode();

protected:
    virtual void run();

private:
    EditLine* mEl;
    String mBuffer;
    int mMsgPipe[2];
    int mStdoutPipe[2];
    bool mIsUtf8;
    Options mOptions;

    enum State {
        Normal,
        Waiting
    } mState;

    Process *mNodeProcess;
};

#endif
