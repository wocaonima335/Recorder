#ifndef FFVENCODERTHREAD_H
#define FFVENCODERTHREAD_H

#include "ffthread.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

class FFVEncoderPars;
class FFVEncoder;
class FFVFrameQueue;
class FFVideoPars;
class FFMuxer;
class FFVFilter;

class FFVEncoderThread : public FFThread
{
public:
    FFVEncoderThread();
    virtual ~FFVEncoderThread() override;

    void init(FFVFilter *vFilter_, FFVEncoder *vEncoder_, FFMuxer *muxer_, FFVFrameQueue *frmQueue_);
    void wakeAllThread();
    void close();

    void setStartTimeUs(int64_t us) { start_time_us = us; }

protected:
    virtual void run() override;

private:
    void initEncoder(AVFrame *frame);

private:
    FFVEncoder *vEncoder = nullptr;
    FFVFrameQueue *frmQueue = nullptr;
    FFMuxer *muxer = nullptr;

    int streamIndex = -1;
    FFVFilter *vFilter = nullptr;

    AVRational timeBase;
    AVRational frameRate;

    int64_t firstFramePts = 0;
    bool firstFrame = true;

    int64_t start_time_us = 0;
    bool useWallClockPts = true;
};

#endif // FFVENCODERTHREAD_H
