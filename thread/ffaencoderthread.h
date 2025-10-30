#ifndef FFAENCODERTHREAD_H
#define FFAENCODERTHREAD_H

#include "ffthread.h"

#include <condition_variable>
#include <mutex>

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

    void onPauseChanged(bool pauseFlag, int64_t ts_us);

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

    std::atomic<bool> paused{false};
    int64_t pause_start_us{0};
    int64_t pause_accum_us{0};
    bool first_after_resume{false}; // 音频不需要关键帧，这里仅保留结构一致性
    std::mutex pause_mutex;
    std::condition_variable pause_cv;
};

#endif // FFAENCODERTHREAD_H
