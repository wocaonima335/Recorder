#ifndef FFMUXERTHREAD_H
#define FFMUXERTHREAD_H

#include "ffthread.h"

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
};

#endif // FFMUXERTHREAD_H
