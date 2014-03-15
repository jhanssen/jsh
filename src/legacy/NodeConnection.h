#ifndef NODECONNECTION_H
#define NODECONNECTION_H

#include <rct/String.h>

class NodeConnection
{
public:
    NodeConnection(int socketFd);
    ~NodeConnection();

    bool send(uint32_t msg);
    bool send(const String &msg);

private:
    int mFd;
};

#endif
