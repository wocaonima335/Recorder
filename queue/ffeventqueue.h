#ifndef FFEVENTQUEUE_H
#define FFEVENTQUEUE_H

#include "event/ffevent.h"

struct FFEventTraits
{
    static FFEvent *allocateFromSrc(FFEvent *src) { return src; }
    static void releaseSrc(FFEvent * /*src*/) {}
    static FFEvent *allocateNull() { return nullptr; }
    static void release(FFEvent *&ev)
    {
        if (ev) {
            delete ev;
            ev = nullptr;
        }
    }
    static bool isNull(const FFEvent *ev) { return ev == nullptr; }
};

class FFEventQueue final
{
public:
    static FFEventQueue &getInstance();

    FFEventQueue(const FFEventQueue &) = delete;
    FFEventQueue &operator=(const FFEventQueue &) = delete;

    void enqueue(FFEvent *event);
    FFEvent *dequeue();
    void clearQueue();
    void wakeAllThread();

    ~FFEventQueue();

private:
    FFEventQueue();
    FFBoundedQueue<FFEvent, FFEventTraits> *impl;
};

#endif // FFEVENTQUEUE_H
