#ifndef CHAIN_H
#define CHAIN_H

#include <rct/String.h>
#include <rct/SignalSlot.h>
#include <memory>

class Chain
{
public:
    typedef std::shared_ptr<Chain> SharedPtr;
    typedef std::weak_ptr<Chain> WeakPtr;

    Chain() : mIsComplete(false), mPrevComplete(false), mErrToOut(false), mNext(0) { }
    virtual ~Chain() { delete mNext; }

    void setStdErrToStdOut(bool errToOut) { mErrToOut = true; }
    void chain(Chain* chain) { assert(!mNext); mNext = chain; chain->init(this); }

    void finalize();

    Signal<std::function<void(String&&)> >& finishedStdOut() { return mFinishedStdOut; };
    Signal<std::function<void(String&&)> >& finishedStdErr() { return mFinishedStdErr; };
    Signal<std::function<void()> >& complete() { return mComplete; }

protected:
    Signal<std::function<void(String&&)> >& stdout() { return mStdOut; }
    Signal<std::function<void(String&&)> >& stderr() { return mStdErr; }
    Signal<std::function<void(Chain*)> >& closed() { return mClosed; }

protected:
    virtual void init(Chain* previous) = 0;
    virtual void notifyIsFirst() { mPrevComplete = true; }
    virtual void exec() { }

    bool errIsOut() const { return mErrToOut; }

protected:
    bool mIsComplete, mPrevComplete;

private:
    void lastStdOut(String&& stdout);
    void lastStdErr(String&& stderr);
    void lastClosed(Chain*);

private:
    Signal<std::function<void(String&&)> > mStdOut, mStdErr;
    Signal<std::function<void(String&&)> > mFinishedStdOut, mFinishedStdErr;
    Signal<std::function<void(Chain*)> > mClosed;
    Signal<std::function<void()> > mComplete;
    bool mErrToOut;

    Chain* mNext;
    friend class ChainProcess;
    friend class ChainJavaScript;
};

#endif
