#include "ffvencoderthread.h"

#include "decoder/ffvdecoder.h"
#include "encoder/ffvencoder.h"
#include "filter/ffvfilter.h"
#include "muxer/ffmuxer.h"
#include "queue/ffvframequeue.h"

#include <iostream>

FFVEncoderThread::FFVEncoderThread() {}

FFVEncoderThread::~FFVEncoderThread() {}

void FFVEncoderThread::init(FFVFilter *vFilter_,
                            FFVEncoder *vEncoder_,
                            FFMuxer *muxer_,
                            FFVFrameQueue *frmQueue_)
{
    vFilter = vFilter_;
    vEncoder = vEncoder_;
    muxer = muxer_;
    frmQueue = frmQueue_;
    std::cerr << "[VEncThread] init: vFilter=" << vFilter
              << " vEncoder=" << vEncoder
              << " muxer=" << muxer
              << " frmQueue=" << frmQueue << std::endl;
}

void FFVEncoderThread::wakeAllThread()
{
    if (frmQueue) {
        frmQueue->wakeAllThread();
    }
    if (vEncoder) {
        vEncoder->wakeAllThread();
    }
}

void FFVEncoderThread::close()
{
    if (vEncoder) {
        vEncoder->close();
    }
    firstFrame = true;
    firstFramePts = 0;
    streamIndex = -1;
    std::cerr << "[VEncThread] close: reset firstFrame and streamIndex" << std::endl;
}

void FFVEncoderThread::run()
{
    std::cerr << "[VEncThread] run start" << std::endl;
    while (!m_stop) {
        AVFrame *frame = frmQueue->dequeue();
        if (frame == nullptr) {
            std::cerr << "[VEncThread] frmQueue->dequeue returned nullptr, stopping thread" << std::endl;
            break;
        }

        // log dequeued frame basic info
        std::cerr << "[VEncThread] dequeued frame=" << frame
                  << " pts=" << frame->pts
                  << " w=" << frame->width
                  << " h=" << frame->height
                  << " fmt=" << frame->format
                  << std::endl;

        if (streamIndex == -1) {
            std::cerr << "[VEncThread] streamIndex is -1, initializing encoder with first frame" << std::endl;
            initEncoder(frame);
            std::cerr << "[VEncThread] encoder initialized: streamIndex=" << streamIndex
                      << " frameRate=" << frameRate.num << "/" << frameRate.den
                      << " timeBase=" << timeBase.num << "/" << timeBase.den << std::endl;
        }

        if (firstFrame) {
            firstFramePts = frame->pts;
            firstFrame = false;
            std::cerr << "[VEncThread] first frame: firstFramePts=" << firstFramePts
                      << ", encode with relativePts=0" << std::endl;
            vEncoder->encode(frame, streamIndex, 0, timeBase);
        } else {
            int64_t relativePts = frame->pts - firstFramePts;
            std::cerr << "[VEncThread] encode frame: relativePts=" << relativePts
                      << " (abs pts=" << frame->pts << ")"
                      << " streamIndex=" << streamIndex
                      << " timeBase=" << timeBase.num << "/" << timeBase.den << std::endl;
            vEncoder->encode(frame, streamIndex, relativePts, timeBase);
        }

        // after submit to encoder, free frame
        std::cerr << "[VEncThread] frame submitted to encoder, releasing frame" << std::endl;
        AVFrameTraits::release(frame);
    }
    std::cerr << "[VEncThread] run exit" << std::endl;
}

void FFVEncoderThread::initEncoder(AVFrame *frame)
{
    frameRate = vFilter->getFrameRate();
    //    frameRate = {30,1};
    timeBase = vFilter->getTimeBase();
    //    timeBase ={1,10000000};

    std::cerr << "[VEncThread] initEncoder: frameRate=" << frameRate.num << "/" << frameRate.den
              << " timeBase=" << timeBase.num << "/" << timeBase.den << std::endl;

    vEncoder->initVideo(frame, frameRate);

    muxer->addStream(vEncoder->getCodecCtx());
    streamIndex = muxer->getVStreamIndex();
    std::cerr << "[VEncThread] initEncoder: stream added to muxer, streamIndex=" << streamIndex << std::endl;
}
