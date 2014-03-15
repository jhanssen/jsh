#ifndef SPLICER_H
#define SPLICER_H

#include <functional>

class Splicer
{
public:
    static void splice(int from, int to);

    enum ErrorType { ErrorFrom, ErrorTo };
    static unsigned int addCloseCallback(std::function<void(int)>&& cb);
    static unsigned int addErrorCallback(std::function<void(ErrorType, int, int)>&& cb);
    static void removeCloseCallback(unsigned int id);
    static void removeErrorCallback(unsigned int id);

private:
};

#endif
