#include "ffvfilterthread.h"
#include "filter/ffvfilter.h"
#include "queue/ffvframequeue.h"
#include "recorder/ffrecorder.h"

FFVFilterThread::FFVFilterThread()
{
    encoderFlag.store(false);
    screenFlag.store(false);
    pauseFlag.store(false);

    pauseTime = 0;
    lastPauseTime = 0;
}

FFVFilterThread::~FFVFilterThread()
{
    if (screenFrame) {
        av_frame_unref(screenFrame);
        av_frame_free(&screenFrame);
    }

    if (lastVideoFrame) {
        av_frame_unref(lastVideoFrame);
        av_frame_free(&lastVideoFrame);
    }
}

void FFVFilterThread::init(FFVFrameQueue *frmQueue_, FFVFilter *filter_)
{
    frmQueue = frmQueue_;
    filter = filter_;
}

void FFVFilterThread::startEncoder()
{
    std::lock_guard<std::mutex> lock(mutex);
    encoderFlag.store(true);
    cond.notify_one();
}

void FFVFilterThread::stopEncoder()
{
    std::lock_guard<std::mutex> lock(mutex);
    encoderFlag.store(false);
    pauseFlag.store(false);
    pauseTime = 0;
    lastPauseTime = 0;
    cond.notify_one();
}

void FFVFilterThread::openVideoSource(int sourceType)
{
    std::lock_guard<std::mutex> lock(mutex);
    enum demuxerType type = static_cast<demuxerType>(sourceType);
    switch (type) {
    case SCREEN:
        screenFlag.store(true);
        break;
    default:
        break;
    }

    cond.notify_one();
}

void FFVFilterThread::closeVideoSource(int sourceType)
{
    std::lock_guard<std::mutex> lock(mutex);
    enum demuxerType type = static_cast<demuxerType>(sourceType);

    switch (type) {
    case SCREEN:
        screenFlag.store(false);
        break;

    default:
        break;
    }

    cond.notify_one();
}

void FFVFilterThread::pauseEncoder()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (pauseFlag.load()) {
        pauseTime += av_gettime_relative() - lastPauseTime;
        pauseFlag.store(false);
    } else {
        pauseFlag.store(true);
        lastPauseTime = av_gettime_relative();
    }
}

void FFVFilterThread::peekStart() {}

void FFVFilterThread::wakeAllThread()
{
    if (frmQueue) {
        frmQueue->wakeAllThread();
    }
}

void FFVFilterThread::run()
{
    while (!m_stop) {
        bool hasScreen = screenFlag.load();
        bool hasEncoder = encoderFlag.load();
        bool hasPause = pauseFlag.load();

        if (!hasScreen && !hasEncoder && !hasPause) {
            std::unique_lock<std::mutex> lock(mutex);
            cond.wait_for(lock, std::chrono::milliseconds(100));
            continue;
        }

        auto start = av_gettime_relative();

        if (hasEncoder && !hasPause) {
            std::lock_guard<std::mutex> lock(mutex);
            AVFrame *encodeFrame = frmQueue->dequeue();
            auto end = av_gettime_relative();
            int64_t duration = (end - start) * 10;

            overplayPts = av_gettime_relative() * 10 + duration - pauseTime * 10;
            encodeFrame->pts = overplayPts;
            filter->sendEncodeFrame(encodeFrame);
        }
    }
}
