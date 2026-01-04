#include "ffaencoderthread.h"

#include "encoder/ffaencoder.h"
#include "muxer/ffmuxer.h"
#include "queue/ffaframequeue.h"

#include <QDebug>
#include <algorithm>

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
    streamIndex = -1;
    last_apts = 0;
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
    int64_t now = ts_us > 0 ? ts_us : av_gettime_relative();

    if (pauseFlag) {
        if (paused.load(std::memory_order_relaxed)) {
            return;
        }
        pause_start_us = now;
        paused.store(true, std::memory_order_release);
        pause_cv.notify_all();
        qDebug() << "[AEncThread] paused at" << pause_start_us << "us";
    } else {
        if (!paused.load(std::memory_order_relaxed)) {
            return;
        }
        pause_accum_us += (now - pause_start_us);
        paused.store(false, std::memory_order_release);
        first_after_resume = true;
        pause_cv.notify_all();
        qDebug() << "[AEncThread] resumed at" << now << "us, total_pause=" << pause_accum_us << "us";
    }
}

void FFAEncoderThread::run()
{
    static constexpr AVRational SRC_TB = {1, AV_TIME_BASE};

    while (!m_stop) {
        AVFrame *frame = frmQueue->dequeue();
        if (!frame) {
            break;
        }

        if (streamIndex == -1) {
            initEncoder(frame);
        }

        if (paused.load(std::memory_order_acquire)) {
            AVFrameTraits::release(frame);
            continue;
        }

        int64_t pause_accum_snapshot;
        bool need_reset;
        {
            std::lock_guard<std::mutex> lk(pause_mutex);
            pause_accum_snapshot = pause_accum_us;
            need_reset = first_after_resume;
            first_after_resume = false;
        }

        int64_t now_us = av_gettime_relative();
        int64_t wall_us = now_us - start_time_us - pause_accum_snapshot;
        int64_t apts = av_rescale_q(wall_us, SRC_TB, audioTimeBase);

        apts = std::max(apts, last_apts);
        last_apts = apts + 1;

        if (need_reset) {
            aEncoder->resetPending(apts);
            aEncoder->flush();
            qDebug() << "[AEncThread] resetPending+flush on resume: base_pts=" << apts
                     << "tb=" << audioTimeBase.num << "/" << audioTimeBase.den;
        }

        aEncoder->encode(frame, streamIndex, apts, audioTimeBase);
        AVFrameTraits::release(frame);
    }
}

void FFAEncoderThread::initEncoder(AVFrame *frame)
{
    if (!aEncoder || !muxer) {
        qWarning() << "[AEncThread] initEncoder failed: null encoder or muxer";
        return;
    }

    aEncoder->initAudio(frame);
    audioTimeBase = aEncoder->getCodecCtx()->time_base;
    muxer->addStream(aEncoder->getCodecCtx());
    streamIndex = muxer->getAStreamIndex();

    if (streamIndex < 0) {
        qWarning() << "[AEncThread] initEncoder failed: invalid stream index" << streamIndex;
    } else {
        qDebug() << "[AEncThread] initialized: streamIndex=" << streamIndex
                 << "timeBase=" << audioTimeBase.num << "/" << audioTimeBase.den;
    }
}
