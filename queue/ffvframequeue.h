#ifndef FFVFRAMEQUEUE_H
#define FFVFRAMEQUEUE_H

#include "ffboundedqueue.h"
#include <iostream>

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

    // 新增监控方法
    void printQueueStats() const;
    void resetStats();

private:
    AVFrame *peekQueue();
    FFBoundedQueue<AVFrame, AVFrameTraits> *impl;

    mutable std::atomic<size_t> m_enqueueCount{0};
    mutable std::atomic<size_t> m_dequeueCount{0};
    mutable std::chrono::steady_clock::time_point m_lastStatsTime;
};

#endif
