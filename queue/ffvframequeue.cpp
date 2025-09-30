#include "ffvframequeue.h"

#define MAX_FRAME_SIZE 2

FFVFrameQueue::FFVFrameQueue()
    : impl(new FFBoundedQueue<AVFrame, AVFrameTraits>())
{}

FFVFrameQueue::~FFVFrameQueue()
{
    close();
}

void FFVFrameQueue::enqueue(AVFrame *srcFrame)
{
    impl->enqueueFromSrc(srcFrame);
}

AVFrame *FFVFrameQueue::dequeue()
{
    return impl->dequeue();
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
