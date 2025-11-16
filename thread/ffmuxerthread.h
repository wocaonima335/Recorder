#ifndef FFMUXERTHREAD_H
#define FFMUXERTHREAD_H

#include "ffthread.h"

#include <algorithm> // for std::min / std::max
#include <cmath>     // for std::abs
#include <mutex>
extern "C" {
#include <libavformat/avformat.h>
}

class FFAPacketQueue;
class FFVPacketQueue;
class FFMuxer;
class FFAEncoder;
class FFVEncoder;
class FFPacket;
class FFRecorder;

class FFMuxerThread : public FFThread
{
public:
    FFMuxerThread();
    ~FFMuxerThread() override;

    void init(FFAPacketQueue *aPktQueue_,
              FFVPacketQueue *vPktQueue_,
              FFMuxer *muxer_,
              FFAEncoder *aEncoder_,
              FFVEncoder *vEncoder_,
              FFRecorder *recorder_);

    void close();
    void wakeAllThread();

protected:
    virtual void run() override;

private:
    void sendCaptureProcessEvent(double seconds);

private:
    double calculateQueueDuration(FFAPacketQueue *queue, AVRational timeBase);
    double calculateQueueDuration(FFVPacketQueue *queue, AVRational timeBase);

private:
    FFAPacketQueue *aPktQueue = nullptr;
    FFVPacketQueue *vPktQueue = nullptr;

    FFMuxer *muxer = nullptr;

    FFVEncoder *vEncoder = nullptr;
    FFAEncoder *aEncoder = nullptr;

    AVRational vTimeBase;
    AVRational aTimeBase;

    std::mutex mutex;

    double lastProcessTime = 0;

    FFRecorder *recorderCtx = nullptr;

private:
    // 若你已添加该类，可保留现有定义
    class AdaptiveSync
    {
    private:
        double avgTimeDiff = 0.0;
        int sampleCount = 0;

    public:
        double calculateDynamicThreshold(double audioTime, double videoTime)
        {
            double diff = std::abs(audioTime - videoTime);
            avgTimeDiff = (avgTimeDiff * sampleCount + diff) / (sampleCount + 1);
            sampleCount = std::min(sampleCount + 1, 1000);
            // 基于历史平均差异动态调整阈值；最小 16ms
            return std::max(0.016, avgTimeDiff * 1.5);
        }

        int samples() const { return sampleCount; }
        void reset()
        {
            avgTimeDiff = 0.0;
            sampleCount = 0;
        }
    };
};

#endif // FFMUXERTHREAD_H
