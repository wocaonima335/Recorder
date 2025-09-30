#ifndef FFAFRAMEQUEUE_H
#define FFAFRAMEQUEUE_H
#include "ffboundedqueue.h"

class FFAFrameQueue
{
public:
    explicit FFAFrameQueue();
    ~FFAFrameQueue();

    void enqueue(AVFrame *srcFrame);
    void enqueueNull();
    AVFrame *dequeue();

    void wakeAllThread();
    void clearQueue();
    void flushQueue();
    void close();
    void start();
    bool peekEmpty();

private:
    AVFrame *peekQueue();
    FFBoundedQueue<AVFrame, AVFrameTraits> *impl;
};
#endif
