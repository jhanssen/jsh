#include "NodeJS.h"
#include <rct/Log.h>

NodeJS::NodeJS()
    : mFlags(0), mNodePendingMessageLength(0), mNodeProcess(0)
{
    mSocketClientReconnectTimer.timeout().connect(std::bind(&NodeJS::reconnect, this));
}

NodeJS::~NodeJS()
{
    if (mNodeProcess) {
        mNodeProcess->kill();
        delete mNodeProcess;
    }
}

bool NodeJS::init(const Path &socketFile, unsigned int flags)
{
    mFlags = flags;
    mSocketFile = socketFile;
    if (flags & Autostart) {
        mNodeProcess = new Process;
        List<String> args;
        args << JSH_DOT_JS << String::format<128>("--socket-file=%s", socketFile.constData());
        if (!mNodeProcess->start("node", args) && !mNodeProcess->start("nodejs", args)) {
            error("Can't launch nodejs %s", mNodeProcess->errorString().constData());
            delete mNodeProcess;
            mNodeProcess = 0;
            return false;
        }
    }
    reconnect();
}

void NodeJS::reconnect()
{
    mSocketClientReconnectTimer.stop();
    if (mSocketClient)
        return;
    mSocketClient = std::make_shared<SocketClient>();
    if (!mSocketClient->connect(Path::home() + "/.jsh-socket")) {
        mSocketClient.reset();
        mSocketClientReconnectTimer.restart(5000, Timer::SingleShot);
        return;
    }
    mSocketClient->disconnected().connect([this](const SocketClient::SharedPtr &) { mSocketClient.reset(); reconnect(); });
    mSocketClient->readyRead().connect(std::bind(&NodeJS::onReadyRead, this, std::placeholders::_1));
}

void NodeJS::onReadyRead(const SocketClient::SharedPtr &socket)
{
    assert(socket == mSocketClient);
    const Buffer buffer = std::move(socket->takeBuffer());
    mNodeReadBuffer.append(reinterpret_cast<const char*>(buffer.data()), buffer.size());

    int length = mNodeReadBuffer.length();
    int pos = 0;
    while (true) {
        if (!mNodePendingMessageLength) {
            if (length < sizeof(mNodePendingMessageLength)) {
                break;
            }
            mNodePendingMessageLength = *reinterpret_cast<const uint32_t*>(&mNodeReadBuffer[pos]);
            pos += sizeof(uint32_t);
            length -= sizeof(uint32_t);
        }
        if (length >= mNodePendingMessageLength) {
            char tmp = '\0';
            std::swap(mNodeReadBuffer[pos + mNodePendingMessageLength], tmp);
            processNodeResponse(mNodeReadBuffer.constData() + pos, mNodePendingMessageLength);
            std::swap(mNodeReadBuffer[pos + mNodePendingMessageLength], tmp);
            length -= mNodePendingMessageLength;
            pos += mNodePendingMessageLength;
            mNodePendingMessageLength = 0;
        } else {
            break;
        }
    }
    if (!length) {
        mNodeReadBuffer.clear();
    } else if (pos) {
        mNodeReadBuffer = mNodeReadBuffer.mid(pos);
    }
}

void NodeJS::processNodeResponse(const char *data, int /* length */)
{
    Value nodeResponse = Value::fromJSON(data);
    error() << "Got response" << nodeResponse;
}

Value NodeJS::evaluate(const String &script)
{

}

Value NodeJS::call(const String &object, const String &function, const List<Value> &args, bool *ok)
{

}

bool NodeJS::checkSyntax(const String &code)
{

}
