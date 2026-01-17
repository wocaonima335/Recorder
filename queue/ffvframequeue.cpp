#include "ffvframequeue.h"

#define MAX_FRAME_SIZE 2

FFVFrameQueue::FFVFrameQueue()
    : impl(new FFBoundedQueue<AVFrame, AVFrameTraits>(30))
{}

FFVFrameQueue::~FFVFrameQueue()
{
    close();
}

void FFVFrameQueue::enqueue(AVFrame *srcFrame)
{
    // auto start = std::chrono::steady_clock::now();
    impl->enqueueFromSrc(srcFrame);
    // auto end = std::chrono::steady_clock::now();

    m_enqueueCount.fetch_add(1, std::memory_order_relaxed);
}

AVFrame *FFVFrameQueue::dequeue()
{
    // auto start = std::chrono::steady_clock::now();
    AVFrame *frame = impl->dequeue();
    // auto end = std::chrono::steady_clock::now();

    // if (frame) {
    //     m_dequeueCount.fetch_add(1, std::memory_order_relaxed);
    // }

    // auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // if (wait_ms > 50) { // 超过50ms警告
    //     std::cerr << "[VFrameQueue] dequeue blocked for " << wait_ms
    //               << "ms, queue_len=" << impl->length() << std::endl;
    // }

    return frame;
}

void FFVFrameQueue::wakeAllThread()
{
    impl->wakeAllThread();
}

void FFVFrameQueue::clearQueue()
{
    impl->clearQueue();
}

void FFVFrameQueue::enqueueNull()
{
    impl->enqueueNull();
}

void FFVFrameQueue::flushQueue()
{
    impl->flushQueue();
}

void FFVFrameQueue::close()
{
    impl->close();
}

void FFVFrameQueue::start()
{
    impl->start();
}

bool FFVFrameQueue::peekEmpty()
{
    return impl->peekEmpty();
}

AVFrame *FFVFrameQueue::peekQueue()
{
    return impl->peekQueue();
}

void FFVFrameQueue::printQueueStats() const
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastStatsTime).count();

    if (elapsed >= 5) { // 每5秒打印一次
        size_t enqueued = m_enqueueCount.load();
        size_t dequeued = m_dequeueCount.load();

        qDebug() << "[VFrameQueue] Stats: enqueued=" << enqueued << ", dequeued=" << dequeued
                 << ", queue_len=" << impl->length();

        m_lastStatsTime = now;
    }
}
