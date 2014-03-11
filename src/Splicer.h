#ifndef SPLICER_H
#define SPLICER_H

class Splicer
{
public:
    typedef void (*ClosedCallback)(void*, int, int);

    static void splice(int from, int to, ClosedCallback callback, void* userdata);
};

#endif
