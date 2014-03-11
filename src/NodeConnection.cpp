#include "NodeConnection.h"
#include "Splicer.h"
#include <rct/Log.h>

NodeConnection::NodeConnection(int socketFd)
    : mFd(socketFd)
{
}

NodeConnection::~NodeConnection()
{
}

static inline bool sendData(int socketFd, const char* data, int size)
{
    do {
        const int w = ::write(socketFd, data, size);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        assert(size >= w);
        size -= w;
        data += w;
    } while (size);
    return true;
}

bool NodeConnection::send(uint32_t msg)
{
    return sendData(mFd, reinterpret_cast<char*>(&msg), sizeof(uint32_t));
}

bool NodeConnection::send(const String &msg)
{
    return (send(msg.size()) && sendData(mFd, msg.constData(), msg.size()));
}
