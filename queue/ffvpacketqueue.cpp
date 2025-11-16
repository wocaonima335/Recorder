#include "ffvpacketqueue.h"
#include "ffpacket.h"

FFVPacketQueue::FFVPacketQueue()
    : serial(0)
    , impl(new FFBoundedQueue<FFPacket, FFPacketTraits>(30))

{}

FFVPacketQueue::~FFVPacketQueue()
{
    close();
    delete impl;
}

FFPacket *FFVPacketQueue::dequeue()
{
    return impl->dequeue();
}

FFPacket *FFVPacketQueue::peekQueue() const
{
    return impl->peekQueue();
}

FFPacket *FFVPacketQueue::peekBack() const
{
    return impl->peekBack();
}

void FFVPacketQueue::enqueue(AVPacket *pkt)
{
    FFPacket *ffpkt = static_cast<FFPacket *>(av_mallocz(sizeof(FFPacket)));
    av_packet_move_ref(&ffpkt->packet, pkt);
    ffpkt->serial = serial.load();
    ffpkt->type = NORMAL;
    impl->enqueueFromSrc(ffpkt);
}

void FFVPacketQueue::enqueueFlush()
{
    FFPacket *ffpkt = static_cast<FFPacket *>(av_mallocz(sizeof(FFPacket)));
    ffpkt->type = FLUSH;
    ffpkt->serial = serial++; // 与原实现保持一致
    impl->enqueueFromSrc(ffpkt);
}

void FFVPacketQueue::enqueueNull()
{
    FFPacket *ffpkt = static_cast<FFPacket *>(av_mallocz(sizeof(FFPacket)));
    ffpkt->type = NULLP;
    ffpkt->serial = serial.load();
    ffpkt->packet.data = nullptr;
    impl->enqueueFromSrc(ffpkt);
}

void FFVPacketQueue::flushQueue()
{
    while (true) {
        FFPacket *pkt = impl->peekQueue();
        if (pkt == nullptr || pkt->serial == serial.load()) {
            break;
        }
        pkt = impl->dequeue();
        if (!pkt)
            break;
        FFPacketTraits::release(pkt);
    }
}

bool FFVPacketQueue::tryEnqueue(AVPacket *pkt)
{
    FFPacket *ffpkt = static_cast<FFPacket *>(av_mallocz(sizeof(FFPacket)));
    av_packet_move_ref(&ffpkt->packet, pkt);
    ffpkt->serial = serial.load();
    ffpkt->type = NORMAL;
    return impl->tryEnqueueFromSrc(ffpkt);
}

FFPacket *FFVPacketQueue::tryDequeue()
{
    return impl->tryDequeue();
}

size_t FFVPacketQueue::getSerial()
{
    return serial.load();
}

void FFVPacketQueue::close()
{
    impl->close();
}

void FFVPacketQueue::start()
{
    impl->start();
}

int FFVPacketQueue::length()
{
    return impl->length();
}

void FFVPacketQueue::wakeAllThread()
{
    impl->wakeAllThread();
}

void FFVPacketQueue::clearQueue()
{
    impl->clearQueue();
}
