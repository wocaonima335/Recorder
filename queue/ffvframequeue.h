#pragma

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

private:
    FFBoundedQueue<AVFrame, AVFrameTraits> impl;
};
