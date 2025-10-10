#include "ffmuxerthread.h"

#include "encoder/ffaencoder.h"
#include "encoder/ffvencoder.h"

#include "muxer/ffmuxer.h"
#include "queue/ffapacketqueue.h"
#include "queue/ffpacket.h"
#include "queue/ffvpacketqueue.h"

#include <QtCore/QtGlobal>

#define CAPTURE_TIME 60

const double SYNC_THRESHOLD = 0.020;

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
    bool audioFinish = true;
    bool videoFinish = true;

    muxer->writeHeader();

    FFPacket *audioPkt = nullptr;
    FFPacket *videoPkt = nullptr;
    AVPacket *aPacket = nullptr;
    AVPacket *vPacket = nullptr;

    double audioPtsSec = 0;
    double videoPtsSec = 0;

    int ret = 0;
    while (!m_stop) {
        if (audioFinish) {
            std::lock_guard<std::mutex> lock(mutex);
            audioPkt = aPktQueue->dequeue();

            if (audioPkt == nullptr) {
                break; // 避免设置 m_stop，让基类统一管理
            }

            aPacket = &audioPkt->packet;
            if (aTimeBase.den == -1 && aTimeBase.num == -1) {
                aTimeBase = aEncoder->getCodecCtx()->time_base;
            }

            audioPtsSec = aPacket->pts * av_q2d(aTimeBase);

            if (audioPtsSec < 0) {
                // 修复：使用统一的释放方法
                FFPacketTraits::release(audioPkt);
                audioPkt = nullptr;
                audioFinish = true;
                continue;
            }
        }

        if (videoFinish) {
            std::lock_guard<std::mutex> lock(mutex);
            videoPkt = vPktQueue->dequeue();

            if (videoPkt == nullptr) {
                break; // 避免设置 m_stop
            }

            vPacket = &videoPkt->packet;

            if (vTimeBase.den == -1 && vTimeBase.num == -1) {
                vTimeBase = vEncoder->getCodecCtx()->time_base;
            }

            videoPtsSec = vPacket->pts * av_q2d(vTimeBase);

            if (videoPtsSec < 0) {
                // 修复：使用统一的释放方法
                FFPacketTraits::release(videoPkt);
                videoPkt = nullptr;
                videoFinish = true;
                continue;
            }
        }

        if (audioPtsSec + SYNC_THRESHOLD < videoPtsSec) {
            ret = muxer->mux(aPacket);
            if (ret < 0) {
                std::cerr << "Mux Audio Fail !" << std::endl;
                // 修复：错误时也要释放资源
                FFPacketTraits::release(audioPkt);
                FFPacketTraits::release(videoPkt);
                return;
            }

            // 修复：mux 成功后释放 FFPacket
            FFPacketTraits::release(audioPkt);
            audioPkt = nullptr;
            audioFinish = true;
            videoFinish = false;

        } else if (videoPtsSec + SYNC_THRESHOLD < audioPtsSec) {
            ret = muxer->mux(vPacket);
            if (ret < 0) {
                std::cerr << "Mux Video Fail !" << std::endl;
                // 修复：错误时也要释放资源
                FFPacketTraits::release(audioPkt);
                FFPacketTraits::release(videoPkt);
                return;
            }

            // 修复：mux 成功后释放 FFPacket
            FFPacketTraits::release(videoPkt);
            videoPkt = nullptr;
            videoFinish = true;
            audioFinish = false;

        } else {
            // 队列长度比较逻辑...
            double audioQueueDuration = calculateQueueDuration(aPktQueue, aTimeBase);
            double videoQueueDuration = calculateQueueDuration(vPktQueue, vTimeBase);

            if (audioQueueDuration > videoQueueDuration + 0.010) {
                ret = muxer->mux(aPacket);
                if (ret < 0) {
                    FFPacketTraits::release(audioPkt);
                    FFPacketTraits::release(videoPkt);
                    return;
                }
                FFPacketTraits::release(audioPkt);
                audioPkt = nullptr;
                audioFinish = true;
                videoFinish = false;
            } else {
                ret = muxer->mux(vPacket);
                if (ret < 0) {
                    FFPacketTraits::release(audioPkt);
                    FFPacketTraits::release(videoPkt);
                    return;
                }
                FFPacketTraits::release(videoPkt);
                videoPkt = nullptr;
                videoFinish = true;
                audioFinish = false;
            }
        }
    }

    // 修复：退出时清理剩余资源
    if (audioPkt) {
        FFPacketTraits::release(audioPkt);
    }
    if (videoPkt) {
        FFPacketTraits::release(videoPkt);
    }

    muxer->writeTrailer();
}
void FFMuxerThread::sendCaptureProcessEvent(double seconds)
{
    Q_UNUSED(seconds);
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
