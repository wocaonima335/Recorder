#include "ffmuxerthread.h"

#include "encoder/ffaencoder.h"
#include "encoder/ffvencoder.h"

#include "muxer/ffmuxer.h"
#include "queue/ffapacketqueue.h"
#include "queue/ffeventqueue.h"
#include "queue/ffpacket.h"
#include "queue/ffvpacketqueue.h"

#define CAPTURE_TIME 60

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
                m_stop = true;
                std::cerr << "audioPkt is nullptr" << std::endl;
                continue;
            }

            aPacket = &audioPkt->packet;
            if (aTimeBase.den == -1 && aTimeBase.num == -1) {
                aTimeBase = aEncoder->getCodecCtx()->time_base;
            }

            audioPtsSec = aPacket->pts * av_q2d(aTimeBase);
            std::cerr << "[MuxQ] got audio pkt: pts=" << aPacket->pts << " dts=" << aPacket->dts
                      << " sec=" << audioPtsSec << " size=" << aPacket->size
                      << " tb=" << aTimeBase.num << "/" << aTimeBase.den << std::endl;

            if (audioPtsSec < 0) {
                audioFinish = true;
                av_packet_unref(aPacket);
                continue;
            }
        }

        if (videoFinish) {
            std::lock_guard<std::mutex> lock(mutex);
            videoPkt = vPktQueue->dequeue();

            if (videoPkt == nullptr) {
                std::cerr << "videoPkt is nullptr" << std::endl;
                m_stop = true;
                continue;
            }

            vPacket = &videoPkt->packet;

            if (vTimeBase.den == -1 && vTimeBase.num == -1) {
                vTimeBase = vEncoder->getCodecCtx()->time_base;
            }

            videoPtsSec = vPacket->pts * av_q2d(vTimeBase);
            std::cerr << "[MuxQ] got video pkt: pts=" << vPacket->pts
                      << " dts=" << vPacket->dts
                      << " sec=" << videoPtsSec
                      << " size=" << vPacket->size
                      << " tb=" << vTimeBase.num << "/" << vTimeBase.den << std::endl;
            if (videoPtsSec < 0) {
                videoFinish = true;
                av_packet_unref(vPacket);
                continue;
            }
        }

        std::cerr << "[Mux] write video pkt: pts=" << vPacket->pts << " dts=" << vPacket->dts
                  << " size=" << vPacket->size << std::endl;

        if (audioPtsSec < videoPtsSec) {
            //            std::cout<<"audio Finish:"<<audioPtsSec<<std::fixed<<std::endl;
            ret = muxer->mux(aPacket);
            if (ret < 0) {
                std::cerr << "Mux Audio Fail !" << std::endl;
                m_stop = true;
                return;
            }
            std::cerr << "[Mux] wrote aideo pkt ok" << std::endl;

            audioFinish = true;
            videoFinish = false;

        } else {
            ret = muxer->mux(vPacket);
            if (ret < 0) {
                std::cerr << "Mux Video Fail !" << std::endl;
                m_stop = true;
                return;
            }
            std::cerr << "[Mux] wrote video pkt ok" << std::endl;

            videoFinish = true;
            audioFinish = false;
        }
    }
    muxer->writeTrailer();
}

void FFMuxerThread::sendCaptureProcessEvent(double seconds) {}
