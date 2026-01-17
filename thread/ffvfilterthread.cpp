#include "ffvfilterthread.h"

#include "filter/ffvfilter.h"
#include "queue/ffvframequeue.h"
#include "recorder/ffrecorder.h"

#include <iostream>

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
        AVFrameTraits::release(screenFrame);
    }

    if (lastVideoFrame) {
        AVFrameTraits::release(lastVideoFrame);
    }
}

void FFVFilterThread::init(FFVFrameQueue *frmQueue_, FFVFilter *filter_)
{
    frmQueue = frmQueue_;
    filter = filter_;
}

void FFVFilterThread::updateQueue(FFVFrameQueue *frmQueue_)
{
    std::lock_guard<std::mutex> lock(mutex);
    frmQueue = frmQueue_;
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
    case CAMERA:
        cameraFlag.store(true);
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
    case CAMERA:
        cameraFlag.store(false);
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
        bool hascamera = cameraFlag.load();

        if (!hascamera && !hasEncoder && !hasPause) {
            std::unique_lock<std::mutex> lock(mutex);
            // log: thread is idle and waiting for condition
            qDebug() << "[FFVFilterThread] idle, waiting (camera=" << hascamera
                     << ", encoder=" << hasEncoder << ", pause=" << hasPause << ")";
            cond.wait_for(lock, std::chrono::milliseconds(100));
            continue;
        }

        auto start = av_gettime_relative();

        if (hasEncoder && !hasPause) {
            std::lock_guard<std::mutex> lock(mutex);
            qDebug() << "[FFVFilterThread] ready to dequeue frame (encoder=1, pause=0)";
            AVFrame *encodeFrame = frmQueue->dequeue();
            auto end = av_gettime_relative();
            int64_t duration = (end - start) * 10;
            qDebug() << "[FFVFilterThread] dequeue done, frame=" << encodeFrame
                     << ", wait_us=" << (end - start);

            overplayPts = av_gettime_relative() * 10 + duration - pauseTime * 10;
            qDebug() << "[FFVFilterThread] computed pts (100ns units)=" << overplayPts
                     << ", pauseTime(100ns)=" << (pauseTime.load() * 10);

            encodeFrame->pts = overplayPts;
            qDebug() << "[FFVFilterThread] set frame->pts and forwarding to encoder queue";
            filter->sendEncodeFrame(encodeFrame);
            qDebug() << "[FFVFilterThread] forwarded frame to encoderFrameQueue via "
                        "FFVFilter::sendEncodeFrame";
        }
    }
}
