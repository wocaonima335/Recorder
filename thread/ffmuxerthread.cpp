#include "ffmuxerthread.h"

#include "encoder/ffaencoder.h"
#include "encoder/ffvencoder.h"

#include "muxer/ffmuxer.h"
#include "queue/ffapacketqueue.h"
#include "queue/ffeventqueue.h"
#include "queue/ffpacket.h"
#include "queue/ffvpacketqueue.h"

#include "event/abstracteventfactory.h"
#include "event/eventcategory.h"
#include "event/eventfactorymanager.h"

#include <QtCore/QtGlobal>
#include <cstdio>
#include <chrono>

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
    if (muxer)
    {
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
    if (vPktQueue)
    {
        vPktQueue->wakeAllThread();
    }
    if (aPktQueue)
    {
        aPktQueue->wakeAllThread();
    }
}

void FFMuxerThread::run()
{
    fprintf(stderr, "[MuxThread] run() start. Waiting header write...\n");
    muxer->writeHeader();

    // 先分别拿到首包
    auto dequeue_first = [this](auto *q) -> FFPacket *
    {
        FFPacket *pkt = nullptr;
        while (!pkt && !m_stop)
        {
            pkt = q->dequeue();
        }
        return pkt;
    };

    FFPacket *audioPkt = dequeue_first(aPktQueue);
    FFPacket *videoPkt = dequeue_first(vPktQueue);
    if (!audioPkt || !videoPkt)
        return;

    AVPacket *aPacket = &audioPkt->packet;
    AVPacket *vPacket = &videoPkt->packet;

    const AVRational aTB = aEncoder->getCodecCtx()->time_base;
    const AVRational vTB = vEncoder->getCodecCtx()->time_base;
    const AVRational vFPS = vEncoder->getCodecCtx()->framerate.num > 0
                                ? vEncoder->getCodecCtx()->framerate
                                : AVRational{30, 1};
    // 视频一帧的 PTS 步长（视频时基下）
    int64_t vFrameDur = av_rescale_q(1, av_inv_q(vFPS), vTB);

    // 用首个视频帧的 PTS 作为基准，丢弃过早的音频帧
    int64_t vPts0 = vPacket->pts;
    while (!m_stop && aPacket &&
           av_compare_ts(aPacket->pts, aTB, vPts0 - vFrameDur, vTB) < 0)
    {
        // 丢弃过早的音频包
        FFPacketTraits::release(audioPkt);
        audioPkt = aPktQueue->dequeue();
        if (!audioPkt)
            break;
        aPacket = &audioPkt->packet;
    }
    if (!audioPkt)
        return;

    const AVRational usecTB{1, 1000000};
    AdaptiveSync adaptSync;
    auto ptsToSec = [](int64_t pts, AVRational tb) -> double
    {
        if (pts == AV_NOPTS_VALUE)
            return NAN;
        return pts * av_q2d(tb);
    };

    fprintf(stderr,
            "[MuxThread] time_base: audio=%d/%d, video=%d/%d\n",
            aTB.num, aTB.den, vTB.num, vTB.den);

    while (!m_stop)
    {
        const double aSec = ptsToSec(aPacket->pts, aTB);
        const double vSec = ptsToSec(vPacket->pts, vTB);

        double thrSec = (adaptSync.samples() < 10) ? SYNC_THRESHOLD
                                                   : adaptSync.calculateDynamicThreshold(aSec, vSec);
        thrSec = std::min(thrSec, 0.030);
        const int64_t thrUsec = (int64_t)llround(thrSec * 1000000.0);
        const int64_t thrAPts = av_rescale_q(thrUsec, usecTB, aTB);
        const int64_t thrVPts = av_rescale_q(thrUsec, usecTB, vTB);

        bool chooseAudio = false;
        if (aPacket->pts == AV_NOPTS_VALUE && vPacket->pts == AV_NOPTS_VALUE)
        {
            chooseAudio = true;
        }
        else if (aPacket->pts == AV_NOPTS_VALUE)
        {
            chooseAudio = false;
        }
        else if (vPacket->pts == AV_NOPTS_VALUE)
        {
            chooseAudio = true;
        }
        else
        {
            chooseAudio = av_compare_ts(aPacket->pts - thrAPts, aTB, vPacket->pts, vTB) <= 0;
        }

        int ret;
        double processedSec;
        if (chooseAudio)
        {
            ret = muxer->mux(aPacket);
            processedSec = aSec;
            FFPacketTraits::release(audioPkt);
            audioPkt = aPktQueue->dequeue();
            if (!audioPkt)
                break;
            aPacket = &audioPkt->packet;
        }
        else
        {
            ret = muxer->mux(vPacket);
            processedSec = vSec;
            FFPacketTraits::release(videoPkt);
            videoPkt = vPktQueue->dequeue();
            if (!videoPkt)
                break;
            vPacket = &videoPkt->packet;
        }

        if (processedSec - lastProcessTime >= 0.01)
        {
            sendCaptureProcessEvent(processedSec);
            lastProcessTime = processedSec;
        }
        if (ret < 0)
        {
            fprintf(stderr, "[MuxThread] muxer->mux() returned error=%d, breaking.\n", ret);
            break;
        }
    }

    if (audioPkt)
        FFPacketTraits::release(audioPkt);
    if (videoPkt)
        FFPacketTraits::release(videoPkt);
    fprintf(stderr, "[MuxThread] loop end, writing trailer.\n");
    muxer->writeTrailer();
}

void FFMuxerThread::sendCaptureProcessEvent(double seconds)
{
    ProcessEventParams params;
    params.type = ProcessEventType::CAPTURE_PROCESS;
    params.curSec = seconds;
    auto ev = EventFactoryManager::getInstance().createEvent(EventCategory::PROCESS,
                                                             recorderCtx,
                                                             params);
    FFEventQueue::getInstance().enqueue(ev.release());
}

double FFMuxerThread::calculateQueueDuration(FFAPacketQueue *queue, AVRational timeBase)
{
    if (!queue || timeBase.den <= 0)
    {
        return 0.0;
    }

    FFPacket *firstPkt = queue->peekQueue();
    FFPacket *lastPkt = queue->peekBack();

    if (!firstPkt || !lastPkt)
    {
        return 0.0;
    }

    int64_t firstPts = firstPkt->packet.pts;
    int64_t lastPts = lastPkt->packet.pts;
    int64_t lastDuration = lastPkt->packet.duration;
    if (firstPts == AV_NOPTS_VALUE || lastPts == AV_NOPTS_VALUE)
    {
        return 0.0;
    }

    double duration = (lastPts + lastDuration - firstPts) * av_q2d(timeBase);
    return duration > 0 ? duration : 0.0;
}

double FFMuxerThread::calculateQueueDuration(FFVPacketQueue *queue, AVRational timeBase)
{
    if (!queue || timeBase.den <= 0)
    {
        return 0.0;
    }

    FFPacket *firstPkt = queue->peekQueue();
    FFPacket *lastPkt = queue->peekBack();

    if (!firstPkt || !lastPkt)
    {
        return 0.0;
    }

    int64_t firstPts = firstPkt->packet.pts;
    int64_t lastPts = lastPkt->packet.pts;
    int64_t lastDuration = lastPkt->packet.duration;
    if (firstPts == AV_NOPTS_VALUE || lastPts == AV_NOPTS_VALUE)
    {
        return 0.0;
    }

    double duration = (lastPts + lastDuration - firstPts) * av_q2d(timeBase);
    return duration > 0 ? duration : 0.0;
}
