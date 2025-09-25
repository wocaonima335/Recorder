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
        std::cerr << "[Demux] fmtCtx is null, return -1" << std::endl;
        return -1;
    }

    int ret = av_read_frame(fmtCtx, packet);

    // 打印读取结果
    static int read_cnt = 0;
    std::cerr << "[Demux] av_read_frame ret=" << ret << " count=" << ++read_cnt << std::endl;

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
            std::cerr << "[Demux] EOF, enqueueNull A/V, return 1" << std::endl;
            return 1;
        } else {
            char err[256] = {0};
            av_strerror(ret, err, sizeof(err));
            std::cerr << "[Demux][ERR] ret=" << ret << " msg=" << err << " -> close fmtCtx" << std::endl;
            avformat_close_input(&fmtCtx);
            av_packet_free(&packet);
            return -1;
        }
    }

    if (stopFlag) {
        std::cerr << "[Demux] stopFlag set, return 0" << std::endl;
        return 0;
    }

    if (aStream && packet->stream_index == aStream->index) {
        if (aPktQueue) {
            static int a_cnt = 0;
            std::cerr << "[Demux] A pkt idx=" << packet->stream_index
                      << " pts=" << packet->pts << " dts=" << packet->dts
                      << " dur=" << packet->duration << " size=" << packet->size
                      << " count=" << ++a_cnt << std::endl;
            aPktQueue->enqueue(packet);
            av_packet_free(&packet);
        } else {
            av_packet_unref(packet);
            av_packet_free(&packet);
        }
    } else if (vStream && packet->stream_index == vStream->index) {
        if (vPktQueue) {
            static int v_cnt = 0;
            std::cerr << "[Demux] V pkt idx=" << packet->stream_index
                      << " pts=" << packet->pts << " dts=" << packet->dts
                      << " dur=" << packet->duration << " size=" << packet->size
                      << " count=" << ++v_cnt << std::endl;
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

    if (!inputFmt) {
        std::cerr << "Cannot find input format: " << format << std::endl;
        return;
    }
    // 假设 url = "desktop"，inputFmt = av_find_input_format("gdigrab")
    AVDictionary *opts = nullptr;

    // 1) 设置偶数化分辨率，避免后续 x264 报 "height not divisible by 2"
    //    注意：宽高必须都在虚拟桌面范围之内
    av_dict_set(&opts, "video_size", "3840x1266", 0);

    // 2) 固定帧率
    av_dict_set(&opts, "framerate", "30", 0);

    // 3) 关键：与日志一致的起点坐标，确保截取区域不会越界（根据你的日志：x=0, y=-187）
    av_dict_set(&opts, "offset_x", "0", 0);
    av_dict_set(&opts, "offset_y", "-187", 0);

    // 4) 可选：绘制鼠标
    // av_dict_set(&opts, "draw_mouse", "1", 0);

    int ret = avformat_open_input(&fmtCtx, url.c_str(), inputFmt, &opts);
    av_dict_free(&opts); // 无论成功与否都释放字典
    if (ret == AVERROR(EIO)) {
        // 回退策略：去掉裁剪，仅设帧率，避免区域越界导致的 -5
        AVDictionary *fallback = nullptr;
        av_dict_set(&fallback, "framerate", "30", 0);
        ret = avformat_open_input(&fmtCtx, url.c_str(), inputFmt, &fallback);
        av_dict_free(&fallback);
    }
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
    av_dict_free(&opts);
}

int Demuxer::getType()
{
    return type;
}

void Demuxer::PrintDshowDevices()
{
    // 枚举 gdigrab 的“源”（gdigrab 一般不实现设备列表，下面加了回退方案）
    const AVInputFormat *fmt = av_find_input_format("gdigrab");
    if (!fmt) {
        std::cerr << "gdigrab input format not found, your FFmpeg may not include gdigrab."
                  << std::endl;
        return;
    }

    // 1) 先尝试通过设备 API 列举（很多版本可能返回 0 或不支持）
    AVDeviceInfoList *list = nullptr;
    int count = avdevice_list_input_sources(const_cast<AVInputFormat *>(fmt),
                                            nullptr,
                                            nullptr,
                                            &list);

    if (count > 0 && list) {
        std::cout << "Found " << list->nb_devices << " gdigrab sources:" << std::endl;
        for (int i = 0; i < list->nb_devices; ++i) {
            AVDeviceInfo *info = list->devices[i];
            const char *name = (info && info->device_name) ? info->device_name : "";
            const char *desc = (info && info->device_description) ? info->device_description : "";
            std::cout << "  [" << i << "] name=" << name << " | desc=" << desc << std::endl;
        }
        avdevice_free_list_devices(&list);
        return;
    }
    avdevice_free_list_devices(&list);

    // 2) 回退方案：让 gdigrab 把可抓取窗口列表打印到日志（等价于 CLI: ffmpeg -f gdigrab -list_windows 1 -i desktop）
    AVFormatContext *tmpCtx = nullptr;
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "list_windows", "1", 0);
    av_log_set_level(AV_LOG_INFO); // 确保能看到窗口列表输出

    int ret = avformat_open_input(&tmpCtx, "desktop", fmt, &opts);
    if (ret < 0) {
        char err[256] = {0};
        av_strerror(ret, err, sizeof(err));
        std::cerr << "gdigrab list_windows failed: " << err << std::endl;
    } else {
        // gdigrab 已在日志中打印了窗口列表，这里立即关闭
        avformat_close_input(&tmpCtx);
    }
    av_dict_free(&opts);
}

void Demuxer::printError(int ret)
{
    {
        char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
        int res = av_strerror(ret, errorBuffer, sizeof errorBuffer);
        if (res < 0) {
            std::cerr << "Unknow Error!" << std::endl;
        } else {
            std::cerr << "Error:" << errorBuffer << std::endl;
        }
    }
}
