#ifndef FFMUXER_H
#define FFMUXER_H

#include <atomic>
#include <chrono>
#include <iostream>
#include <shared_mutex>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

class FFMuxer
{
public:
    FFMuxer();
    ~FFMuxer();

    void init(const std::string &url_, std::string const &format_ = "mp4");
    void addStream(AVCodecContext *codecCtx);
    int mux(AVPacket *packet);
    void writeHeader();
    void writeTrailer();

    int getAStreamIndex();
    int getVStreamIndex();

    void close();

private:
    void initMuxer();
    void printError(int ret);

private:
    std::string url;
    std::string format;

    std::atomic<bool> headerFlag{false};
    std::atomic<bool> trailerFlag{false};
    std::atomic<bool> readyFlag{false};
    std::atomic<int> streamCount{0};

    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *aCodecCtx = nullptr;
    AVCodecContext *vCodecCtx = nullptr;

    AVStream *aStream = nullptr;
    AVStream *vStream = nullptr;

    int aStreamIndex = -1;
    int vStreamIndex = -1;

    std::shared_mutex mutex;
};

#endif // FFMUXER_H
