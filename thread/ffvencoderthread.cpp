#include "ffvencoderthread.h"

#include "encoder/ffvencoder.h"
#include "filter/ffvfilter.h"
#include "muxer/ffmuxer.h"
#include "opengl/ffglitem.h"
#include "queue/ffvframequeue.h"

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
}

void FFVEncoderThread::wakeAllThread()
{
    if (frmQueue)
        frmQueue->wakeAllThread();
    if (vEncoder)
        vEncoder->wakeAllThread();
}

void FFVEncoderThread::close()
{
    if (vEncoder)
        vEncoder->close();
    streamIndex = -1;
}

void FFVEncoderThread::onPauseChanged(bool pausedFlag, int64_t ts_us)
{
    std::lock_guard<std::mutex> lk(pause_mutex);
    int64_t now = ts_us > 0 ? ts_us : av_gettime_relative();
    if (pausedFlag) {
        pause_start_us = now;
        paused.store(true, std::memory_order_release);
    } else {
        pause_accum_us.fetch_add(now - pause_start_us, std::memory_order_relaxed);
        first_after_resume.store(true, std::memory_order_release);
        paused.store(false, std::memory_order_release);
    }
}

void FFVEncoderThread::run()
{
    static constexpr AVRational kSrcTimeBase{1, AV_TIME_BASE};
    int64_t frame_count = 0;

    while (!m_stop) {
        if (!frmQueue)
            break;

        AVFrame *frame = frmQueue->dequeue();
        if (!frame)
            break;

        if (streamIndex == -1) {
            if (!vFilter || !vEncoder || !muxer) {
                AVFrameTraits::release(frame);
                break;
            }
            initEncoder(frame);
        }

        if (paused.load(std::memory_order_acquire)) {
            AVFrameTraits::release(frame);
            continue;
        }

        if (first_after_resume.exchange(false, std::memory_order_acquire))
            frame->pict_type = AV_PICTURE_TYPE_I;

        int64_t wall_us = av_gettime_relative() - start_time_us - pause_accum_us.load(std::memory_order_relaxed);
        int64_t wall_pts = av_rescale_q(wall_us, kSrcTimeBase, timeBase);
        int64_t vpts = std::max(wall_pts, frame_count);
        frame_count = vpts + 1;

        sendPreviewData(frame);
        vEncoder->encode(frame, streamIndex, vpts, timeBase);
        AVFrameTraits::release(frame);
    }
}

void FFVEncoderThread::sendPreviewData(AVFrame *frame)
{
    if (frame->format != AV_PIX_FMT_YUV420P)
        return;

    FFGLItem *previewItem = PreviewBridge::instance().get();
    if (!previewItem)
        return;

    const int w = frame->width;
    const int h = frame->height;

    QByteArray yArr(w * h, Qt::Uninitialized);
    QByteArray uArr(w * h / 4, Qt::Uninitialized);
    QByteArray vArr(w * h / 4, Qt::Uninitialized);

    auto copyPlane = [](char *dst, const uint8_t *src, int width, int height, int linesize) {
        for (int row = 0; row < height; ++row)
            memcpy(dst + row * width, src + row * linesize, width);
    };

    copyPlane(yArr.data(), frame->data[0], w, h, frame->linesize[0]);
    copyPlane(uArr.data(), frame->data[1], w / 2, h / 2, frame->linesize[1]);
    copyPlane(vArr.data(), frame->data[2], w / 2, h / 2, frame->linesize[2]);

    QMetaObject::invokeMethod(previewItem, "setYUVData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, yArr), Q_ARG(QByteArray, uArr),
                              Q_ARG(QByteArray, vArr), Q_ARG(int, w), Q_ARG(int, h));
}

void FFVEncoderThread::initEncoder(AVFrame *frame)
{
    frameRate = vFilter->getFrameRate();

    vEncoder->initVideo(frame, frameRate);
    timeBase = vEncoder->getCodecCtx()->time_base;

    muxer->addStream(vEncoder->getCodecCtx());
    streamIndex = muxer->getVStreamIndex();
}
