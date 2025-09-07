#include "ffvmuxerthread.h"
#include "encoder/ffvencoder.h"
#include "muxer/ffmuxer.h"
#include "queue/ffpacket.h"
#include "queue/ffvpacketqueue.h"

FFVMuxerThread::FFVMuxerThread() {}

FFVMuxerThread::~FFVMuxerThread() {}

void FFVMuxerThread::init(FFVPacketQueue *pktQueue_, FFMuxer *muxer_)
{
    pktQueue = pktQueue_;
    muxer = muxer_;
}

void FFVMuxerThread::run()
{
    bool write[2] = {false, false};
    while (!m_stop) {
        FFPacket *pkt = pktQueue->dequeue();
        if (!pkt) {
            continue;
        }
        AVPacket *packet = &pkt->packet;

        if (packet == nullptr) {
            continue;
        }
        if (packet->data == nullptr) {
            av_packet_unref(packet);
            av_packet_free(&packet);
            continue;
        }
        if (!write[0]) {
            muxer->writeHeader();
            write[0] = true;
        }

        int ret = muxer->mux(packet);
        if (!write[1]) {
            write[1] = true;
        }

        if (ret < 0) {
            m_stop = true;
        }

        av_packet_unref(packet);
        av_packet_free(&packet);
    }
    if (write[0] && write[1]) {
        muxer->writeTrailer();
    }
}

void FFVMuxerThread::initMuxer() {}
