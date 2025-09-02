#include "ffadecoderthread.h"

#include "decoder/ffadecoder.h"
#include "queue/ffapacketqueue.h"
#include "queue/ffpacket.h"

FFADecoderThread::FFADecoderThread()
    : stopFlag(false)
{}

FFADecoderThread::~FFADecoderThread()
{
    if (aPktQueue) {
        delete aPktQueue;
        aPktQueue = nullptr;
    }

    if (aDecoder) {
        delete aDecoder;
        aDecoder = nullptr;
    }
}

void FFADecoderThread::init(FFADecoder *aDecoder_, FFAPacketQueue *aPktQueue_)
{
    aDecoder = aDecoder_;
    aPktQueue = aPktQueue_;
}

void FFADecoderThread::wakeAllThread()
{
    if (aPktQueue)
        aPktQueue->wakeAllThread();

    if (aDecoder)
        aDecoder->wakeAllThreads();
}

void FFADecoderThread::close()
{
    if (aDecoder) {
        aDecoder->close();
    }
    stopFlag.store(true, std::memory_order_release);
}

bool FFADecoderThread::peekStop()
{
    return stopFlag.load(std::memory_order_acquire);
}

void FFADecoderThread::run()
{
    while (!m_stop) {
        stopFlag.store(false, std::memory_order_release);
        FFPacket *pkt = aPktQueue->dequeue();
        if (pkt == nullptr) {
            continue;
        }

        if (pkt->serial != aPktQueue->getSerial()) {
            aPktQueue->flushQueue();
            aDecoder->flushQueue();

            aDecoder->flushDecoder();
        } else {
            if (pkt->type == NULLP && pkt->packet.data == nullptr) {
                aDecoder->decode(nullptr);
                aDecoder->enqueueNull();
            } else {
                aDecoder->decode(&pkt->packet);
            }
            av_packet_unref(&pkt->packet);
            av_freep(&pkt);
        }
    }
}
