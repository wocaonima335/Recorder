#ifndef FFVENCODERTHREAD_H
#define FFVENCODERTHREAD_H

#include "ffthread.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
#include <libavutil/time.h>
}

#include <mutex>

class FFVEncoder;
class FFVFrameQueue;
class FFMuxer;
class FFVFilter;
class FFGLItem;

class FFVEncoderThread : public FFThread
{
public:
    FFVEncoderThread();
    virtual ~FFVEncoderThread() override;

    void init(FFVFilter *vFilter_, FFVEncoder *vEncoder_, FFMuxer *muxer_, FFVFrameQueue *frmQueue_);
    void wakeAllThread();
    void close();

    void setStartTimeUs(int64_t us) { start_time_us = us; }
    void onPauseChanged(bool pausedFlag, int64_t ts_us);

protected:
    virtual void run() override;

private:
    void initEncoder(AVFrame *frame);
    void sendPreviewData(AVFrame *frame);

private:
    FFVEncoder *vEncoder = nullptr;
    FFVFrameQueue *frmQueue = nullptr;
    FFMuxer *muxer = nullptr;
    FFVFilter *vFilter = nullptr;

    int streamIndex = -1;
    AVRational timeBase{};
    AVRational frameRate{};
    int64_t start_time_us = 0;

    std::atomic<bool> paused{false};
    std::atomic<int64_t> pause_accum_us{0};
    std::atomic<bool> first_after_resume{false};
    std::mutex pause_mutex;
    int64_t pause_start_us = 0;
};

#endif // FFVENCODERTHREAD_H
