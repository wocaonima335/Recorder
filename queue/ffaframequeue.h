#ifndef FFAFRAMEQUEUE_H
#define FFAFRAMEQUEUE_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

extern "C" {
#include <libavformat/avformat.h>
}

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

private:
    AVFrame *peekQueue();
    static void freeFrame(AVFrame *frame);

private:
    std::queue<AVFrame *> frmQueue;
    std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> m_stop;
};

#endif // FFAFRAMEQUEUE_H
