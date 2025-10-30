#ifndef FFAENCODERTHREAD_H
#define FFAENCODERTHREAD_H

#include "ffthread.h"

extern "C" {
#include <libavformat/avformat.h>
}

class FFAEncoder;
class FFAFrameQueue;
class FFMuxer;
class FFAFilter;

class FFAEncoderThread : public FFThread
{
public:
    FFAEncoderThread();
    virtual ~FFAEncoderThread() override;

    void init(FFAFilter *aFilter_, FFAEncoder *aEncoder_, FFMuxer *muxer_, FFAFrameQueue *frmQueue_);

    void close();
    void wakeAllThread();
    void setStartTimeUs(int64_t us) { start_time_us = us; }

protected:
    virtual void run() override;

private:
    void initEncoder(AVFrame *frame);

private:
    FFAEncoder *aEncoder = nullptr;
    FFAFrameQueue *frmQueue = nullptr;
    FFMuxer *muxer = nullptr;
    AVRational audioTimeBase = {0, 1};

    int streamIndex = -1;
    FFAFilter *aFilter = nullptr;

    int64_t firstFramePts = 0;
    bool firstFrame = true;
    int64_t start_time_us = 0;
};

#endif // FFAENCODERTHREAD_H
