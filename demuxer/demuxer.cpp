#include "demuxer.h"

#include "queue/ffapacketqueue.h"
#include "queue/ffvpacketqueue.h"

Demuxer::Demuxer() {}

Demuxer::~Demuxer() {}

void Demuxer::init(const string &url_,
                   const string &format_,
                   FFAPacketQueue *aPktQueue_,
                   FFVPacketQueue *vPktQueue_,
                   int type_)
{
    std::lock_guard<std::mutex> lock(mutex);
    url = url_;
    format = format_;
    aPktQueue = aPktQueue_;
    vPktQueue = vPktQueue_;
    type = type_;
    stopFlag = false;
    initDemuxer();
}

int Demuxer::demux()
{
    std::lock_guard<std::mutex> lock(mutex);
    AVPacket *packet = av_packet_alloc();

    if (fmtCtx == nullptr) {
        return -1;
    }

    int ret = av_read_frame(fmtCtx, packet);

    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            if (aPktQueue) {
                aPktQueue->enqueueNull();
                av_packet_unref(packet);
                av_packet_free(&packet);
            }

            if (vPktQueue) {
                vPktQueue->enqueueNull();
                av_packet_unref(packet);
                av_packet_free(&packet);
            }
            std::cout << "AVERROR_EOF" << endl;
            return 1;
        } else {
            printError(ret);
            avformat_close_input(&fmtCtx);
            av_packet_free(&packet);
            return -1;
        }
    }

    if (stopFlag) {
        return 0;
    }

    if (aStream && packet->stream_index == aStream->index) {
        if (aPktQueue) {
            aPktQueue->enqueue(packet);
            av_packet_free(&packet);
        } else {
            av_packet_unref(packet);
            av_packet_free(&packet);
        }
    } else if (vStream && packet->stream_index == vStream->index) {
        if (vPktQueue) {
            vPktQueue->enqueue(packet);
            av_packet_free(&packet);
        } else {
            av_packet_unref(packet);
            av_packet_free(&packet);
        }
    }
    return 0;
}

AVStream *Demuxer::getAStream()
{
    std::lock_guard<std::mutex> lock(mutex);
    return aStream;
}

AVStream *Demuxer::getVStream()
{
    std::lock_guard<std::mutex> lock(mutex);
    return vStream;
}

void Demuxer::wakeAllThread()
{
    if (vPktQueue) {
        vPktQueue->wakeAllThread();
    } else {
        aPktQueue->wakeAllThread();
    }
}

void Demuxer::close()
{
    std::lock_guard<std::mutex> lock(mutex);
    stopFlag = true;
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }

    if (opts) {
        av_dict_free(&opts);
    }
}

void Demuxer::initDemuxer()
{
    std::cout << "url = " << url << std::endl;
    avformat_network_init();
    avdevice_register_all();

    inputFmt = av_find_input_format(format.c_str());

    int ret = avformat_open_input(&fmtCtx, url.c_str(), inputFmt, &opts);
    if (ret < 0) {
        avformat_close_input(&fmtCtx);
        printError(ret);
        return;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        avformat_close_input(&fmtCtx);
        printError(ret);
        return;
    }

    for (size_t i = 0; i < fmtCtx->nb_streams; ++i) {
        AVStream *stream = fmtCtx->streams[i];
        AVCodecParameters *codecPar = stream->codecpar;

        if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO) {
            aStream = stream;
            aTimeBase = stream->time_base;
        } else if (codecPar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vStream = stream;
            vTimeBase = stream->time_base;
        }
    }
}

int Demuxer::getType()
{
    return type;
}
