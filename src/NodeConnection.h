#ifndef NODECONNECTION_H
#define NODECONNECTION_H

#include <rct/SocketClient.h>
#include <rct/Path.h>
#include <rct/String.h>
#include <rct/Timer.h>
#include <rct/Value.h>
#include <rct/SignalSlot.h>

class NodeConnection
{
public:
    NodeConnection(const Path &socketFile);
    bool isConnected() const { return mSocketClient && mSocketClient->isConnected(); }

    template <int StaticBufSize>
    bool send(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        const String ret = String::format<StaticBufSize>(format, args);
        va_end(args);
        return send(ret);
    }

    bool send(const String &msg)
    {
        assert(!msg.isEmpty());
        // ### need to convert to little endian if we're on big endian
        if (mSocketClient) {
            uint32_t size = msg.size();
            return (mSocketClient->write(reinterpret_cast<const unsigned char*>(&size), sizeof(size))
                    && mSocketClient->write(msg));
        }
        return false;
    }

    Signal<std::function<void(const Value&)> >& message() { return mMessage; }
private:
    void reconnect();
    void onReadyRead(const SocketClient::SharedPtr &);
    void processNodeResponse(const char *data, int length);

    const Path mSocketFile;
    SocketClient::SharedPtr mSocketClient;
    Timer mSocketClientReconnectTimer;
    String mNodeReadBuffer;
    uint32_t mNodePendingMessageLength;
    Signal<std::function<void(const Value&)> > mMessage;
};


#endif
