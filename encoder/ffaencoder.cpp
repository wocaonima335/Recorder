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
    }
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
