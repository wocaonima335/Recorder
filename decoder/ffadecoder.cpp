#include "ffadecoder.h"
#include "queue/ffaframequeue.h"
#include "resampler/ffaresampler.h"

FFADecoder::FFADecoder()
    : m_stop(false)
{}

FFADecoder::~FFADecoder()
{
    close();
}

void FFADecoder::decode(AVPacket *packet)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (codecCtx == nullptr) {
        return;
    }

    if (packet) {
        std::cerr << "[ADec] decode enter, pkt=" << packet << " pts=" << packet->pts
                  << " dts=" << packet->dts << std::endl;
    } else {
        std::cerr << "[ADec] decode enter, pkt=null (flush)" << std::endl;
    }

    int ret = avcodec_send_packet(codecCtx, packet);

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        printError(ret);
        avcodec_free_context(&codecCtx);
        return;
    }
    AVFrame *frame = av_frame_alloc();
    while (ret >= 0) {
        if (m_stop.load(std::memory_order_acquire)) {
            std::cerr << "[ADec] m_stop observed, break receive-loop" << std::endl;
            break;
        }

        ret = avcodec_receive_frame(codecCtx, frame);

        std::cerr << "[ADec] receive_frame ret=" << ret;
        if (ret == AVERROR(EAGAIN))
            std::cerr << " (EAGAIN)";
        if (ret == AVERROR_EOF)
            std::cerr << " (EOF)";
        std::cerr << std::endl;

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
            if (aPars == nullptr) {
                aPars = new FFAudioPars();
                swraPars = new FFAudioPars();
                initAudioPars(frame);
                if (aPars->aFormatEnum != swraPars->aFormatEnum) {
                    resampler = new FFAResampler();
                    initResampler();
                    printFmt();
                }
            }

            if (resampler) {
                AVFrame *swrFrame = nullptr;
                resampler->resample(frame, &swrFrame);
                av_frame_unref(frame);
                if (m_stop.load(std::memory_order_acquire)) {
                    av_frame_unref(frame);
                    av_frame_free(&swrFrame);
                    m_stop.store(false, std::memory_order_release);
                    break;
                } else {
                    std::cout << "audio frame pts:" << swrFrame->pts << std::endl;

                    if (frmQueue != nullptr)
                        frmQueue->enqueue(swrFrame);
                    AVFrameTraits::release(swrFrame);
                }
            } else {
                if (m_stop.load(std::memory_order_acquire)) {
                    av_frame_unref(frame);
                    m_stop.store(false, std::memory_order_release);
                    break;
                } else {
                    // 无重采样分支打印 pts
                    std::cerr << "ADec][nosws] frame pts=" << frame->pts << std::endl;
                    frmQueue->enqueue(frame);
                }
            }
        }
    }
    av_frame_free(&frame);
}

void FFADecoder::init(AVStream *stream_, FFAFrameQueue *frmQueue_)
{
    std::lock_guard<std::mutex> lock(mutex);
    m_stop.store(false, std::memory_order_release);
    stream = stream_;
    frmQueue = frmQueue_;

    if (stream->codecpar == nullptr) {
        return;
    }

    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);

    if (codec == nullptr) {
        std::cerr << "找不到音频解码器" << std::endl;
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        std::cerr << "分配解码器上下文失败" << std::endl;
        return;
    }

    int ret = avcodec_parameters_to_context(codecCtx, stream->codecpar);
    if (ret < 0) {
        printError(ret);
        avcodec_free_context(&codecCtx);
        return;
    }

    AVDictionary *codec_options = nullptr;

    ret = avcodec_open2(codecCtx, codec, &codec_options);
    if (ret < 0) {
        printError(ret);
        avcodec_free_context(&codecCtx);
        return;
    }
}

void FFADecoder::flushDecoder()
{
    avcodec_flush_buffers(codecCtx);
}

FFAudioPars *FFADecoder::getAudioPars()
{
    return swraPars;
}

int FFADecoder::getTotalSec()
{
    return static_cast<int>(0.5 + stream->duration * av_q2d(stream->time_base));
}

void FFADecoder::wakeAllThreads()
{
    frmQueue->wakeAllThread();
}

void FFADecoder::stop()
{
    m_stop.store(true, std::memory_order_release);
}

void FFADecoder::enqueueNull()
{
    frmQueue->enqueueNull();
}

void FFADecoder::flushQueue()
{
    frmQueue->flushQueue();
}

void FFADecoder::close()
{
    decode(nullptr);
    stop();
    std::lock_guard<std::mutex> lock(mutex);
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
    }
    if (aPars) {
        delete aPars;
        aPars = nullptr;
    }
    if (swraPars) {
        delete swraPars;
        swraPars = nullptr;
    }

    if (resampler) {
        delete resampler;
        resampler = nullptr;
    }
}

AVCodecContext *FFADecoder::getCodecCtx()
{
    return codecCtx;
}

AVStream *FFADecoder::getStream()
{
    return stream;
}

void FFADecoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof(errorBuffer));
    if (res < 0) {
        std::cerr << "UnKonw Error!" << std::endl;
    } else {
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}

void FFADecoder::initAudioPars(AVFrame *frame)
{
    aPars->timeBase = stream->time_base;
    aPars->nbChannels = frame->ch_layout.nb_channels;
    aPars->aFormatEnum = codecCtx->sample_fmt;
    aPars->sampleRate = frame->sample_rate = 48000;
    aPars->sampleSize = av_get_bytes_per_sample(codecCtx->sample_fmt);
    aPars->sampleRate = frame->sample_rate;

    memcpy(swraPars, aPars, sizeof(FFAudioPars));
    swraPars->aFormatEnum = AV_SAMPLE_FMT_FLTP;
    swraPars->sampleSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP);
    swraPars->sampleRate = 48000;
}

void FFADecoder::initResampler()
{
    resampler->init(aPars, swraPars);
}

void FFADecoder::printFmt()
{
    std::cout << "audio format : " << av_get_sample_fmt_name(aPars->aFormatEnum) << std::endl;
    std::cout << "sample_rate : " << aPars->sampleRate << std::endl;
    std::cout << "channels : " << aPars->nbChannels << std::endl;
    std::cout << "time_base : " << aPars->timeBase.num << " / " << aPars->timeBase.den << std::endl;

    std::cout << "=================" << std::endl;
    std::cout << "audio format : " << av_get_sample_fmt_name(swraPars->aFormatEnum) << std::endl;
    std::cout << "sample_rate : " << swraPars->sampleRate << std::endl;
    std::cout << "channels : " << swraPars->nbChannels << std::endl;
    std::cout << "time_base : " << swraPars->timeBase.num << " / " << swraPars->timeBase.den
              << std::endl;
}
