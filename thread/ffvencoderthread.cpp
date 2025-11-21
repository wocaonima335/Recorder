#include "ffvencoderthread.h"

#include "decoder/ffvdecoder.h"
#include "encoder/ffvencoder.h"
#include "filter/ffvfilter.h"
#include "muxer/ffmuxer.h"
#include "opengl/ffglitem.h"
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
    gLItem = PreviewBridge::instance().get();
    qDebug() << "[VEncThread] init: vFilter=" << vFilter << " vEncoder=" << vEncoder
             << " muxer=" << muxer << " frmQueue=" << frmQueue;
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
    qDebug() << "[VEncThread] close: reset firstFrame and streamIndex";
}

void FFVEncoderThread::onPauseChanged(bool pausedFlag, int64_t ts_us)
{
    std::unique_lock<std::mutex> lk(pause_mutex);
    if (pausedFlag) {
        // 收到 paused=true
        pause_start_us = ts_us > 0 ? ts_us : av_gettime_relative();
        paused.store(true, std::memory_order_release);
        pause_cv.notify_all();
        qDebug() << "[VEncThread] paused at " << pause_start_us << "us";
    } else {
        // 收到 paused=false
        int64_t now = ts_us > 0 ? ts_us : av_gettime_relative();
        pause_accum_us += (now - pause_start_us);
        paused.store(false, std::memory_order_release);
        first_after_resume = true; // 恢复后首帧 I 帧
        pause_cv.notify_all();
        qDebug() << "[VEncThread] resumed at " << now << "us, total_pause=" << pause_accum_us
                 << "us";
    }
}

void FFVEncoderThread::run()
{
    int64_t frame_count = 0;

    while (!m_stop) {
        AVFrame *frame = frmQueue->dequeue();
        if (!frame)
            break;

        if (streamIndex == -1)
            initEncoder(frame);

        // 如果暂停：软暂停（丢帧+短暂等待），避免队列爆涨；如需硬暂停，可阻塞等待
        if (paused.load(std::memory_order_acquire)) {
            // 软暂停：直接释放帧并等待，避免忙等
            AVFrameTraits::release(frame);
            continue;
        }

        // 恢复后的首帧请求关键帧（如果你在 filter 前）
        if (first_after_resume) {
            frame->pict_type = AV_PICTURE_TYPE_I;
            first_after_resume = false;
        }

        // 计算校正后的墙钟 PTS（扣除暂停累计时长）
        int64_t now_us = av_gettime_relative();
        int64_t wall_us = now_us - start_time_us - pause_accum_us;
        AVRational src_tb = {1, AV_TIME_BASE};
        int64_t wall_pts = av_rescale_q(wall_us, src_tb, timeBase);

        // 理想帧计数 PTS（单调保障）
        int64_t ideal_pts = frame_count;

        // 混合策略：以墙钟为主，确保单调
        int64_t vpts = std::max(wall_pts, ideal_pts);
        frame_count = vpts + 1;

        if (gLItem && frame->format == AV_PIX_FMT_YUV420P) {
            const int w = frame->width;
            const int h = frame->height;

            // 注意 stride：按行拷贝避免因 linesize > width 造成图像错乱
            QByteArray yArr;
            yArr.resize(w * h);
            QByteArray uArr;
            uArr.resize(w * h / 4);
            QByteArray vArr;
            vArr.resize(w * h / 4);

            auto copyPlane =
                [](QByteArray &dst, const uint8_t *src, int width, int height, int linesize) {
                    char *d = dst.data();
                    for (int row = 0; row < height; ++row) {
                        memcpy(d + row * width, src + row * linesize, width);
                    }
                };

            copyPlane(yArr, frame->data[0], w, h, frame->linesize[0]);
            copyPlane(uArr, frame->data[1], w / 2, h / 2, frame->linesize[1]);
            copyPlane(vArr, frame->data[2], w / 2, h / 2, frame->linesize[2]);

            // 异步调用到 UI 线程
            QMetaObject::invokeMethod(gLItem,
                                      "setYUVData",
                                      Qt::QueuedConnection,
                                      Q_ARG(QByteArray, yArr),
                                      Q_ARG(QByteArray, uArr),
                                      Q_ARG(QByteArray, vArr),
                                      Q_ARG(int, w),
                                      Q_ARG(int, h));
        }

        vEncoder->encode(frame, streamIndex, vpts, timeBase);

        AVFrameTraits::release(frame);
    }
}

void FFVEncoderThread::initEncoder(AVFrame *frame)
{
    frameRate = vFilter->getFrameRate();

    vEncoder->initVideo(frame, frameRate);
    timeBase = vEncoder->getCodecCtx()->time_base;

    muxer->addStream(vEncoder->getCodecCtx());
    streamIndex = muxer->getVStreamIndex();
}
