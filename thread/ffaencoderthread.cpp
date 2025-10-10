#include "ffaencoderthread.h"

#include "decoder/ffadecoder.h"
#include "encoder/ffaencoder.h"
#include "filter/ffafilter.h"
#include "muxer/ffmuxer.h"
#include "queue/ffaframequeue.h"

FFAEncoderThread::FFAEncoderThread() {}

FFAEncoderThread::~FFAEncoderThread() {}

void FFAEncoderThread::init(FFAFilter *aFilter_,
                            FFAEncoder *aEncoder_,
                            FFMuxer *muxer_,
                            FFAFrameQueue *frmQueue_)
{
    aEncoder = aEncoder_;
    muxer = muxer_;
    frmQueue = frmQueue_;
    aFilter = aFilter_;
}

void FFAEncoderThread::close()
{
    if (aEncoder)
        aEncoder->close();
    firstFrame = true;
    firstFramePts = 0;
    streamIndex = -1;
}

void FFAEncoderThread::wakeAllThread()
{
    if (frmQueue) {
        frmQueue->wakeAllThread();
    }
    if (aEncoder) {
        aEncoder->wakeAllThread();
    }
}

void FFAEncoderThread::run()
{
    while (!m_stop) {
        AVFrame *frame = frmQueue->dequeue();
        if (frame == nullptr) {
            break;
        }

        if (streamIndex == -1) {
            initEncoder(frame);
        }

        if (firstFrame) {
            firstFramePts = frame->pts;
            firstFrame = false;

            aEncoder->encode(frame, streamIndex, 0, audioTimeBase);
        } else {
            int64_t relativePts = frame->pts - firstFramePts;
            aEncoder->encode(frame, streamIndex, relativePts, audioTimeBase);
        }
        AVFrameTraits::release(frame);
    }
}

void FFAEncoderThread::initEncoder(AVFrame *frame)
{
    audioTimeBase = aFilter->getTimeBase();
    aEncoder->initAudio(frame);

    muxer->addStream(aEncoder->getCodecCtx());
    streamIndex = muxer->getAStreamIndex();
}
