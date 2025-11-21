#include "ffvdecoder.h"
#include "queue/ffvframequeue.h"
#include "resampler/ffvresampler.h"

#include <iostream>

FFVDecoder::~FFVDecoder()
{
    close();
}

void FFVDecoder::decode(AVPacket *packet)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (codecCtx == nullptr) {
        return;
    }

    // // 增加解码入口打印
    // if (packet) {
    //     std::cerr << "[VDec] decode enter, pkt=" << packet
    //               << " pts=" << packet->pts << " dts=" << packet->dts << std::endl;
    // } else {
    //     std::cerr << "[VDec] decode enter, pkt=null (flush)" << std::endl;
    // }

    int ret = avcodec_send_packet(codecCtx, packet);
    // 打印 send_packet 返回值
    // std::cerr << "[VDec] send_packet ret=" << ret;
    // if (ret == AVERROR(EAGAIN))
    //     std::cerr << " (EAGAIN)";
    // if (ret == AVERROR_EOF)
    //     std::cerr << " (EOF)";
    // std::cerr << std::endl;

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        printError(ret);
        avcodec_free_context(&codecCtx);
        return;
    }

    AVFrame *frame = av_frame_alloc();
    while (ret >= 0) {
        if (m_stop.load(std::memory_order_acquire)) {
            qDebug() << "[VDec] m_stop observed, break receive-loop";
            break;
        }

        ret = avcodec_receive_frame(codecCtx, frame);
        // 打印 receive_frame 返回值
        // std::cerr << "[VDec] receive_frame ret=" << ret;
        // if (ret == AVERROR(EAGAIN)) std::cerr << " (EAGAIN)";
        // if (ret == AVERROR_EOF) std::cerr << " (EOF)";
        // std::cerr << std::endl;

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                break;
            } else if (ret == AVERROR(EAGAIN)) {
                continue;
            } else {
                printError(ret);
                av_frame_free(&frame);
                avcodec_free_context(&codecCtx);
                return;
            }
        } else {
            if (vPars == nullptr) {
                vPars = new FFVideoPars();
                swsvPars = new FFVideoPars();

                initVideoPars(frame);
                if (vPars->pixFmtEnum != swsvPars->pixFmtEnum) {
                    resampler = new FFVResampler();
                    initResampler();
                }
                // 初始化视频参数后打印一次
                qDebug() << "[VDec] initVideoPars: w=" << frame->width << " h=" << frame->height
                         << " in_pixfmt=" << codecCtx->pix_fmt
                         << " out_pixfmt=" << swsvPars->pixFmtEnum
                         << " fr=" << codecCtx->framerate.num << "/" << codecCtx->framerate.den;
            }

            if (resampler) {
                AVFrame *swsFrame = nullptr;
                resampler->resample(frame, &swsFrame);
                av_frame_unref(frame);
                if (m_stop.load(std::memory_order_acquire)) {
                    av_frame_unref(swsFrame);
                    av_frame_free(&swsFrame);
                    m_stop.store(false, std::memory_order_release);
                    break;
                } else {
                    // 打印重采样后帧 pts
                    // if (swsFrame) {
                    //     std::cerr << "[VDec][sws] frame pts=" << swsFrame->pts << std::endl;
                    // }
                    //解码队列
                    AVFrame *decoderFrame = av_frame_clone(swsFrame);
                    qDebug() << "video frame pts: " << decoderFrame->pts;
                    if (frmQueue != nullptr) {
                        frmQueue->enqueue(decoderFrame);
                    }
                    AVFrameTraits::release(decoderFrame);
                    AVFrameTraits::release(swsFrame);
                }
            } else {
                if (m_stop.load(std::memory_order_acquire)) {
                    av_frame_unref(frame);
                    break;
                } else {
                    qDebug() << "[VDec][nosws] frame pts=" << frame->pts;

                    if (frmQueue) {
                        AVFrame *decoderFrame = av_frame_clone(frame);
                        frmQueue->enqueue(decoderFrame);
                        av_frame_free(&decoderFrame);
                        qDebug() << "VIDEO decoder run !";
                    }

                    av_frame_unref(frame);
                    qDebug() << "decoder eunqueue!";
                }
            }
        }
    }
    av_frame_free(&frame);
}

void FFVDecoder::init(AVStream *stream_, FFVFrameQueue *frmQueue_)
{
    std::lock_guard<std::mutex> lock(mutex);
    stream = stream_;
    frmQueue = frmQueue_;
    m_stop.store(false, std::memory_order_release);

    const AVCodec *codec = nullptr;
    if (stream->codecpar == nullptr) {
        return;
    }

    if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
        codec = avcodec_find_decoder_by_name("h264");
        hardDecodeFlag = false;
    } else {
        codec = avcodec_find_decoder(stream->codecpar->codec_id);
    }

    if (codec == nullptr) {
        qDebug() << "找不到视频解码器";
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        qDebug() << "分配解码器上下文失败";
        avcodec_free_context(&codecCtx);
        return;
    }

    int ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
    if (ret < 0) {
        printError(ret);
        avcodec_free_context(&codecCtx);
        return;
    }
    AVDictionary *codec_options = nullptr;
    if (!hardDecodeFlag) {
        codecCtx->thread_count = av_cpu_count();
        av_dict_set(&codec_options, "fast", "1", 0);
    } else {
        av_dict_set(&codec_options, "low_latency", "1", 0);
    }

    ret = avcodec_open2(codecCtx, codec, &codec_options);

    if (ret < 0) {
        printError(ret);
        avcodec_free_context(&codecCtx);
        return;
    }
    av_dict_free(&codec_options);
}

void FFVDecoder::flushDecoder()
{
    avcodec_flush_buffers(codecCtx);
}

int FFVDecoder::getTotalSec()
{
    return static_cast<int>(stream->duration * av_q2d(stream->time_base));
}

FFVideoPars *FFVDecoder::getVideoPars()
{
    return swsvPars;
}

AVCodecContext *FFVDecoder::getCodecCtx()
{
    return codecCtx;
}

AVStream *FFVDecoder::getStream()
{
    return stream;
}

void FFVDecoder::enqueueNull()
{
    frmQueue->enqueueNull();
}

void FFVDecoder::wakeAllThread()
{
    frmQueue->wakeAllThread();
}

void FFVDecoder::stop()
{
    m_stop.store(true, std::memory_order_release);
}

void FFVDecoder::flushQueue()
{
    frmQueue->flushQueue();
}

void FFVDecoder::close()
{
    decode(nullptr);
    stop();

    std::lock_guard<std::mutex> lock(mutex);
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
    }
    if (vPars) {
        delete vPars;
        vPars = nullptr;
    }
    if (resampler) {
        delete resampler;
        resampler = nullptr;
    }
    if (swsvPars) {
        delete swsvPars;
        swsvPars = nullptr;
    }

    qDebug() << "video Deocder close!";
}

FFVDecoder::FFVDecoder()
    : m_stop(false)
{}

void FFVDecoder::initResampler()
{
    resampler->init(vPars, swsvPars);
}

void FFVDecoder::initVideoPars(AVFrame *frame)
{
    vPars->timeBase = stream->time_base;
    vPars->pixFmtEnum = codecCtx->pix_fmt;
    vPars->frameRate = codecCtx->framerate;

    if (codecCtx->framerate.den == 0 || codecCtx->framerate.num == 0) {
        vPars->frameRate = stream->avg_frame_rate;
        codecCtx->framerate = stream->avg_frame_rate; //调整frameRate
    }

    vPars->width = frame->width;
    vPars->height = frame->height;

    memcpy(swsvPars, vPars, sizeof(FFVideoPars));
    swsvPars->pixFmtEnum = AV_PIX_FMT_YUV420P;
}

void FFVDecoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof errorBuffer);
    if (res < 0) {
        std::cerr << "Unknow Error!" << std::endl;
    } else {
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}
