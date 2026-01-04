# Bandicam 复用实现对标 FFmpeg 官方的优化分析

## 一、架构层面对比

### FFmpeg 官方复用 (`ffmpeg_mux.c`)

```
调度器驱动（sch_mux_receive）
    ↓
muxer_thread()（专用复用线程）
    ↓
mux_packet_filter()（统一处理流）
    ├─ 流复制（of_streamcopy）
    ├─ BSF 过滤器链
    ├─ 字幕心跳修复
    └─ 同步队列（sync_queue）
    ↓
write_packet()（时间戳修正 + 写入）
    ├─ mux_fixup_ts()（关键：DTS 单调性保证）
    └─ av_interleaved_write_frame()
```

### Bandicam 当前实现

```
音视频分别从队列取包
    ↓
FFMuxerThread::run()
    ├─ 自适应同步算法（AdaptiveSync）
    └─ 交替处理音频/视频
    ↓
FFMuxer::mux()
    ├─ 时基转换
    ├─ 基本有效性检查（pts/dts/duration）
    └─ av_interleaved_write_frame()
```

**关键差异**：

- ✅ Bandicam 有自适应同步（创新点）
- ❌ 缺少 DTS 单调性强制保证（可能导致容器写入警告）
- ❌ 缺少 BSF（比特流过滤器）链支持
- ❌ 没有流复制优化（虽然当前全转码）

---

## 二、时间戳处理对比

### FFmpeg 的 `mux_fixup_ts()` 三层校验

```c
// 第1层：DTS > PTS 修正（无效时间戳对）
if (pkt->dts > pkt->pts) {
    // 三值中位数算法
    pkt->pts = pkt->dts = ...中位数...
}

// 第2层：DTS 单调性保证（最关键）
int64_t max = ms->last_mux_dts + !(oformat->flags & AVFMT_TS_NONSTRICT);
if (pkt->dts < max) {
    if (pkt->pts >= pkt->dts)
        pkt->pts = FFMAX(pkt->pts, max);
    pkt->dts = max;  // 强制提升 DTS
}
ms->last_mux_dts = pkt->dts;

// 第3层：音频特殊处理（样本级精度）
if (ost->type == AVMEDIA_TYPE_AUDIO && !ost->enc) {
    pkt->dts = av_rescale_delta(...);  // 使用样本数精确计算
    pkt->pts = pkt->dts;
}
```

### Bandicam 当前实现

```c
// 仅基本有效性检查
if (packet->pts == AV_NOPTS_VALUE ||
    packet->dts == AV_NOPTS_VALUE ||
    packet->pts < 0) {
    // 直接丢弃 - 过于激进！
    return 0;
}
```

**问题**：

1. **时间戳无效时直接丢弃帧**（可能导致卡顿或进度条跳跃）
2. **没有 DTS 单调性强制**（某些容器可能输出异常）
3. **没有跟踪 last_mux_dts**（无法检测回退）

---

## 三、同步机制对比

### FFmpeg 的同步队列（`sync_queue`）

多个输出流通过全局队列实现**时间精确同步**：

```c
// 发送到同步队列
sq_send(mux->sq_mux, ms->sq_idx_mux, pkt);

// 从队列中按序出取（全局排序）
while (1) {
    ret = sq_receive(mux->sq_mux, -1, pkt);  // -1=任意流
    // 接收按全局 PTS 排序的包
    write_packet(mux, get_stream_by_ret, pkt);
}
```

**特点**：

- 全局 PTS 排序，不同流间精确同步
- 自动处理流速不匹配（快流等待慢流）

### Bandicam 的自适应同步

```c
// 动态阈值同步
double thrSec = adaptSync.calculateDynamicThreshold(aSec, vSec);
bool chooseAudio = av_compare_ts(aPacket->pts - thrAPts, aTB,
                                  vPacket->pts, vTB) <= 0;
```

**特点**：

- 基于实时误差动态调整阈值
- 不需要全局排序队列
- **更轻量、低延迟**

**对比结论**：

- FFmpeg：精确但复杂（需要全局队列）
- Bandicam：实用且高效（适合实时录屏）

---

## 四、数据包生命周期对比

### FFmpeg 的流程

```
sch_mux_receive()
    ↓ (从调度器获取)
av_bsf_send_packet()  ← [可选] BSF 链处理
    ↓ (每个 BSF 可产生多个包)
av_bsf_receive_packet()
    ↓ (循环处理所有输出包)
sync_queue_process()  ← [可选] 同步队列
    ↓
write_packet()
    ├─ mux_fixup_ts()
    └─ av_interleaved_write_frame()
```

### Bandicam 的流程

```
FFAPacketQueue::dequeue()  /  FFVPacketQueue::dequeue()
    ↓ (分别取音视频)
muxer->mux(packet)
    ├─ 时基转换
    ├─ 有效性检查
    └─ av_interleaved_write_frame()
```

**关键差异**：

1. **BSF 链缺失**（对 H.264 需要处理 SPS/PPS，当前隐性依赖编码器输出正确）
2. **时间戳修正不足**（可能在容器格式中暴露）
3. **流包顺序依赖交错写入**（依赖 `av_interleaved_write_frame` 自动排序）

---

## 五、问题与改进建议

### 问题 1：时间戳回退导致写入失败

**现象**：在某些编码器配置下，DTS 可能出现回退

```
[Mux Write] Audio pts=1000 dts=1000
[Mux Write] Video pts=950 dts=950  ← DTS 回退！
```

**FFmpeg 的解决**：

```c
// 跟踪最后一个 mux 的 DTS，确保单调
if (pkt->dts < ms->last_mux_dts) {
    pkt->dts = ms->last_mux_dts;
}
ms->last_mux_dts = pkt->dts;
```

**建议实现**（参考方案）：

```cpp
// 在 FFMuxer 中添加
struct MuxStreamState {
    int64_t last_mux_dts = -1;
    int64_t last_mux_pts = -1;
};
std::map<int, MuxStreamState> stream_states;  // 按流索引

// 在 mux() 中修正
int FFMuxer::mux(AVPacket *packet) {
    // ... 时基转换后 ...

    int streamIdx = packet->stream_index;
    auto& state = stream_states[streamIdx];

    // DTS > PTS 修正
    if (packet->dts > packet->pts) {
        packet->pts = packet->dts =
            std::min(packet->pts, packet->dts);
    }

    // DTS 单调性保证
    if (packet->dts < state.last_mux_dts) {
        packet->dts = state.last_mux_dts;
        if (packet->pts < packet->dts)
            packet->pts = packet->dts;
    }
    state.last_mux_dts = packet->dts;

    // ... 继续写入 ...
}
```

---

### 问题 2：无效时间戳处理过于激进

**现象**：某些编码器输出 `AV_NOPTS_VALUE`，当前直接丢弃

```cpp
if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE) {
    return 0;  // 丢弃 ← 这会导致进度条跳变！
}
```

**FFmpeg 的处理**：

```c
// 音频样本级补偿
if (packet->pts == AV_NOPTS_VALUE) {
    packet->pts = av_rescale_q(sample_count, {1, sample_rate}, tb);
}

// 从前一帧推算
if (packet->dts < 0) {
    packet->dts = std::max(packet->dts, state.last_dts + frame_duration);
}
```

**建议实现**：

```cpp
// 添加到 FFMuxerThread::run()
struct StreamState {
    int64_t next_expected_pts = 0;
    int64_t frame_duration = 0;
};

// 处理音频
if (aPacket->pts == AV_NOPTS_VALUE) {
    aPacket->pts = aState.next_expected_pts;
}
aState.next_expected_pts = aPacket->pts + aPacket->duration;

// 处理视频
if (vPacket->dts == AV_NOPTS_VALUE) {
    vPacket->dts = std::max(
        vState.next_expected_dts,
        vState.frame_duration  // 从 fps 计算
    );
}
```

---

### 问题 3：缺少 BSF 链支持

**现象**：直接依赖编码器输出正确的包（H.264 需要 `h264_mp4toannexb` 或 `h264_nal` 等）

**FFmpeg 的处理**：

```c
// 为需要的流添加 BSF
const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
av_bsf_alloc(bsf, &ctx);
av_bsf_init(ctx);

// 处理包时
av_bsf_send_packet(ctx, pkt);
while (av_bsf_receive_packet(ctx, out_pkt) >= 0) {
    // out_pkt 是转换后的包
}
```

**建议实现**（轻量级）：

```cpp
// 在 FFMuxer 中添加
bool FFMuxer::needsBSF(AVCodecContext* codecCtx) {
    // H.264 MP4 输出可能需要转换
    return codecCtx->codec_id == AV_CODEC_ID_H264 &&
           format == "mp4";
}

// 在 addStream 中
if (needsBSF(codecCtx)) {
    const AVBitStreamFilter *bsf =
        av_bsf_get_by_name("h264_mp4toannexb");
    av_bsf_alloc(bsf, &bsf_ctx);
    av_bsf_init(bsf_ctx);
}

// 在 mux 中应用
if (bsf_ctx) {
    av_bsf_send_packet(bsf_ctx, packet);
    while (av_bsf_receive_packet(bsf_ctx, out_pkt) >= 0) {
        av_interleaved_write_frame(fmtCtx, out_pkt);
        av_packet_unref(out_pkt);
    }
} else {
    av_interleaved_write_frame(fmtCtx, packet);
}
```

---

### 问题 4：文件大小限制检测缺失

**FFmpeg 的处理**：

```c
// 在 write_packet 前检查
int64_t fs = avio_size(s->pb);  // 获取当前文件大小
if (fs >= mux->limit_filesize) {
    return AVERROR_EOF;  // 触发文件关闭
}
```

**建议实现**：

```cpp
// 添加到 FFMuxer
int64_t max_filesize = 4LL * 1024 * 1024 * 1024;  // 4GB

int FFMuxer::mux(AVPacket *packet) {
    // 检查文件大小
    int64_t current_size = avio_size(fmtCtx->pb);
    if (current_size >= max_filesize) {
        // 触发文件关闭，或者启动新文件
        qWarning() << "File size limit reached" << current_size;
        return -1;  // 发送 EOF 信号
    }
    // ... 继续 ...
}
```

---

### 问题 5：统计信息与监控缺失

**FFmpeg 输出**：

```
[mux] video:1234KiB audio:567KiB subtitle:0KiB other:0KiB
[mux] global headers:10KiB muxing overhead: 1.5%
[latency] muxer <- demux: 5ms, encode: 12ms, filter: 3ms
```

**建议实现**：

```cpp
// 在 FFMuxer 中添加统计结构
struct MuxStats {
    int64_t video_size = 0;
    int64_t audio_size = 0;
    int64_t video_frames = 0;
    int64_t audio_frames = 0;
    int64_t start_time = 0;
};

// 在 mux() 中累加
if (streamIndex == vStreamIndex) {
    stats.video_size += packet->size;
    stats.video_frames++;
} else if (streamIndex == aStreamIndex) {
    stats.audio_size += packet->size;
    stats.audio_frames++;
}

// 在 writeTrailer 后输出
void FFMuxer::printStats() {
    auto elapsed = av_gettime_relative() - stats.start_time;
    qInfo() << QString("[Mux Stats] video:%1KiB audio:%2KiB "
                       "frames(v:%3 a:%4) duration:%5s bitrate:%6kbps")
        .arg(stats.video_size / 1024)
        .arg(stats.audio_size / 1024)
        .arg(stats.video_frames)
        .arg(stats.audio_frames)
        .arg(elapsed / 1000000.0)
        .arg((stats.video_size + stats.audio_size) * 8 /
             (elapsed / 1000000.0) / 1000);
}
```

---

## 六、分阶段优化路线

### Phase 1（高优先级，防止容器错误）

- ✅ **实现 DTS 单调性保证**（防止播放器错误）
- ✅ **改进无效时间戳处理**（补偿而非丢弃）
- ✅ **添加 last_mux_dts 跟踪**

**预期效果**：消除容器格式警告，支持更多播放器

### Phase 2（中优先级，提升稳定性）

- ✅ **文件大小限制检测**
- ✅ **统计信息输出**
- ✅ **错误恢复机制**

**预期效果**：生产环境就绪

### Phase 3（低优先级，功能补全）

- ✅ **轻量级 BSF 链支持**（H.264 mp4toannexb）
- ✅ **流复制优化**（若增加 passthrough 编码）
- ✅ **高级同步队列**（如需多文件同步）

**预期效果**：与 FFmpeg 官方特性对齐

---

## 七、参考实现代码框架

见下一部分的具体代码改进建议。
