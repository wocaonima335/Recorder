#include "ffvencoder.h"
#include "queue/ffvpacketqueue.h"

#include <QtGlobal>
#include <iostream>
#include <thread>

FFVEncoder::FFVEncoder() {}

FFVEncoder::~FFVEncoder()
{
    close();
}

void FFVEncoder::init(FFVPacketQueue *pktQueue_)
{
    pktQueue = pktQueue_;
}

void FFVEncoder::close()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }

    if (vPars) {
        delete vPars;
        vPars = nullptr;
    }

    lastPts = -1;
}

void FFVEncoder::wakeAllThread()
{
    if (pktQueue) {
        pktQueue->wakeAllThread();
    }
}

int FFVEncoder::encode(AVFrame *frame, int streamIndex, int64_t pts, AVRational timeBase)
{
    Q_UNUSED(timeBase);
    std::lock_guard<std::mutex> lock(mutex);

    if (frame == nullptr || codecCtx == nullptr) {
        std::cout << "nullptr" << std::endl;
        return 0;
    }

    frame->pts = pts; // 将计算好的 PTS 写到 AVFrame 上，交给编码器

    int ret = avcodec_send_frame(codecCtx, frame);
    if (ret < 0) {
        printError(ret);
        return -1;
    }

    while (1) {
        AVPacket *pkt = av_packet_alloc();
        ret = avcodec_receive_packet(codecCtx, pkt);
        if (ret == AVERROR(EAGAIN)) {
            av_packet_free(&pkt);
            printError(ret);
            break;
        } else if (ret == AVERROR_EOF) {
            std::cout << "Encode Video EOF !" << std::endl;
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            printError(ret);
            av_packet_free(&pkt);
            return -1;
        } else {
            pkt->stream_index = streamIndex;
            std::cerr << "[VEncPkt] produced: pts=" << pkt->pts
                      << " dts=" << pkt->dts
                      << " size=" << pkt->size
                      << " stream=" << streamIndex << std::endl;
            pktQueue->enqueue(pkt);
            av_packet_free(&pkt);
        }
    }

    return 0;
}

AVCodecContext *FFVEncoder::getCodecCtx()
{
    return codecCtx;
}

FFVEncoderPars *FFVEncoder::getEncoderPars()
{
    return vPars;
}

void FFVEncoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof errorBuffer);
    if (res < 0) {
        std::cerr << "Unknow Error!" << std::endl;
    } else {
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}

void FFVEncoder::initVideo(AVFrame *frame, AVRational fps)
{
    std::lock_guard<std::mutex> lock(mutex);
    resetPtsClock();

    // 参数验证
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        std::cerr << "Invalid frame parameters!" << std::endl;
        return;
    }

    // 清理旧参数（防止内存泄漏）
    if (vPars) {
        delete vPars;
        vPars = nullptr;
    }

    // 初始化编码参数
    vPars = new FFVEncoderPars();
    vPars->width = frame->width;
    vPars->height = frame->height;
    vPars->videoFmt = AV_PIX_FMT_YUV420P;
    vPars->frameRate = fps;

    // 根据分辨率智能设置码率
    int pixelCount = vPars->width * vPars->height;
    if (pixelCount > 1920 * 1080) {        // 1080p以上
        vPars->biteRate = 4 * 1024 * 1024; // 4Mbps
    } else if (pixelCount > 1280 * 720) {  // 720p以上
        vPars->biteRate = 2 * 1024 * 1024; // 2Mbps
    } else {
        vPars->biteRate = 1 * 1024 * 1024; // 1Mbps
    }

    // 编码器选择（增加备选方案）
    const AVCodec *codec = nullptr;

    // 备用方案：通用H.264编码器
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        std::cout << "Using fallback encoder: H264" << std::endl;
    }

    if (!codec) {
        std::cerr << "Find H264 Codec Fail !" << std::endl;
        delete vPars;
        vPars = nullptr;
        return;
    }

    // 清理旧编码器上下文
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        std::cerr << "Alloc CodecCtx Fail !" << std::endl;
        delete vPars;
        vPars = nullptr;
        return;
    }

    // 统一的编码器参数配置
    codecCtx->width = vPars->width;
    codecCtx->height = vPars->height;
    codecCtx->bit_rate = vPars->biteRate;
    codecCtx->rc_max_rate = vPars->biteRate;
    codecCtx->rc_buffer_size = vPars->biteRate * 2;
    codecCtx->framerate = vPars->frameRate;
    codecCtx->time_base = av_inv_q(vPars->frameRate); // 等同于 {fps.den, fps.num}
    codecCtx->pix_fmt = vPars->videoFmt;
    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // 根据使用场景优化配置
    bool isHardwareEncoder = codec->id == AV_CODEC_ID_H264
                             && (strstr(codec->name, "nvenc") || strstr(codec->name, "amf")
                                 || strstr(codec->name, "videotoolbox"));

    if (isHardwareEncoder) {
        // 硬件编码器优化配置
        codecCtx->gop_size = 60;    // 硬件编码器可以处理更大的GOP
        codecCtx->max_b_frames = 2; // 硬件编码器可以处理少量B帧
    } else {
        // 软件编码器低延迟配置
        codecCtx->gop_size = 30;
        codecCtx->max_b_frames = 0; // 无B帧以减少延迟
        codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    }

    // 通用性能优化
    codecCtx->keyint_min = codecCtx->gop_size;
    codecCtx->thread_count = codecCtx->thread_count
        = std::min(8, (int) std::thread::hardware_concurrency());
    ;
    codecCtx->thread_type = FF_THREAD_FRAME;

    // 质量与速度平衡
    codecCtx->qmin = 10;
    codecCtx->qmax = 40;
    codecCtx->max_qdiff = 4;

    AVDictionary *codec_options = nullptr;

    // 智能预设选择
    const char *preset = isHardwareEncoder ? "medium" : "ultrafast";
    av_dict_set(&codec_options, "preset", preset, 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);

    // 码率控制优化
    av_dict_set(&codec_options, "rc-lookahead", "0", 0);

    int ret = avcodec_open2(codecCtx, codec, &codec_options);
    if (ret < 0) {
        std::cerr << "Open codec failed: ";
        printError(ret);
        avcodec_free_context(&codecCtx);
        delete vPars;
        vPars = nullptr;
        av_dict_free(&codec_options);
        return;
    }

    // 检查未使用的选项
    if (codec_options) {
        AVDictionaryEntry *t = nullptr;
        std::cout << "Unused codec options:" << std::endl;
        while ((t = av_dict_get(codec_options, "", t, AV_DICT_IGNORE_SUFFIX))) {
            std::cout << "  " << t->key << " = " << t->value << std::endl;
        }
        av_dict_free(&codec_options);
    }

    std::cout << "Encoder initialized successfully: " << vPars->width << "x" << vPars->height
              << " @" << av_q2d(vPars->frameRate) << "fps"
              << " bitrate: " << (vPars->biteRate / 1024 / 1024) << "Mbps" << std::endl;
}

void FFVEncoder::resetPtsClock()
{
    lastPts = -1;
}
