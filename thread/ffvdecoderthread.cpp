#include "ffvdecoderthread.h"
#include "decoder/ffvdecoder.h"
#include "queue/ffeventqueue.h"
#include "queue/ffpacket.h"
#include "queue/ffvpacketqueue.h"

FFVDecoderThread::FFVDecoderThread()
{
    stopFlag = true;
}

FFVDecoderThread::~FFVDecoderThread()
{
    if (vPktQueue) {
        delete vPktQueue;
        vPktQueue = nullptr;
    }
    if (vDecoder) {
        delete vDecoder;
        vDecoder = nullptr;
    }
}

void FFVDecoderThread::init(FFVDecoder *vDecoder_, FFVPacketQueue *vPktQueue_)
{
    vDecoder = vDecoder_;
    vPktQueue = vPktQueue_;
}

void FFVDecoderThread::wakeAllThread()
{
    if (vPktQueue) {
        vPktQueue->wakeAllThread();
    }
    if (vDecoder) {
        vDecoder->wakeAllThread();
    }
}

void FFVDecoderThread::close()
{
    if (vDecoder) {
        vDecoder->close();
    }
    stopFlag.store(true, std::memory_order_release);
}

bool FFVDecoderThread::peekStop()
{
    return stopFlag.load(std::memory_order_acquire);
}

void FFVDecoderThread::run()
{
    while (!m_stop) {
        FFPacket *pkt = vPktQueue->dequeue();
        if (pkt == nullptr) {
            continue;
        }

        if (pkt->serial != vPktQueue->getSerial()) {
            vPktQueue->flushQueue();
            vDecoder->flushQueue();
            vDecoder->flushDecoder();
        } else {
            if (pkt->type == NULLP && pkt->packet.data == nullptr) {
                vDecoder->decode(nullptr);

                vDecoder->enqueueNull();
            } else {
                vDecoder->decode(&pkt->packet);
            }
            av_packet_unref(&pkt->packet);
            av_freep(&pkt->packet);
        }
    }
}
