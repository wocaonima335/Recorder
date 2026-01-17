#include "ffeventloop.h"
#include "ffevent.h"
#include "queue/ffeventqueue.h"
#include "thread/ffthreadpool.h"

FFEventLoop::FFEventLoop()
    : m_stop(true)
{}

void FFEventLoop::start()
{
    m_stop.store(false, std::memory_order_release);
    loopThread = std::thread(&FFEventLoop::work, this);
}

void FFEventLoop::stop()
{
    m_stop.store(true, std::memory_order_release);
    wakeAllThread();
}

void FFEventLoop::wait()
{
    if (loopThread.joinable()) {
        loopThread.join();
    }
}

void FFEventLoop::wakeAllThread()
{
    if (evQueue) {
        evQueue->wakeAllThread();
    }
}

void FFEventLoop::work()
{
    while (!m_stop) {
        FFEvent *event = evQueue->dequeue();
        if (!event) {
            continue;
        }

        threPool->submit([event]() mutable {
            qDebug() << "submit task";
            event->work();
            delete event;
        });
    }
}

FFEventLoop::~FFEventLoop()
{
    stop();
    // threPool由FFRecorder管理，此处不释放
}

void FFEventLoop::init(FFEventQueue *evQueue_, FFThreadPool *threPool_)
{
    evQueue = evQueue_;
    threPool = threPool_;
}
