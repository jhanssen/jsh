#ifndef CHAIN_H
#define CHAIN_H

#include <rct/String.h>
#include <rct/SignalSlot.h>

class Chain
{
public:
    Chain() : mErrToOut(false), mNext(0) { }
    virtual ~Chain() { }

    void setStdErrToStdOut(bool errToOut) { mErrToOut = true; }
    void chain(Chain* chain) { assert(!mNext); mNext = chain; chain->init(this); }

    void exec();

    Signal<std::function<void(String&&)> >& finishedStdOut() { return mFinishedStdOut; };
    Signal<std::function<void(String&&)> >& finishedStdErr() { return mFinishedStdErr; };

protected:
    Signal<std::function<void(String&&)> >& stdout() { return mStdOut; }
    Signal<std::function<void(String&&)> >& stderr() { return mStdErr; }
    Signal<std::function<void()> >& closed() { return mClosed; }

protected:
    virtual void init(Chain* previous) = 0;

    bool errIsOut() const { return mErrToOut; }

private:
    void lastStdOut(String&& stdout);
    void lastStdErr(String&& stderr);

private:
    Signal<std::function<void(String&&)> > mStdOut, mStdErr;
    Signal<std::function<void(String&&)> > mFinishedStdOut, mFinishedStdErr;
    Signal<std::function<void()> > mClosed;
    bool mErrToOut;

    Chain* mNext;
    friend class ChainProcess;
};

#endif
