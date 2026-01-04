#include "ffaframequeue.h"

#define MAX_FRAME_SIZE 3

FFAFrameQueue::FFAFrameQueue()
    : impl(new FFBoundedQueue<AVFrame, AVFrameTraits>(30))
{}

FFAFrameQueue::~FFAFrameQueue()
{
    close();
}

void FFAFrameQueue::enqueue(AVFrame *srcFrame)
{
    impl->enqueueFromSrc(srcFrame);
}

void FFAFrameQueue::enqueueNull()
{
    impl->enqueueNull();
}

AVFrame *FFAFrameQueue::dequeue()
{
    return impl->dequeue();
}

void FFAFrameQueue::wakeAllThread()
{
    impl->wakeAllThread();
}

void FFAFrameQueue::clearQueue()
{
    impl->clearQueue();
}

void FFAFrameQueue::flushQueue()
{
    impl->flushQueue();
}

void FFAFrameQueue::close()
{
    wakeAllThread();
    clearQueue();
}

void FFAFrameQueue::start()
{
    impl->start();
}

AVFrame *FFAFrameQueue::peekQueue()
{
    return impl->peekQueue();
}
