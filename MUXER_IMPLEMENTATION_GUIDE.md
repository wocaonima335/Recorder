# FFMuxer 和 FFMuxerThread 优化实现方案

## 一、FFMuxer 升级方案（核心改进）

### 改进点：

1. **DTS 单调性保证**
2. **无效时间戳补偿**
3. **流状态跟踪**
4. **文件大小限制**
5. **统计信息收集**

### 建议的新 Header

```cpp
// ffmuxer.h - 增量修改

#ifndef FFMUXER_H
#define FFMUXER_H

#include <atomic>
#include <chrono>
#include <iostream>
#include <shared_mutex>
#include <thread>
#include <map>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

// ===== 新增：流状态结构 =====
struct MuxStreamState {
    int64_t last_mux_dts = -1;      // 最后写入的 DTS
    int64_t last_mux_pts = -1;      // 最后写入的 PTS
    int64_t next_expected_pts = 0;  // 用于补偿无效 PTS
    int64_t frame_duration = 0;     // 从 fps/sample_rate 计算
};

// ===== 新增：复用统计结构 =====
struct MuxStats {
    int64_t video_size = 0;
    int64_t audio_size = 0;
    int64_t video_frames = 0;
    int64_t audio_frames = 0;
    int64_t total_packets = 0;
    int64_t dropped_packets = 0;
    std::chrono::steady_clock::time_point start_time;
};

class FFMuxer
{
public:
    FFMuxer();
    ~FFMuxer();

    void init(const std::string &url_, std::string const &format_ = "mp4");
    void addStream(AVCodecContext *codecCtx);

    // ===== 改进的 mux 接口 =====
    int mux(AVPacket *packet);

    void writeHeader();
    void writeTrailer();

    int getAStreamIndex();
    int getVStreamIndex();

    // ===== 新增：配置接口 =====
    void setMaxFileSize(int64_t bytes) { max_filesize = bytes; }
    void printStats() const;

    void close();

private:
    void initMuxer();
    void printError(int ret);

    // ===== 新增：时间戳修正函数（参考 FFmpeg mux_fixup_ts）=====
    void mux_fixup_ts(AVPacket *pkt, int streamIndex);

    // ===== 新增：无效时间戳补偿 =====
    void compensate_invalid_ts(AVPacket *pkt, int streamIndex);

private:
    std::string url;
    std::string format;

    std::atomic<bool> headerFlag{false};
    std::atomic<bool> trailerFlag{false};
    std::atomic<bool> readyFlag{false};
    std::atomic<int> streamCount{0};

    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *aCodecCtx = nullptr;
    AVCodecContext *vCodecCtx = nullptr;

    AVStream *aStream = nullptr;
    AVStream *vStream = nullptr;

    int aStreamIndex = -1;
    int vStreamIndex = -1;

    // ===== 新增：流状态管理 =====
    std::map<int, MuxStreamState> stream_states;

    // ===== 新增：文件大小限制 =====
    int64_t max_filesize = 4LL * 1024 * 1024 * 1024;  // 4GB 默认

    // ===== 新增：统计信息 =====
    MuxStats stats;

    std::shared_mutex mutex;
};

#endif // FFMUXER_H
```

### 建议的新实现

```cpp
// ffmuxer.cpp - 关键改进

// ===== 1. 时间戳修正（核心改进，参考 FFmpeg mux_fixup_ts） =====
void FFMuxer::mux_fixup_ts(AVPacket *pkt, int streamIndex)
{
    auto& state = stream_states[streamIndex];

    // 第1步：DTS > PTS 修正（无效时间戳对）
    if (pkt->dts > pkt->pts) {
        // 采用中位数算法（参考 FFmpeg）
        int64_t pts = pkt->pts;
        int64_t dts = pkt->dts;
        int64_t last_dts = state.last_mux_dts;

        // 三值中位数
        int64_t max3 = std::max({pts, dts, last_dts});
        int64_t min3 = std::min({pts, dts, last_dts});
        int64_t mid3 = pts + dts + last_dts - max3 - min3;

        pkt->pts = pkt->dts = mid3;

        qWarning() << "[Muxer] Fixed invalid dts>pts: adjusted to" << mid3;
    }

    // 第2步：DTS 单调性保证（最关键的修正）
    // 确保 DTS 严格单调递增，这对某些容器格式（如 MP4）至关重要
    if (state.last_mux_dts >= 0 && pkt->dts < state.last_mux_dts) {
        int64_t min_dts = state.last_mux_dts + 1;

        if (pkt->pts >= pkt->dts) {
            pkt->pts = std::max(pkt->pts, min_dts);
        }
        pkt->dts = min_dts;

        qWarning() << "[Muxer] Fixed dts regression: adjusted from"
                  << state.last_mux_dts << "to" << min_dts;
    }

    state.last_mux_dts = pkt->dts;
    state.last_mux_pts = pkt->pts;
}

// ===== 2. 无效时间戳补偿（比之前的丢弃好得多） =====
void FFMuxer::compensate_invalid_ts(AVPacket *pkt, int streamIndex)
{
    auto& state = stream_states[streamIndex];

    // 如果 PTS 无效，尝试从前一帧推断
    if (pkt->pts == AV_NOPTS_VALUE || pkt->pts < 0) {
        if (state.next_expected_pts > 0) {
            pkt->pts = state.next_expected_pts;
            qDebug() << "[Muxer] Compensated missing PTS: used"
                    << state.next_expected_pts;
        } else {
            // 最后的手段：使用 DTS 或设为 0
            pkt->pts = (pkt->dts != AV_NOPTS_VALUE) ? pkt->dts : 0;
        }
    }

    // 如果 DTS 无效，从 PTS 或 frame_duration 推断
    if (pkt->dts == AV_NOPTS_VALUE || pkt->dts < 0) {
        if (pkt->pts != AV_NOPTS_VALUE) {
            pkt->dts = pkt->pts;
        } else if (state.frame_duration > 0) {
            pkt->dts = state.last_mux_dts + state.frame_duration;
        } else {
            pkt->dts = pkt->pts;
        }
        qDebug() << "[Muxer] Compensated missing DTS: used" << pkt->dts;
    }

    // 更新预期下一个 PTS（用于后续补偿）
    if (pkt->duration > 0) {
        state.next_expected_pts = pkt->pts + pkt->duration;
    } else {
        // 无 duration 时使用 frame_duration
        state.next_expected_pts = pkt->pts + std::max(1LL, state.frame_duration);
    }
}

// ===== 3. 改进的 mux 函数 =====
int FFMuxer::mux(AVPacket *packet)
{
    if (trailerFlag.load())
        return 1;

    int streamIndex = packet->stream_index;

    // 检查流有效性
    if (streamIndex != aStreamIndex && streamIndex != vStreamIndex) {
        qWarning() << "[Muxer] Unknown stream index" << streamIndex;
        return -1;
    }

    const char *streamType = (streamIndex == aStreamIndex) ? "Audio" : "Video";

    // 获取源和目标时基
    AVRational srcTimeBase, dstTimeBase;
    {
        std::lock_guard<std::shared_mutex> lock(mutex);
        if (streamIndex == aStreamIndex) {
            srcTimeBase = aCodecCtx->time_base;
            dstTimeBase = aStream->time_base;
        } else {
            srcTimeBase = vCodecCtx->time_base;
            dstTimeBase = vStream->time_base;
        }
    }

    // 第1步：时基转换
    packet->pts = av_rescale_q(packet->pts, srcTimeBase, dstTimeBase);
    packet->dts = av_rescale_q(packet->dts, srcTimeBase, dstTimeBase);
    packet->duration = av_rescale_q(packet->duration, srcTimeBase, dstTimeBase);

    // 第2步：补偿无效时间戳
    compensate_invalid_ts(packet, streamIndex);

    // 第3步：检查是否文件过大
    {
        std::lock_guard<std::shared_mutex> lock(mutex);
        if (fmtCtx && fmtCtx->pb) {
            int64_t current_size = avio_size(fmtCtx->pb);
            if (current_size >= max_filesize) {
                qWarning() << "[Muxer] File size limit reached"
                          << current_size << "bytes";
                return -2;  // 特殊返回值表示大小限制
            }
        }
    }

    // 第4步：输入包信息日志（调试用）
    char pts_buf[AV_TS_MAX_STRING_SIZE];
    char dts_buf[AV_TS_MAX_STRING_SIZE];
    av_ts_make_string(pts_buf, packet->pts);
    av_ts_make_string(dts_buf, packet->dts);

    qDebug().noquote() << "[Mux Input]" << streamType
                       << "pts=" << pts_buf << "dts=" << dts_buf
                       << "size=" << packet->size;

    // 第5步：修正时间戳（DTS 单调性、有效性等）
    mux_fixup_ts(packet, streamIndex);

    // 第6步：累加统计
    if (streamIndex == aStreamIndex) {
        stats.audio_size += packet->size;
        stats.audio_frames++;
    } else {
        stats.video_size += packet->size;
        stats.video_frames++;
    }
    stats.total_packets++;

    // 第7步：写入操作（独占锁）
    {
        std::lock_guard<std::shared_mutex> lock(mutex);
        if (fmtCtx == nullptr) {
            return -1;
        }

        packet->stream_index = streamIndex;

        // 输出包信息（最终状态）
        av_ts_make_string(pts_buf, packet->pts);
        av_ts_make_string(dts_buf, packet->dts);

        qDebug().noquote() << "[Mux Write]" << streamType
                           << "pts=" << pts_buf
                           << "(" << QString::number(av_q2d(dstTimeBase) * packet->pts, 'f', 6) << "s)"
                           << "dts=" << dts_buf
                           << "(" << QString::number(av_q2d(dstTimeBase) * packet->dts, 'f', 6) << "s)"
                           << "size=" << packet->size;

        int ret = av_interleaved_write_frame(fmtCtx, packet);
        if (ret < 0) {
            qWarning() << "[Muxer] Write frame failed";
            printError(ret);
            stats.dropped_packets++;
            return -1;
        }
    }

    return 0;
}

// ===== 4. 初始化流状态 =====
void FFMuxer::addStream(AVCodecContext *codecCtx)
{
    std::lock_guard<std::shared_mutex> lock(mutex);

    if (!codecCtx || !codecCtx->codec) {
        qDebug() << "[Muxer] Invalid codec context";
        return;
    }

    if (codecCtx->time_base.num <= 0 || codecCtx->time_base.den <= 0) {
        qDebug() << "[Muxer] Invalid time base";
        return;
    }

    AVStream *stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        qDebug() << "[Muxer] New Stream Fail";
        return;
    }

    int ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (ret < 0) {
        qDebug() << "[Muxer] Copy Parameters From Context Fail";
        printError(ret);
        return;
    }

    stream->time_base = codecCtx->time_base;

    // ===== 新增：初始化流状态 =====
    MuxStreamState& state = stream_states[stream->index];

    if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        aCodecCtx = codecCtx;
        aStream = stream;
        aStreamIndex = stream->index;

        // 音频帧时长 = sample_rate / 1
        if (codecCtx->sample_rate > 0) {
            state.frame_duration = av_rescale_q(
                codecCtx->frame_size ? codecCtx->frame_size : 1024,
                {1, codecCtx->sample_rate},
                codecCtx->time_base
            );
        }
    } else if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
        vCodecCtx = codecCtx;
        vStream = stream;
        vStreamIndex = stream->index;

        // 视频帧时长 = 1 / fps
        if (codecCtx->framerate.num > 0) {
            state.frame_duration = av_rescale_q(
                1,
                av_inv_q(codecCtx->framerate),
                codecCtx->time_base
            );
        }
    }

    streamCount++;
    if (streamCount == 2) {
        readyFlag = true;
    }
}

// ===== 5. 统计信息输出 =====
void FFMuxer::printStats() const
{
    auto elapsed = std::chrono::steady_clock::now() - stats.start_time;
    auto elapsed_sec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0;

    int64_t total_size = stats.video_size + stats.audio_size;
    double bitrate = elapsed_sec > 0 ? (total_size * 8 / elapsed_sec / 1000) : 0;

    qInfo().noquote()
        << QString("[Mux Stats]\n"
                   "  Video: %1KiB (%2 frames)\n"
                   "  Audio: %3KiB (%4 frames)\n"
                   "  Total: %5KiB (%6 packets, %7 dropped)\n"
                   "  Duration: %8s, Bitrate: %9kbps")
        .arg(stats.video_size / 1024)
        .arg(stats.video_frames)
        .arg(stats.audio_size / 1024)
        .arg(stats.audio_frames)
        .arg(total_size / 1024)
        .arg(stats.total_packets)
        .arg(stats.dropped_packets)
        .arg(elapsed_sec, 0, 'f', 1)
        .arg(bitrate, 0, 'f', 1);
}
```

---

## 二、FFMuxerThread 优化方案

### 改进点：

1. **时间戳补偿**（与 FFMuxer 配合）
2. **错误恢复**
3. **更好的 EOF 处理**

```cpp
// ffmuxerthread.cpp - 关键改进

void FFMuxerThread::run()
{
    fprintf(stderr, "[MuxThread] run() start. Waiting header write...\n");
    muxer->writeHeader();

    // 设置统计开始时间
    auto start_time = std::chrono::steady_clock::now();

    auto dequeue_first = [this](auto *q) -> FFPacket * {
        FFPacket *pkt = nullptr;
        while (!pkt && !m_stop) {
            pkt = q->dequeue();
        }
        return pkt;
    };

    FFPacket *audioPkt = dequeue_first(aPktQueue);
    FFPacket *videoPkt = dequeue_first(vPktQueue);
    if (!audioPkt || !videoPkt) {
        qWarning() << "[MuxThread] Failed to get first packets";
        return;
    }

    AVPacket *aPacket = &audioPkt->packet;
    AVPacket *vPacket = &videoPkt->packet;

    const AVRational aTB = aEncoder->getCodecCtx()->time_base;
    const AVRational vTB = vEncoder->getCodecCtx()->time_base;
    const AVRational vFPS = vEncoder->getCodecCtx()->framerate.num > 0
                                ? vEncoder->getCodecCtx()->framerate
                                : AVRational{30, 1};

    int64_t vFrameDur = av_rescale_q(1, av_inv_q(vFPS), vTB);

    int64_t vPts0 = vPacket->pts;
    while (!m_stop && aPacket &&
           av_compare_ts(aPacket->pts, aTB, vPts0 - vFrameDur, vTB) < 0) {
        FFPacketTraits::release(audioPkt);
        audioPkt = aPktQueue->dequeue();
        if (!audioPkt)
            break;
        aPacket = &audioPkt->packet;
    }
    if (!audioPkt)
        return;

    const AVRational usecTB{1, 1000000};
    AdaptiveSync adaptSync;

    auto ptsToSec = [](int64_t pts, AVRational tb) -> double {
        if (pts == AV_NOPTS_VALUE)
            return NAN;
        return pts * av_q2d(tb);
    };

    fprintf(stderr,
            "[MuxThread] time_base: audio=%d/%d, video=%d/%d\n",
            aTB.num, aTB.den, vTB.num, vTB.den);

    int error_count = 0;
    const int MAX_ERRORS = 10;  // 允许最多 10 个连续错误后退出

    while (!m_stop) {
        const double aSec = ptsToSec(aPacket->pts, aTB);
        const double vSec = ptsToSec(vPacket->pts, vTB);

        double thrSec = (adaptSync.samples() < 10)
            ? SYNC_THRESHOLD
            : adaptSync.calculateDynamicThreshold(aSec, vSec);
        thrSec = std::min(thrSec, 0.030);

        const int64_t thrUsec = (int64_t)llround(thrSec * 1000000.0);
        const int64_t thrAPts = av_rescale_q(thrUsec, usecTB, aTB);
        const int64_t thrVPts = av_rescale_q(thrUsec, usecTB, vTB);

        bool chooseAudio = false;
        if (aPacket->pts == AV_NOPTS_VALUE && vPacket->pts == AV_NOPTS_VALUE) {
            chooseAudio = true;
        } else if (aPacket->pts == AV_NOPTS_VALUE) {
            chooseAudio = false;
        } else if (vPacket->pts == AV_NOPTS_VALUE) {
            chooseAudio = true;
        } else {
            chooseAudio = av_compare_ts(aPacket->pts - thrAPts, aTB, vPacket->pts, vTB) <= 0;
        }

        int ret;
        double processedSec;

        if (chooseAudio) {
            ret = muxer->mux(aPacket);
            processedSec = aSec;
            FFPacketTraits::release(audioPkt);
            audioPkt = aPktQueue->dequeue();
            if (!audioPkt) {
                qDebug() << "[MuxThread] No more audio packets";
                break;
            }
            aPacket = &audioPkt->packet;
        } else {
            ret = muxer->mux(vPacket);
            processedSec = vSec;
            FFPacketTraits::release(videoPkt);
            videoPkt = vPktQueue->dequeue();
            if (!videoPkt) {
                qDebug() << "[MuxThread] No more video packets";
                break;
            }
            vPacket = &videoPkt->packet;
        }

        // ===== 改进：错误处理 =====
        if (ret < 0) {
            error_count++;
            if (ret == -2) {
                // 文件大小限制
                qInfo() << "[MuxThread] File size limit reached, stopping";
                break;
            } else if (error_count >= MAX_ERRORS) {
                qWarning() << "[MuxThread] Too many mux errors"
                          << error_count << ", breaking";
                break;
            }
            qWarning() << "[MuxThread] mux() failed with ret="
                      << ret << "error_count=" << error_count;
        } else {
            error_count = 0;  // 成功时重置错误计数
        }

        // 进度事件（每 10ms）
        if (!std::isnan(processedSec) && processedSec - lastProcessTime >= 0.01) {
            sendCaptureProcessEvent(processedSec);
            lastProcessTime = processedSec;
        }
    }

    // 清理
    if (audioPkt)
        FFPacketTraits::release(audioPkt);
    if (videoPkt)
        FFPacketTraits::release(videoPkt);

    fprintf(stderr, "[MuxThread] loop end, writing trailer...\n");
    muxer->writeTrailer();

    // ===== 新增：输出统计 =====
    muxer->printStats();

    fprintf(stderr, "[MuxThread] done.\n");
}
```

---

## 三、实现清单

- [ ] 在 `FFMuxer::mux_fixup_ts()` 实现 DTS 单调性保证
- [ ] 在 `FFMuxer::compensate_invalid_ts()` 实现时间戳补偿
- [ ] 在 `FFMuxer::addStream()` 初始化 `stream_states`
- [ ] 在 `FFMuxer::mux()` 集成新的修正流程
- [ ] 添加文件大小检测
- [ ] 添加 `MuxStats` 统计收集
- [ ] 在 `FFMuxerThread::run()` 改进错误处理和 EOF 逻辑
- [ ] 测试：验证时间戳单调性（ffprobe 检查 pts/dts）
- [ ] 测试：验证文件可播放性（多个播放器）
- [ ] 测试：验证统计信息准确性
