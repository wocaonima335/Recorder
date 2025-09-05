#include "ffaencoder.h"
#include "queue/ffaframequeue.h"
#include "queue/ffapacketqueue.h"

FFAEncoder::FFAEncoder() {}

FFAEncoder::~FFAEncoder()
{
    close();
}

void FFAEncoder::init(FFAPacketQueue *pktQueue_)
{
    pktQueue = pktQueue_;
}

void FFAEncoder::close()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }

    if (aPars) {
        delete aPars;
        aPars = nullptr;
    }
    clearPendingFrame();
}

void FFAEncoder::wakeAllThread()
{
    if (pktQueue) {
        pktQueue->wakeAllThread();
    }
}

int FFAEncoder::encode(AVFrame *frame, int streamIndex, int64_t pts, AVRational timeBase)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (frame == nullptr || codecCtx == nullptr) {
        return 0;
    }
    int frame_size = codecCtx->frame_size;
    int input_samples = frame->nb_samples;
    int bytes_per_sample = av_get_bytes_per_sample(codecCtx->sample_fmt);

    std::vector<uint8_t> merged_data[8];
    int total_samples = pendingFrame.samples + input_samples;

    for (int ch = 0; ch < codecCtx->ch_layout.nb_channels; ++ch) {
        merged_data[ch].resize(total_samples * bytes_per_sample);
        if (pendingFrame.samples > 0) {
            mempcpy(merged_data[ch].data(),
                    pendingFrame.data[ch].data(),
                    pendingFrame.samples * bytes_per_sample);
        }

        memcpy(merged_data[ch].data() + pendingFrame.samples * bytes_per_sample,
               frame->data[ch],
               input_samples * bytes_per_sample);
    }

    int total_full_frames = total_samples / frame_size;
    int remaining_samples = total_samples % frame_size;

    for (int i = 0; i < total_full_frames; i++) {
        AVFrame *sub_frame = av_frame_alloc();
        sub_frame->format = codecCtx->sample_fmt;
        sub_frame->ch_layout = codecCtx->ch_layout;
        sub_frame->ch_layout.nb_channels = codecCtx->ch_layout.nb_channels;
        sub_frame->sample_rate = codecCtx->sample_rate;
        sub_frame->nb_samples = frame_size;
        sub_frame->pts = pendingFrame.next_pts + i * frame_size;

        av_frame_get_buffer(sub_frame, 0);

        for (int ch = 0; ch < sub_frame->ch_layout.nb_channels; ch++) {
            uint8_t *src = merged_data[ch].data() + i * frame_size * bytes_per_sample;
            memcpy(sub_frame->data[ch], src, frame_size * bytes_per_sample);
        }

        avcodec_send_frame(codecCtx, sub_frame);
        av_frame_free(&sub_frame);

        while (1) {
            AVPacket *pkt = av_packet_alloc();
            int ret = avcodec_receive_packet(codecCtx, pkt);

            if (ret == AVERROR(EAGAIN)) {
                av_packet_free(&pkt);
                break;
            } else if (ret < 0) {
                av_packet_free(&pkt);
                return -1;
            }

            pkt->stream_index = streamIndex;
            pktQueue->enqueue(pkt);
        }
    }

    pendingFrame.next_pts = pendingFrame.next_pts + total_full_frames * frame_size;
    pendingFrame.samples = remaining_samples;
    for (int ch = 0; ch < codecCtx->ch_layout.nb_channels; ++ch) {
        pendingFrame.data[ch].resize(remaining_samples * bytes_per_sample);
        if (remaining_samples > 0) {
            uint8_t *src = merged_data[ch].data()
                           + total_full_frames * frame_size * bytes_per_sample;
            memcpy(pendingFrame.data[ch].data(), src, remaining_samples * bytes_per_sample);
        }
    }
    return 0;
}

FFAEncoderPars *FFAEncoder::getEncoderPars()
{
    std::lock_guard<std::mutex> lock(mutex);
    return aPars;
}

AVCodecContext *FFAEncoder::getCodecCtx()
{
    return codecCtx;
}

void FFAEncoder::initAudio(AVFrame *frame)
{
    std::lock_guard<std::mutex> lock(mutex);

    aPars = new FFAEncoderPars();
    aPars->biteRate = 64 * 1024;
    aPars->nbChannel = frame->ch_layout.nb_channels;
    aPars->sampleRate = frame->sample_rate;
    aPars->audioFmt = AV_SAMPLE_FMT_FLTP;

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (codec == nullptr) {
        std::cerr << "Find AAC Codec Fail !" << std::endl;
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        std::cerr << "Alloc CodecCtx Fail !" << std::endl;
        return;
    }

    codecCtx->bit_rate = aPars->biteRate;
    codecCtx->ch_layout.nb_channels = aPars->nbChannel;
    av_channel_layout_default(&codecCtx->ch_layout, aPars->nbChannel);
    codecCtx->sample_rate = aPars->sampleRate;
    codecCtx->sample_fmt = aPars->audioFmt;
    codecCtx->time_base = AVRational{1, aPars->sampleRate};
    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        printError(ret);
        return;
    }
}

void FFAEncoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof errorBuffer);
    if (res < 0) {
        std::cerr << "Unknow Error!" << std::endl;
    } else {
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}
