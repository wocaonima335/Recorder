#include "ffmuxerthread.h"

#include "encoder/ffaencoder.h"
#include "encoder/ffvencoder.h"

#include "muxer/ffmuxer.h"
#include "queue/ffapacketqueue.h"
#include "queue/ffpacket.h"
#include "queue/ffvpacketqueue.h"

#include "event/eventcategory.h"

#include <QtCore/QtGlobal>

#define CAPTURE_TIME 60

const double SYNC_THRESHOLD = 1.0 / 30.0; // 约 0.033

FFMuxerThread::FFMuxerThread()
{
    vTimeBase = {-1, -1};
    aTimeBase = {-1, -1};
}

FFMuxerThread::~FFMuxerThread() {}

void FFMuxerThread::init(FFAPacketQueue *aPktQueue_,
                         FFVPacketQueue *vPktQueue_,
                         FFMuxer *muxer_,
                         FFAEncoder *aEncoder_,
                         FFVEncoder *vEncoder_,
                         FFRecorder *recorderCtx_)
{
    aPktQueue = aPktQueue_;
    vPktQueue = vPktQueue_;

    aEncoder = aEncoder_;
    vEncoder = vEncoder_;

    muxer = muxer_;

    recorderCtx = recorderCtx_;
}

void FFMuxerThread::close()
{
    if (muxer) {
        muxer->close();
    }

    aTimeBase = {-1, -1};
    vTimeBase = {-1, -1};

    aEncoder = nullptr;
    vEncoder = nullptr;

    lastProcessTime = 0;
}

void FFMuxerThread::wakeAllThread()
{
    if (vPktQueue) {
        vPktQueue->wakeAllThread();
    }
    if (aPktQueue) {
        aPktQueue->wakeAllThread();
    }
}

void FFMuxerThread::run()
{
    muxer->writeHeader();

    FFPacket *audioPkt = nullptr;
    FFPacket *videoPkt = nullptr;

    while (!audioPkt)
        audioPkt = aPktQueue->dequeue();
    while (!videoPkt)
        videoPkt = vPktQueue->dequeue();

    AVPacket *aPacket = &audioPkt->packet;
    AVPacket *vPacket = &videoPkt->packet;
    aTimeBase = aEncoder->getCodecCtx()->time_base;
    vTimeBase = vEncoder->getCodecCtx()->time_base;

    auto ap2s = [&](const AVPacket *p) { return p->pts * av_q2d(aTimeBase); };
    auto vp2s = [&](const AVPacket *p) { return p->pts * av_q2d(vTimeBase); };

    while (!m_stop) {
        if (!audioPkt) {
            audioPkt = aPktQueue->dequeue();
            if (!audioPkt)
                break;
            aPacket = &audioPkt->packet;
        }
        if (!videoPkt) {
            videoPkt = vPktQueue->dequeue();
            if (!videoPkt)
                break;
            vPacket = &videoPkt->packet;
        }

        double asec = ap2s(aPacket);
        double vsec = vp2s(vPacket);
        int ret = 0;

        if (asec <= vsec + SYNC_THRESHOLD) {
            ret = muxer->mux(aPacket);
            FFPacketTraits::release(audioPkt);
            audioPkt = nullptr;
        } else {
            ret = muxer->mux(vPacket);
            FFPacketTraits::release(videoPkt);
            videoPkt = nullptr;
        }
        if (ret < 0) { // 错误退出时注意释放
            if (audioPkt)
                FFPacketTraits::release(audioPkt);
            if (videoPkt)
                FFPacketTraits::release(videoPkt);
            return;
        }
    }

    if (audioPkt)
        FFPacketTraits::release(audioPkt);
    if (videoPkt)
        FFPacketTraits::release(videoPkt);
    muxer->writeTrailer();
}

void FFMuxerThread::sendCaptureProcessEvent(double seconds)
{
  
}

double FFMuxerThread::calculateQueueDuration(FFAPacketQueue *queue, AVRational timeBase)
{
    if (!queue || timeBase.den <= 0) {
        return 0.0;
    }

    FFPacket *firstPkt = queue->peekQueue();
    FFPacket *lastPkt = queue->peekBack();

    if (!firstPkt || !lastPkt) {
        return 0.0;
    }

    int64_t firstPts = firstPkt->packet.pts;
    int64_t lastPts = lastPkt->packet.pts;
    int64_t lastDuration = lastPkt->packet.duration;
    if (firstPts == AV_NOPTS_VALUE || lastPts == AV_NOPTS_VALUE) {
        return 0.0;
    }

    double duration = (lastPts + lastDuration - firstPts) * av_q2d(timeBase);
    return duration > 0 ? duration : 0.0;
}

double FFMuxerThread::calculateQueueDuration(FFVPacketQueue *queue, AVRational timeBase)
{
    if (!queue || timeBase.den <= 0) {
        return 0.0;
    }

    FFPacket *firstPkt = queue->peekQueue();
    FFPacket *lastPkt = queue->peekBack();

    if (!firstPkt || !lastPkt) {
        return 0.0;
    }

    int64_t firstPts = firstPkt->packet.pts;
    int64_t lastPts = lastPkt->packet.pts;
    int64_t lastDuration = lastPkt->packet.duration;
    if (firstPts == AV_NOPTS_VALUE || lastPts == AV_NOPTS_VALUE) {
        return 0.0;
    }

    double duration = (lastPts + lastDuration - firstPts) * av_q2d(timeBase);
    return duration > 0 ? duration : 0.0;
}
