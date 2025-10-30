#ifndef FFVENCODER_H
#define FFVENCODER_H

#include <mutex>
#include <chrono>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

class FFVPacketQueue;

struct FFVEncoderPars
{
    int width;
    int height;
    int biteRate;
    enum AVPixelFormat videoFmt;
    AVRational frameRate;
};

class FFVEncoder
{
public:
    FFVEncoder();
    ~FFVEncoder();

    void init(FFVPacketQueue *pktQueue_);
    void close();
    void wakeAllThread();

    int encode(AVFrame *frame, int streamIndex, int64_t pts, AVRational timeBase);
    AVCodecContext *getCodecCtx();
    FFVEncoderPars *getEncoderPars();
    void initVideo(AVFrame *frame, AVRational fps);
    void resetPtsClock();

private:
    void printError(int ret);

private:
    FFVPacketQueue *pktQueue = nullptr;
    AVCodecContext *codecCtx = nullptr;
    FFVEncoderPars *vPars = nullptr;
    int64_t lastPts = -1;

    std::mutex mutex;

    // 性能监控相关
    std::chrono::steady_clock::time_point lastEncodeTime;
    double avgEncodeTime = 0.0;
    int encodeCount = 0;
    static constexpr double TARGET_ENCODE_TIME_MS = 33.0; // 30fps目标时间
};

#endif // FFVENCODER_H
