#ifndef FFEVENTQUEUE_H
#define FFEVENTQUEUE_H

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>

class FFEvent;

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
    FFEventQueue()
        : m_stop(false)
    {}
    std::queue<FFEvent *> evQueue;
    std::condition_variable cond;
    std::mutex mutex;
    std::atomic<bool> m_stop;
};

#endif // FFEVENTQUEUE_H
