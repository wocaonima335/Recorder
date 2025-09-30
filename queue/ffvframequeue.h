#ifndef FFVFRAMEQUEUE_H
#define FFVFRAMEQUEUE_H

#include "ffboundedqueue.h"

class FFVFrameQueue
{
public:
    explicit FFVFrameQueue();
    ~FFVFrameQueue();

    void enqueue(AVFrame *srcFrame);
    AVFrame *dequeue();

    void wakeAllThread();
    void clearQueue();
    void enqueueNull();
    void flushQueue();
    void close();
    void start();
    bool peekEmpty();

private:
    AVFrame *peekQueue();
    FFBoundedQueue<AVFrame, AVFrameTraits> *impl;
};

#endif
