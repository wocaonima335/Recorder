#ifndef FFBOUNDEDQUEUE_H
#define FFBOUNDEDQUEUE_H

#include "ffpacket.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <QDebug>
#include <atomic>
#include <condition_variable>
#include <corecrt.h>
#include <mutex>
#include <queue>

using namespace std;

extern "C" {
#include <libavformat/avformat.h>
}

struct FFPacketTraits
{
    static FFPacket *allocateFromSrc(FFPacket *src) { return src; }

    static void releaseSrc(FFPacket * /*src*/)
    {
        // no-op
    }

    static FFPacket *allocateNull()
    {
        FFPacket *pkt = static_cast<FFPacket *>(av_mallocz(sizeof(FFPacket)));
        pkt->type = NULLP;
        pkt->serial = 0; // 实际 serial 由外部设置更合适
        return pkt;
    }

    static void release(FFPacket *&pkt)
    {
        if (pkt) {
            av_packet_unref(&pkt->packet);
            av_freep(&pkt);
            pkt = nullptr;
        }
    }

    static bool isNull(const FFPacket *pkt) { return pkt && pkt->type == NULLP; }
};

struct AVFrameTraits
{
    static AVFrame *allocateFromSrc(AVFrame *src)
    {
        if (!src)
            return nullptr;
        AVFrame *dest = av_frame_alloc();
        if (!dest)
            return nullptr;
        av_frame_move_ref(dest, src);
        av_frame_unref(src);
        return dest;
    }

    static void releaseSrc(AVFrame *src)
    {
        if (src)
            av_frame_unref(src);
    }

    static AVFrame *allocateNull()
    {
        AVFrame *f = av_frame_alloc();
        if (!f)
            return nullptr;
        f->data[0] = nullptr;
        f->data[1] = nullptr;
        f->data[2] = nullptr;
        return f;
    }

    static void release(AVFrame *&frame)
    {
        if (frame) {
            av_frame_unref(frame);
            av_frame_free(&frame);
        }
    }

    static bool isNull(const AVFrame *frame)
    {
        if (!frame)
            return false;
        return frame->data[0] == nullptr && frame->data[1] == nullptr && frame->data[2] == nullptr;
    }
};

template<typename T, typename Traits>

class FFBoundedQueue
{
public:
    explicit FFBoundedQueue(size_t maxSize = 3);

    void start();
    void wakeAllThread();
    void close();
    void enqueueFromSrc(T *src);
    bool tryEnqueueFromSrc(T *src);
    void enqueueNull();
    T *dequeue();
    T *tryDequeue();
    bool peekEmpty() const;
    T *peekQueue() const;
    T *peekBack() const;
    size_t length() const;
    static bool isNull(const T *item);
    void clearQueue();
    void flushQueue();

private:
    mutable std::mutex mutex;
    std::condition_variable cond;
    std::queue<T *> q;
    std::atomic<bool> m_stop;
    size_t m_maxSize;
};

template<typename T, typename Traits>
FFBoundedQueue<T, Traits>::FFBoundedQueue(size_t maxSize)
    : m_maxSize(maxSize)
{
    m_stop.store(false, std::memory_order_release);
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::start()
{
    m_stop.store(false, std::memory_order_release);
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::wakeAllThread()
{
    m_stop.store(true, std::memory_order_release);
    cond.notify_all();
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::close()
{
    wakeAllThread();
    clearQueue();
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::enqueueFromSrc(T *src)
{
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [this]() { return q.size() < m_maxSize || m_stop.load(memory_order_acquire); });

    if (m_stop.load(std::memory_order_acquire)) {
        Traits::releaseSrc(src);
        return;
    }
    T *dest = Traits::allocateFromSrc(src); // 由 Traits 执行 move/ref
    if (!dest) {
        Traits::releaseSrc(src);
        return;
    }
    q.push(dest);
    cond.notify_one();
}

template<typename T, typename Traits>
bool FFBoundedQueue<T, Traits>::tryEnqueueFromSrc(T *src)
{
    std::unique_lock<std::mutex> lock(mutex);

    // 检查是否已停止
    if (m_stop.load(std::memory_order_acquire)) {
        Traits::releaseSrc(src);
        return false;
    }

    // 检查队列是否已满
    if (q.size() >= m_maxSize) {
        return false; // 队列已满，不阻塞直接返回
    }

    T *dest = Traits::allocateFromSrc(src);
    if (!dest) {
        Traits::releaseSrc(src);
        return false;
    }

    q.push(dest);
    cond.notify_one();
    return true;
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::enqueueNull()
{
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock,
              [this]() { return q.size() < m_maxSize || m_stop.load(std::memory_order_acquire); });
    if (m_stop.load(std::memory_order_acquire)) {
        return;
    }
    T *item = Traits::allocateNull();
    if (item) {
        q.push(item);
        cond.notify_one();
    }
}

template<typename T, typename Traits>
T *FFBoundedQueue<T, Traits>::dequeue()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (m_stop.load(std::memory_order_acquire)) {
        return nullptr;
    }
    cond.wait(lock, [this]() { return !q.empty() || m_stop.load(std::memory_order_acquire); });

    if (m_stop.load(std::memory_order_acquire)) {
        return nullptr;
    }

    T *item = q.front();
    q.pop();
    cond.notify_one();
    return item;
}

template<typename T, typename Traits>
inline T *FFBoundedQueue<T, Traits>::tryDequeue()
{
    std::unique_lock<std::mutex> lock(mutex);
    if (m_stop.load(std::memory_order_acquire)) {
        return nullptr;
    }
    if (q.empty()) {
        return nullptr;
    }
    T *item = q.front();
    q.pop();
    cond.notify_one();
    return item;
}

template<typename T, typename Traits>
bool FFBoundedQueue<T, Traits>::peekEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return q.empty();
}

template<typename T, typename Traits>
T *FFBoundedQueue<T, Traits>::peekQueue() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return q.empty() ? nullptr : q.front();
}

template<typename T, typename Traits>
T *FFBoundedQueue<T, Traits>::peekBack() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return q.empty() ? nullptr : q.back();
}

template<typename T, typename Traits>
size_t FFBoundedQueue<T, Traits>::length() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return q.size();
}

template<typename T, typename Traits>
bool FFBoundedQueue<T, Traits>::isNull(const T *item)
{
    return Traits::isNull(item);
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::clearQueue()
{
    std::lock_guard<std::mutex> lock(mutex);
    while (!q.empty()) {
        T *item = q.front();
        q.pop();
        Traits::release(item);
    }
}

template<typename T, typename Traits>
void FFBoundedQueue<T, Traits>::flushQueue()
{
    std::lock_guard<std::mutex> lock(mutex);
    while (!q.empty()) {
        T *item = q.front();
        q.pop();
        Traits::release(item);
    }
    cond.notify_one();
}

#endif
