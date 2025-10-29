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

void FFAEncoderThread::onPauseChanged(bool pauseFlag, int64_t ts_us)
{
    std::unique_lock<std::mutex> lk(pause_mutex);
    if (pauseFlag) {
        pause_start_us = ts_us > 0 ? ts_us : av_gettime_relative();
        paused.store(true, std::memory_order_release);
        pause_cv.notify_all();
        std::cerr << "[AEncThread] paused at " << pause_start_us << "us" << std::endl;
    } else {
        int64_t now = ts_us > 0 ? ts_us : av_gettime_relative();
        pause_accum_us += (now - pause_start_us);
        paused.store(false, std::memory_order_release);
        first_after_resume = true; // 音频不需要 I 帧，但保留标志以保持逻辑一致
        pause_cv.notify_all();
        std::cerr << "[AEncThread] resumed at " << now << "us, total_pause=" << pause_accum_us
                  << "us" << std::endl;
    }
}

void FFAEncoderThread::run()
{
    while (!m_stop) {
        AVFrame *frame = frmQueue->dequeue();
        if (!frame) {
            break;
        }

        if (streamIndex == -1) {
            initEncoder(frame);
        }

        // 用统一墙钟计算音频 PTS
        int64_t now_us = av_gettime_relative();
        int64_t wall_us = now_us - start_time_us;
        AVRational src_tb = {1, AV_TIME_BASE};
        int64_t apts = av_rescale_q(wall_us, src_tb, audioTimeBase);

        aEncoder->encode(frame, streamIndex, apts, audioTimeBase);
        AVFrameTraits::release(frame);
    }
}

void FFAEncoderThread::initEncoder(AVFrame *frame)
{
    // 初始化音频编码器
    aEncoder->initAudio(frame);

    // 使用编码器的 time_base（与输出流一致）
    audioTimeBase = aEncoder->getCodecCtx()->time_base;

    // 复用器添加音频流，并获取音频流索引
    muxer->addStream(aEncoder->getCodecCtx());
    streamIndex = muxer->getAStreamIndex();

    // 首帧标记仅用于初始化，不用于 PTS 计算
    firstFrame = false;
    firstFramePts = 0;
}
