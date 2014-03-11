#include "NodeConnection.h"
#include <rct/Log.h>

NodeConnection::NodeConnection(const Path &socketFile)
    : mSocketFile(socketFile), mNodePendingMessageLength(0)
{
    mSocketClientReconnectTimer.timeout().connect(std::bind(&NodeConnection::reconnect, this));
    reconnect();
}

void NodeConnection::reconnect()
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
    mSocketClient->readyRead().connect(std::bind(&NodeConnection::onReadyRead, this, std::placeholders::_1));
}

void NodeConnection::onReadyRead(const SocketClient::SharedPtr &socket)
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
    if (length) {
        if (pos) {
            mNodeReadBuffer = mNodeReadBuffer.mid(pos);
        }
    } else {
        mNodeReadBuffer.clear();
    }
}

void NodeConnection::processNodeResponse(const char *data, int /* length */)
{
    Value nodeResponse = Value::fromJSON(data);
    error() << "Got response" << nodeResponse;
}

