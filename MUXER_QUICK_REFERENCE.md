# Bandicam 复用优化快速参考卡

## 📌 一页总结

### 问题诊断

```
当前实现              问题                    FFmpeg 方案
─────────────────────────────────────────────────────────────
无 DTS 单调性   → 容器警告、播放异常     ← mux_fixup_ts()
直接丢弃无效TS  → 进度条跳变、帧丢失     ← compensate_invalid_ts()
无流状态跟踪    → 无法修正错误           ← stream_states map
无大小限制      → 磁盘可能溢出           ← avio_size() 检测
无统计信息      → 调试困难               ← MuxStats 结构
```

### 优化路线

```
🔴 Phase 1 (3-4h)          🟡 Phase 2 (1-2h)        ⚪ Phase 3 (5-7h)
├─ DTS 单调性保证           ├─ 文件大小限制           ├─ BSF 链支持
├─ 时间戳补偿               ├─ 统计信息收集           └─ 全局同步队列
└─ 流状态管理               └─ 错误恢复

效果：能用 → 生产级        生产级 → 完善               对标 FFmpeg
风险：低                    低                         中
```

---

## 🔧 核心代码框架

### 1. DTS 单调性保证

```cpp
// 在 ffmuxer.cpp 中
void FFMuxer::mux_fixup_ts(AVPacket *pkt, int streamIndex)
{
    auto& state = stream_states[streamIndex];

    // 第1层：DTS > PTS 修正
    if (pkt->dts > pkt->pts) {
        // 计算中位数
        int64_t median = /* ... */;
        pkt->pts = pkt->dts = median;
    }

    // 第2层：DTS 单调性（关键）
    if (state.last_mux_dts >= 0 && pkt->dts < state.last_mux_dts) {
        pkt->dts = state.last_mux_dts + 1;
        if (pkt->pts < pkt->dts) pkt->pts = pkt->dts;
    }
    state.last_mux_dts = pkt->dts;
}
```

### 2. 时间戳补偿

```cpp
void FFMuxer::compensate_invalid_ts(AVPacket *pkt, int streamIndex)
{
    auto& state = stream_states[streamIndex];

    if (pkt->pts == AV_NOPTS_VALUE) {
        pkt->pts = state.next_expected_pts;
    }
    if (pkt->dts == AV_NOPTS_VALUE) {
        pkt->dts = pkt->pts;
    }
    state.next_expected_pts = pkt->pts + pkt->duration;
}
```

### 3. 流状态初始化

```cpp
// 在 ffmuxer.h 中
struct MuxStreamState {
    int64_t last_mux_dts = -1;
    int64_t last_mux_pts = -1;
    int64_t next_expected_pts = 0;
};

std::map<int, MuxStreamState> stream_states;

// 在 addStream() 中
auto& state = stream_states[stream->index];
if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
    state.frame_duration = av_rescale_q(
        codecCtx->frame_size ? codecCtx->frame_size : 1024,
        {1, codecCtx->sample_rate},
        codecCtx->time_base
    );
}
```

### 4. 改进的 mux() 流程

```cpp
int FFMuxer::mux(AVPacket *packet)
{
    // 1. 时基转换
    packet->pts = av_rescale_q(packet->pts, srcTB, dstTB);
    packet->dts = av_rescale_q(packet->dts, srcTB, dstTB);

    // 2. 补偿无效时间戳
    compensate_invalid_ts(packet, streamIndex);

    // 3. 文件大小检测（Phase 2）
    if (avio_size(fmtCtx->pb) >= max_filesize) {
        return -2;
    }

    // 4. 修正时间戳
    mux_fixup_ts(packet, streamIndex);

    // 5. 累加统计（Phase 2）
    stats.total_packets++;

    // 6. 写入
    return av_interleaved_write_frame(fmtCtx, packet);
}
```

---

## ✅ 实现检查清单

### Phase 1（必做）

- [ ] 添加 `MuxStreamState` 结构到 `ffmuxer.h`
- [ ] 添加 `stream_states` map 到 `ffmuxer.h`
- [ ] 实现 `mux_fixup_ts()` 方法
- [ ] 实现 `compensate_invalid_ts()` 方法
- [ ] 修改 `mux()` 集成上述方法
- [ ] 修改 `addStream()` 初始化流状态
- [ ] 编译测试（确保无编译错误）
- [ ] 功能测试（各分辨率、码率）
- [ ] 播放器兼容性测试（VLC、MPC-HC、FFplay）

### Phase 2（推荐）

- [ ] 添加 `MuxStats` 结构到 `ffmuxer.h`
- [ ] 实现统计收集逻辑（在 `mux()` 中）
- [ ] 实现 `printStats()` 输出
- [ ] 添加 `max_filesize` 成员和检测逻辑
- [ ] 改进 `FFMuxerThread::run()` 错误处理
- [ ] 测试大文件写入（>2GB）
- [ ] 验证统计准确性

### Phase 3（可选）

- [ ] 调研 BSF 需求
- [ ] 设计 BSF 架构
- [ ] 实现 BSF 支持

---

## 🧪 快速验证

### 验证 DTS 单调性

```bash
ffprobe -v error -select_streams v:0 -show_frames \
  -print_format json output.mp4 | \
  jq '.frames[] | .pkt_dts' | sort -c && echo "✓ PASS" || echo "✗ FAIL"
```

### 验证时间戳有效性

```bash
ffprobe -v error -show_entries frame=pkt_pts,pkt_dts \
  -print_format csv=p=0 output.mp4 | head -20
# 应无 NOPTS_VALUE 或负数
```

### 播放器测试

```bash
ffplay output.mp4 &       # FFplay
vlc output.mp4 &          # VLC
```

---

## 📊 性能预期

| 操作                    | 时间复杂度 | CPU 开销 | 内存开销     |
| ----------------------- | ---------- | -------- | ------------ |
| mux_fixup_ts()          | O(1)       | < 0.05%  | 0            |
| compensate_invalid_ts() | O(1)       | < 0.05%  | 0            |
| stream_states 查询      | O(log n)\* | < 0.05%  | ~100 字节/流 |
| 总额外开销              | -          | < 0.2%   | ~200 字节    |

\*n = 流数（通常 2）

---

## 💡 常见问题快答

| 问题                     | 答案                         |
| ------------------------ | ---------------------------- |
| 会影响性能吗？           | 否，< 0.2% CPU 开销          |
| 需要改 encoder 吗？      | 否，独立改进 muxer 即可      |
| 向后兼容吗？             | 是，修正逻辑 if 条件自动跳过 |
| 多久见效？               | Phase 1 完成后立即生效       |
| 能否只做 Phase 1？       | 可以，单独就很有价值         |
| 需要改 decoder 吗？      | 否                           |
| 现有文件需要重新处理吗？ | 否，只影响新录制             |

---

## 🎯 优先级决策树

```
问题：文件不能在某播放器播放？
├─ VLC 能播放？ → 不是 DTS 问题，检查编码器
└─ VLC 也不能？ → 可能是 DTS 或时间戳问题
   ├─ ffprobe 显示 DTS 递减？ → 做 Phase 1
   ├─ 进度条跳跃？ → 做 Phase 1 + Phase 2 中的补偿
   └─ 长录制文件损坏？ → 做 Phase 2 中的大小限制

问题：想要更好的可观测性？
└─ 做 Phase 2 中的统计信息收集

问题：需要 passthrough 编码？
└─ Phase 3 中的 BSF 支持
```

---

## 📈 预期改进

```
实施前              →  实施后
─────────────────────────────────
播放器兼容 95%      →  100% ✓
进度条准确 不稳定   →  稳定 ✓
容器警告 可能存在   →  0 ✓
可观测性 无         →  完整 ✓
CPU 开销 baseline   →  +0.2% ✓
```

---

## 🚀 推荐实施步骤

1. **Day 1-2**：实现 Phase 1（DTS 修正）

   - 编码、编译、基本测试
   - 预计 4-6 小时

2. **Day 3-4**：测试和调试

   - 多播放器兼容性测试
   - 极限场景测试（高分辨率、长录制）
   - 预计 6-8 小时

3. **Day 5**：实现 Phase 2（可选）

   - 统计 + 大小限制 + 错误恢复
   - 预计 3-4 小时

4. **后续**：监控反馈，决策 Phase 3

---

## 📚 深度阅读

详见以下文档：

- `MUXER_OPTIMIZATION_ANALYSIS.md` - 详细分析
- `MUXER_COMPARISON_SUMMARY.md` - 差异对标
- `MUXER_IMPLEMENTATION_GUIDE.md` - 完整实现方案

---

**Version**: 1.0 | **Date**: 2025-11-30 | **Status**: Ready for Implementation
