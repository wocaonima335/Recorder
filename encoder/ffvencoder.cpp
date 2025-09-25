#include "ffvencoder.h"
#include "queue/ffvpacketqueue.h"

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
    std::lock_guard<std::mutex> lock(mutex);

    if (frame == nullptr || codecCtx == nullptr) {
        std::cout << "nullptr" << std::endl;
        return 0;
    }

    pts = av_rescale_q_rnd(pts, timeBase, codecCtx->time_base, AV_ROUND_NEAR_INF);
    if (pts <= lastPts) {
        pts = lastPts + 1;
    }

    frame->pts = pts;

    lastPts = pts;

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
            av_frame_unref(frame);
            av_frame_free(&frame);
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

    vPars = new FFVEncoderPars();
    vPars->biteRate = 2 * 1024 * 1024;
    vPars->width = frame->width;
    vPars->height = frame->height;
    vPars->videoFmt = AV_PIX_FMT_YUV420P;
    vPars->frameRate = fps;

#if 1
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
#else
    const AVCodec *codec = avcodec_find_encoder_by_name("h264_amf");
#endif

    if (codec == nullptr) {
        std::cerr << "Find H264 Codec Fail !" << std::endl;
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        std::cerr << "Alloc CodecCtx Fail !" << std::endl;
        return;
    }

    codecCtx->width = vPars->width;
    codecCtx->height = vPars->height;
    codecCtx->framerate = vPars->frameRate;
    codecCtx->time_base = AVRational{vPars->frameRate.den, vPars->frameRate.num};
    codecCtx->pix_fmt = vPars->videoFmt;
    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //不设置就没有封面

    codecCtx->max_b_frames = 0;
    codecCtx->gop_size = 12;
    codecCtx->keyint_min = 12;
    codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

#if 0
    codecCtx->thread_type = FF_THREAD_FRAME;
    codecCtx->thread_count = 8;
#endif

    AVDictionary *codec_options = nullptr;

#if 0
    av_dict_set(&codec_options, "usage", "3", 0);
    av_dict_set(&codec_options,"max_b_frames","0",0);
    av_dict_set(&codec_options,"latency","true",0);

#else
    av_dict_set(&codec_options, "tune", "zerolatency", 0);
    av_dict_set(&codec_options, "preset", "ultrafast", 0);
#endif

    int ret = avcodec_open2(codecCtx, codec, &codec_options);
    if (ret < 0) {
        printError(ret);
        return;
    }
    av_dict_free(&codec_options);
}
