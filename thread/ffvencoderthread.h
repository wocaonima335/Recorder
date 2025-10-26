#ifndef FFVENCODERTHREAD_H
#define FFVENCODERTHREAD_H

#include "ffthread.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#include <condition_variable>
#include <mutex>

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

    // 供事件调用的线程安全入口
    void onPauseChanged(bool pausedFlag, int64_t ts_us);

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

    std::atomic<bool> paused{false};
    int64_t pause_start_us{0};
    int64_t pause_accum_us{0};
    bool first_after_resume{false};
    std::mutex pause_mutex;
    std::condition_variable pause_cv;
};

#endif // FFVENCODERTHREAD_H
