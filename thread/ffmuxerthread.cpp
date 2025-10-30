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

    // 预计算时间基准转换因子
    const double audioTimeBaseFactor = av_q2d(aEncoder->getCodecCtx()->time_base);
    const double videoTimeBaseFactor = av_q2d(vEncoder->getCodecCtx()->time_base);

    // 获取初始包
    while (!audioPkt && !m_stop)
        audioPkt = aPktQueue->dequeue();
    while (!videoPkt && !m_stop)
        videoPkt = vPktQueue->dequeue();

    if (!audioPkt || !videoPkt)
        return;

    AVPacket *aPacket = &audioPkt->packet;
    AVPacket *vPacket = &videoPkt->packet;

    // 主循环 - 内联优化
    while (!m_stop) {
        // 快速路径：直接计算时间，避免重复函数调用
        const double audioTime = aPacket->pts * audioTimeBaseFactor;
        const double videoTime = vPacket->pts * videoTimeBaseFactor;

        int ret;
        double processedTime;

        if (audioTime <= videoTime + SYNC_THRESHOLD) {
            ret = muxer->mux(aPacket);
            processedTime = audioTime;
            FFPacketTraits::release(audioPkt);
            audioPkt = aPktQueue->dequeue();
            if (!audioPkt)
                break;
            aPacket = &audioPkt->packet;
        } else {
            ret = muxer->mux(vPacket);
            processedTime = videoTime;
            FFPacketTraits::release(videoPkt);
            videoPkt = vPktQueue->dequeue();
            if (!videoPkt)
                break;
            vPacket = &videoPkt->packet;
        }

        // 批量进度更新（减少事件发送频率）
        if (processedTime - lastProcessTime >= 0.01) {
            sendCaptureProcessEvent(processedTime);
            lastProcessTime = processedTime;
        }

        if (ret < 0)
            break; // 快速错误退出
    }

    // 清理
    if (audioPkt)
        FFPacketTraits::release(audioPkt);
    if (videoPkt)
        FFPacketTraits::release(videoPkt);
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
