#include "ffeventqueue.h"

FFEventQueue &FFEventQueue::getInstance()
{
    static FFEventQueue instance;
    return instance;
}

void FFEventQueue::enqueue(FFEvent *event)
{
    impl->enqueueFromSrc(event);
}

FFEvent *FFEventQueue::dequeue()
{
    return impl->dequeue();
}

void FFEventQueue::clearQueue()
{
    impl->clearQueue();
}

void FFEventQueue::wakeAllThread()
{
    impl->wakeAllThread();
}

FFEventQueue::~FFEventQueue()
{
    impl->wakeAllThread();
    impl->clearQueue();
    delete impl;
}

FFEventQueue::FFEventQueue()
    : impl(new FFBoundedQueue<FFEvent, FFEventTraits>())
{}
