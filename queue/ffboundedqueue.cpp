#include "ffboundedqueue.h"

template<typename T, typename Traits>
FFBoundedQueue<T, Traits>::FFBoundedQueue(size_t maxSize)
    : m_stop(false)
    , m_maxSize(maxSize)
{}

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
bool FFBoundedQueue<T, Traits>::peekEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return q.empty();
}

template<typename T, typename Traits>
AVFrame *FFBoundedQueue<T, Traits>::peekQueue() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return q.empty() ? nullptr : q.front();
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
