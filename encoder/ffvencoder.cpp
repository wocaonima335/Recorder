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
    if (codecCtx)
    {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }

    if (vPars)
    {
        delete vPars;
        vPars = nullptr;
    }

    lastPts = -1;
}

void FFVEncoder::wakeAllThread()
{
    if (pktQueue)
    {
        pktQueue->wakeAllThread();
    }
}

int FFVEncoder::encode(AVFrame *frame, int streamIndex, int64_t pts, AVRational timeBase)
{
    Q_UNUSED(timeBase);

    if (frame == nullptr || codecCtx == nullptr)
    {
        std::cout << "nullptr" << std::endl;
        return 0;
    }

    // // 性能监控开始
    // auto encodeStart = std::chrono::steady_clock::now();

    frame->pts = pts; // 将计算好的 PTS 写到 AVFrame 上，交给编码器
    qDebug() << "[FFVEncoder]:encode pts :" << pts;

    int ret = avcodec_send_frame(codecCtx, frame);
    if (ret < 0)
    {
        printError(ret);
        return -1;
    }

    AVPacket *pkt = av_packet_alloc();
    ret = avcodec_receive_packet(codecCtx, pkt);

    if (ret == AVERROR(EAGAIN))
    {
        av_packet_free(&pkt);
        printError(ret);
    }
    else if (ret == AVERROR_EOF)
    {
        std::cout << "Encode Video EOF !";
        av_packet_free(&pkt);
    }
    else if (ret < 0)
    {
        printError(ret);
        av_packet_free(&pkt);
        return -1;
    }
    else
    {
        pkt->stream_index = streamIndex;
        pktQueue->enqueue(pkt);
        av_packet_free(&pkt);
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
    if (res < 0)
    {
        qDebug() << "Unknow Error!";
    }
    else
    {
        qDebug() << "Error:" << errorBuffer;
    }
}

void FFVEncoder::initVideo(AVFrame *frame, AVRational fps)
{
    std::lock_guard<std::mutex> lock(mutex);
    resetPtsClock();

    // 参数验证
    if (!frame || frame->width <= 0 || frame->height <= 0)
    {
        std::cerr << "Invalid frame parameters!" << std::endl;
        return;
    }

    // 清理旧参数（防止内存泄漏）
    if (vPars)
    {
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
    if (pixelCount > 1920 * 1080)
    {                                      // 1080p以上
        vPars->biteRate = 4 * 1024 * 1024; // 4Mbps
    }
    else if (pixelCount > 1280 * 720)
    {                                      // 720p以上
        vPars->biteRate = 2 * 1024 * 1024; // 2Mbps
    }
    else
    {
        vPars->biteRate = 1 * 1024 * 1024; // 1Mbps
    }

    // 编码器选择（优先硬件编码器，但添加可用性测试）
    const AVCodec *codec = nullptr;
    std::string encoderName;
    bool useHardwareEncoder = false;

    // 定义编码器候选列表（按优先级排序）
    struct EncoderCandidate
    {
        const char *name;
        const char *description;
        bool isHardware;
    };

    EncoderCandidate candidates[] = {
        {"h264_nvenc", "NVIDIA NVENC", true},
        {"h264_amf", "AMD AMF", true},
        {"h264_qsv", "Intel QSV", true},
        {"libx264", "Software H264", false},
        {nullptr, nullptr, false}};

    // 逐个测试编码器可用性
    for (int i = 0; candidates[i].name != nullptr; i++)
    {
        codec = avcodec_find_encoder_by_name(candidates[i].name);
        if (codec)
        {
            // 创建临时编码器上下文来测试可用性
            AVCodecContext *testCtx = avcodec_alloc_context3(codec);
            if (testCtx)
            {
                // 设置基本参数进行测试
                testCtx->width = 1280;
                testCtx->height = 720;
                testCtx->pix_fmt = AV_PIX_FMT_YUV420P;
                testCtx->time_base = {1, 30};
                testCtx->framerate = {30, 1};
                testCtx->bit_rate = 1000000;

                // 尝试打开编码器（不使用复杂选项）
                int testRet = avcodec_open2(testCtx, codec, nullptr);
                avcodec_free_context(&testCtx);

                if (testRet >= 0)
                {
                    encoderName = candidates[i].description;
                    useHardwareEncoder = candidates[i].isHardware;
                    std::cout << "Selected encoder: " << encoderName
                              << " (" << candidates[i].name << ")" << std::endl;
                    break;
                }
                else
                {
                    std::cout << "Encoder " << candidates[i].name
                              << " found but failed to initialize, trying next..." << std::endl;
                    codec = nullptr;
                }
            }
            else
            {
                std::cout << "Failed to allocate context for " << candidates[i].name << std::endl;
                codec = nullptr;
            }
        }
        else
        {
            std::cout << "Encoder " << candidates[i].name << " not found" << std::endl;
        }
    }

    if (!codec)
    {
        std::cerr << "Find H264 Codec Fail !" << std::endl;
        delete vPars;
        vPars = nullptr;
        return;
    }

    // 清理旧编码器上下文
    if (codecCtx)
    {
        avcodec_free_context(&codecCtx);
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
    {
        std::cerr << "Alloc CodecCtx Fail !" << std::endl;
        delete vPars;
        vPars = nullptr;
        return;
    }

    // 统一的编码器参数配置
    codecCtx->width = vPars->width;
    codecCtx->height = vPars->height;
    codecCtx->bit_rate = vPars->biteRate;
    codecCtx->rc_max_rate = vPars->biteRate * 1.2; // 允许20%的码率波动
    codecCtx->rc_buffer_size = vPars->biteRate;    // 减小缓冲区以降低延迟
    codecCtx->framerate = vPars->frameRate;
    codecCtx->time_base = av_inv_q(vPars->frameRate); // 等同于 {fps.den, fps.num}
    codecCtx->pix_fmt = vPars->videoFmt;
    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // 根据编码器类型优化配置
    if (useHardwareEncoder)
    {
        // 硬件编码器优化配置
        codecCtx->gop_size = 60;    // 硬件编码器可以处理更大的GOP
        codecCtx->max_b_frames = 0; // 简化配置，不使用B帧

        // 硬件编码器通常不需要多线程
        codecCtx->thread_count = 1;
        codecCtx->thread_type = 0;
    }
    else
    {
        // 软件编码器低延迟配置
        codecCtx->gop_size = 30;    // 减小GOP以降低延迟
        codecCtx->max_b_frames = 0; // 无B帧以减少延迟
        codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

        // 软件编码器多线程优化 - 使用更多线程
        int thread_count = std::thread::hardware_concurrency();
        codecCtx->thread_count = std::max(1, std::min(thread_count, 16)); // 最多16线程
        codecCtx->thread_type = FF_THREAD_FRAME;                          // 只使用帧级并行，避免slice并行的开销

        // 设置编码器特定的线程参数
        codecCtx->slices = codecCtx->thread_count; // 每个线程一个slice
    }

    // 通用性能优化
    codecCtx->keyint_min = codecCtx->gop_size;

    // 质量与速度平衡 - 为软件编码器优化
    if (useHardwareEncoder)
    {
        codecCtx->qmin = 10;
        codecCtx->qmax = 40;
        codecCtx->max_qdiff = 4;
    }
    else
    {
        // 软件编码器：放宽质量限制以提升速度
        codecCtx->qmin = 18; // 提高最小质量值
        codecCtx->qmax = 45; // 提高最大质量值
    }

    AVDictionary *codec_options = nullptr;

    // 根据编码器类型设置专用选项（简化版本）
    if (useHardwareEncoder)
    {
        if (strstr(codec->name, "nvenc"))
        {
            // NVENC 基础选项
            av_dict_set(&codec_options, "preset", "fast", 0); // 使用更兼容的预设
            av_dict_set(&codec_options, "rc", "cbr", 0);      // 恒定码率
        }
        else if (strstr(codec->name, "amf"))
        {
            // AMD AMF 基础选项
            av_dict_set(&codec_options, "quality", "speed", 0); // 速度优先
            av_dict_set(&codec_options, "rc", "cbr", 0);        // 恒定码率
        }
        else if (strstr(codec->name, "qsv"))
        {
            // Intel QSV 基础选项
            av_dict_set(&codec_options, "preset", "fast", 0); // 快速预设
        }
    }
    else
    {
        // 软件编码器高速选项
        av_dict_set(&codec_options, "preset", "ultrafast", 0); // 最快预设
        av_dict_set(&codec_options, "tune", "zerolatency", 0); // 零延迟调优
        av_dict_set(&codec_options, "x264-params", "no-cabac:ref=1:deblock=0:me=dia:subme=0:rc-lookahead=0:weightp=0:mixed-refs=0:8x8dct=0:trellis=0", 0);
    }

    int ret = avcodec_open2(codecCtx, codec, &codec_options);
    if (ret < 0)
    {
        std::cerr << "Open codec failed: ";
        printError(ret);
        avcodec_free_context(&codecCtx);
        delete vPars;
        vPars = nullptr;
        av_dict_free(&codec_options);
        return;
    }

    // 检查未使用的选项
    if (codec_options)
    {
        AVDictionaryEntry *t = nullptr;
        std::cout << "Unused codec options:" << std::endl;
        while ((t = av_dict_get(codec_options, "", t, AV_DICT_IGNORE_SUFFIX)))
        {
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
