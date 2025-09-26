#include "ffmuxer.h"

FFMuxer::FFMuxer() {}

FFMuxer::~FFMuxer()
{
    std::lock_guard<std::shared_mutex> lock(mutex);
    if (!headerFlag) {
        writeTrailer();
    }

    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
    }
    url.clear();
}

void FFMuxer::init(const std::string &url_, const std::string &format_)
{
    std::lock_guard<std::shared_mutex> lock(mutex);
    url = url_;
    format = format_;
    initMuxer();
}

void FFMuxer::addStream(AVCodecContext *codecCtx)
{
    std::lock_guard<std::shared_mutex> lock(mutex);

    // 验证编码器上下文
    if (!codecCtx || !codecCtx->codec) {
        std::cerr << "Invalid codec context!" << std::endl;
        return;
    }

    // 验证时间基
    if (codecCtx->time_base.num <= 0 || codecCtx->time_base.den <= 0) {
        std::cerr << "Invalid time base: " << codecCtx->time_base.num << "/"
                  << codecCtx->time_base.den << std::endl;
        return;
    }

    AVStream *stream = avformat_new_stream(fmtCtx, nullptr);

    if (!stream) {
        std::cerr << "New Stream Fail !" << std::endl;
        return;
    }

    int ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);

    if (ret < 0) {
        std::cerr << "Copy Parameters From Context Fail !" << std::endl;
        printError(ret);
        return;
    }

    stream->time_base = codecCtx->time_base;
    if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        aCodecCtx = codecCtx;
        aStream = stream;
        aStreamIndex = stream->index;

    } else if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
        vCodecCtx = codecCtx;
        vStream = stream;
        vStreamIndex = stream->index;
    }

    streamCount++;
    //    std::cout << "streamCout = "<< streamCount <<std::endl;
    if (streamCount == 2) {
        readyFlag = true; // 直接赋值，由锁保证可见性
    }
}

int FFMuxer::mux(AVPacket *packet)
{
    if (trailerFlag.load())
        return 1;

    int streamIndex = packet->stream_index;
    AVRational srcTimeBase, dstTimeBase;
    {
        std::lock_guard<std::shared_mutex> lock(mutex);
        if (streamIndex == aStreamIndex) {
            srcTimeBase = aCodecCtx->time_base;
            dstTimeBase = aStream->time_base;
        } else if (streamIndex == vStreamIndex) {
            srcTimeBase = vCodecCtx->time_base;
            dstTimeBase = vStream->time_base;
        } else {
            return -1;
        }
    }

    // 时间戳转换
    packet->pts = av_rescale_q(packet->pts, srcTimeBase, dstTimeBase);
    packet->dts = av_rescale_q(packet->dts, srcTimeBase, dstTimeBase);
    packet->duration = av_rescale_q(packet->duration, srcTimeBase, dstTimeBase);

    if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE || packet->pts < 0) {
        av_packet_unref(packet);
        av_packet_free(&packet);
        return 0;
    }

    // 写入操作（独占锁）
    {
        std::lock_guard<std::shared_mutex> lock(mutex);
        if (fmtCtx == nullptr) {
            return 0;
        }

        int ret = av_interleaved_write_frame(fmtCtx, packet);
        if (ret < 0) {
            std::cerr << "Mux Fail !" << std::endl;
            printError(ret);
            av_packet_unref(packet);
            av_packet_free(&packet);
            return -1;
        }
    }

    av_packet_unref(packet);
    av_packet_free(&packet);
    return 0;
}

void FFMuxer::writeHeader()
{
    while (!readyFlag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::lock_guard<std::shared_mutex> lock(mutex);

    if (headerFlag) {
        return;
    }

    headerFlag = true;
    if (fmtCtx == nullptr) {
        std::cerr << "fmtCtx is nullptr" << std::endl;
        return;
    }

    int ret = avformat_write_header(fmtCtx, nullptr);
    if (ret < 0) {
        std::cerr << "Write Header Fail !" << std::endl;
        printError(ret);
        headerFlag = false; // 恢复状态以防后续错误
    }
}

void FFMuxer::writeTrailer()
{
    std::lock_guard<std::shared_mutex> lock(mutex); // 保护尾文件写入状态
    if (trailerFlag) {                              // 直接判断bool变量
        return;
    }
    trailerFlag = true; // 设置状态后再执行写入

    if (fmtCtx == nullptr) {
        return;
    }
    int ret = av_write_trailer(fmtCtx);
    if (ret < 0) {
        std::cerr << "Write Trailer Fail !" << std::endl;
        printError(ret);
        trailerFlag = false; // 恢复状态以防后续错误
    }
}

int FFMuxer::getAStreamIndex()
{
    std::lock_guard<std::shared_mutex> lock(mutex);
    return aStreamIndex;
}

int FFMuxer::getVStreamIndex()
{
    std::lock_guard<std::shared_mutex> lock(mutex);
    return vStreamIndex;
}

void FFMuxer::close()
{
    std::lock_guard<std::shared_mutex> lock(mutex);
    if (fmtCtx) {
        if (fmtCtx->pb)
            avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx); // 释放输出上下文
        fmtCtx = nullptr;
    }
    if (!headerFlag) {
        writeTrailer();
    }
    url.clear();
    format.clear();

    headerFlag = false;
    trailerFlag = false;
    readyFlag = false;

    aStreamIndex = vStreamIndex = -1;
    streamCount = 0;

    aStream = nullptr;
    vStream = nullptr;

    aCodecCtx = nullptr;
    vCodecCtx = nullptr;
}

void FFMuxer::initMuxer()
{
    int ret = avformat_alloc_output_context2(&fmtCtx, nullptr, format.c_str(), url.c_str());
    if (ret < 0) {
        std::cerr << "Alloc Output Context Fail !" << std::endl;
        printError(ret);
        return;
    }

    if (format == "rtsp") {
        AVDictionary *opts = nullptr;
        ret = av_opt_set(&opts, "rtsp_transport", "tcp", 0);
        if (ret < 0) {
            std::cerr << "av_opt_set:rtsp_transport fail" << std::endl;
        }
        ret = av_dict_set(&opts, "stimeout", "5000000", 0);
        if (ret < 0) {
            std::cerr << "av_dict_set: stimeout fail" << std::endl;
        }

        ret = av_opt_set_dict(fmtCtx, &opts);
        if (ret < 0) {
            std::cerr << "av_opt_set_dict fail" << std::endl;
        }

        av_dict_free(&opts);

    } else {
        ret = avio_open(&fmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "Open File Fail !" << std::endl;
            printError(ret);
            return;
        }
    }
}

void FFMuxer::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof(errorBuffer));
    if (res < 0) {
        std::cerr << "Unknow Error!" << std::endl;
    } else {
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}
