#ifndef FFVDECODER_H
#define FFVDECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/cpu.h>
#include <libavutil/hwcontext.h>
}
#include <atomic>
#include <memory>
#include <mutex>

struct FFVideoPars
{
    int width;
    int height;
    AVRational frameRate;
    AVPixelFormat pixFmtEnum;
    AVRational timeBase;
};

class FFVFrameQueue;
class FFVResampler;

class FFVDecoder
{
public:
    static FFVDecoder &getInstance();

    FFVDecoder(const FFVDecoder &) = delete;
    FFVDecoder *operator=(const FFVDecoder &) = delete;

    ~FFVDecoder();
    void decode(AVPacket *packet);
    void init(AVStream *stream_, FFVFrameQueue *frmQueue_);
    void flushDecoder();
    int getTotalSec();
    FFVideoPars *getVideoPars();

    AVCodecContext *getCodecCtx();
    AVStream *getStream();

    void enqueueNull();
    void wakeAllThread();
    void stop();
    void flushQueue();
    void close();

private:
    // 私有构造函数
    explicit FFVDecoder();

    void initResampler();
    void initVideoPars(AVFrame *frame);
    void printError(int ret);

private:
    AVCodecContext *codecCtx = nullptr;
    AVStream *stream = nullptr;
    FFVFrameQueue *frmQueue = nullptr;
    FFVideoPars *vPars = nullptr;
    FFVideoPars *swsvPars = nullptr;
    FFVResampler *resampler = nullptr;

    bool hardDecodeFlag = false;

    std::atomic<bool> m_stop;

    std::mutex mutex;

    // 单例相关的静态成员
    static std::once_flag s_flag;
    static std::unique_ptr<FFVDecoder> s_instance;
};

#endif // FFVDECODER_H
