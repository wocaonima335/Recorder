# Bandicam vs FFmpeg 官方复用实现对标总结

## 一、核心数据对比表

| 特性                  | FFmpeg 官方      | Bandicam 当前   | 优先级 | 建议行动                 |
| --------------------- | ---------------- | --------------- | ------ | ------------------------ |
| **DTS 单调性保证**    | ✅ 强制          | ❌ 无           | 🔴 高  | 在 mux_fixup_ts() 中实现 |
| **无效 PTS/DTS 处理** | ✅ 补偿          | ❌ 丢弃         | 🔴 高  | 改为补偿而非丢弃         |
| **时基转换**          | ✅ av_rescale_q  | ✅ av_rescale_q | ⚪ 低  | 已实现                   |
| **时间戳跟踪**        | ✅ last_mux_dts  | ❌ 无           | 🔴 高  | 添加 stream_states map   |
| **同步队列**          | ✅ 全局排序      | ✅ 自适应阈值   | ⚪ 低  | 当前方案更轻量（保留）   |
| **BSF 链支持**        | ✅ 完整          | ❌ 无           | 🟡 中  | Phase 3 实现             |
| **流复制优化**        | ✅ of_streamcopy | ❌ 全转码       | ⚪ 低  | 当前需求不强             |
| **文件大小限制**      | ✅ 检测          | ❌ 无           | 🟡 中  | 在 mux() 中添加          |
| **统计信息**          | ✅ 详细          | ❌ 无           | 🟡 中  | 添加 MuxStats 结构       |
| **错误恢复**          | ✅ 健壮          | ❌ 基础         | 🟡 中  | 改进错误处理逻辑         |

---

## 二、关键差异分析

### 差异 1：时间戳修正的层次

**FFmpeg**（三层防护）：

```c
// 层1：DTS > PTS 无效对修正
if (pkt->dts > pkt->pts) { /* 中位数修正 */ }

// 层2：DTS 单调性强制
if (pkt->dts < ms->last_mux_dts) { pkt->dts = ms->last_mux_dts; }

// 层3：音频样本级补偿
pkt->dts = av_rescale_delta(...);
```

**Bandicam**（一层检查）：

```cpp
if (pts == AV_NOPTS_VALUE) { return 0; }  // 直接丢弃
```

**影响**：

- FFmpeg 方式更健壮，容器格式不会报警
- Bandicam 方式可能导致进度条跳变或播放异常

**修复建议**：采用 FFmpeg 的三层防护策略

---

### 差异 2：无效时间戳处理

**FFmpeg**（积极补偿）：

```c
// 从前一帧或样本数推断
if (pkt->pts == AV_NOPTS_VALUE) {
    pkt->pts = ms->next_expected_pts;
}
// 确保 DTS >= PTS
pkt->dts = std::max(pkt->dts, pkt->pts);
```

**Bandicam**（直接丢弃）：

```cpp
if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE) {
    return 0;  // 丢弃包
}
```

**影响**：

- 某些编码器输出 `AV_NOPTS_VALUE` 是正常的
- 丢弃会导致音视频长度不一致或卡顿
- 应该补偿而非丢弃

**修复建议**：实现 `compensate_invalid_ts()` 函数

---

### 差异 3：同步机制的选择

**FFmpeg**（全局排序队列）：

```c
// 多流通过全局同步队列排序
sq_send(mux->sq_mux, stream_idx, pkt);
while (sq_receive(...) >= 0) { write_packet(...); }
```

- 优点：精确同步，容易扩展到多文件
- 缺点：复杂，多一层队列，延迟可能增加

**Bandicam**（自适应阈值同步）：

```cpp
// 动态计算阈值，哪个先达到就写哪个
bool chooseAudio = av_compare_ts(aPacket->pts - thrAPts, aTB,
                                  vPacket->pts, vTB) <= 0;
```

- 优点：简单，低延迟，适合实时录屏
- 缺点：不是全局最优顺序，但在 `av_interleaved_write_frame` 会自动排序

**结论**：Bandicam 的方案 **更适合实时录屏场景**，无需改进

---

### 差异 4：错误处理策略

| 场景         | FFmpeg           | Bandicam   |
| ------------ | ---------------- | ---------- |
| 文件大小超限 | 返回 AVERROR_EOF | 无检测     |
| 多次写入失败 | 记录错误继续     | 无重试逻辑 |
| 无效时间戳   | 补偿             | 丢弃       |
| 流未就绪     | 等待 + 超时      | 简单阻塞   |

**建议改进**：

1. 添加文件大小检测 → 触发文件轮转
2. 添加错误计数 → 连续错误后中止
3. 改善 EOF 处理 → 确保所有包写入

---

## 三、分优先级实现路线

### 🔴 Phase 1（关键，防止容器错误）

#### 1.1 实现 `mux_fixup_ts()`

```cpp
void FFMuxer::mux_fixup_ts(AVPacket *pkt, int streamIndex)
{
    auto& state = stream_states[streamIndex];

    // 第1步：DTS > PTS 修正
    if (pkt->dts > pkt->pts) {
        int64_t median = /* 计算中位数 */;
        pkt->pts = pkt->dts = median;
    }

    // 第2步：DTS 单调性（关键）
    if (pkt->dts < state.last_mux_dts) {
        pkt->dts = state.last_mux_dts + 1;
    }
    state.last_mux_dts = pkt->dts;
}
```

**预期收益**：

- ✅ 消除容器格式写入警告
- ✅ 支持更多播放器
- ⏱️ 实现时间：2 小时

---

#### 1.2 改进时间戳补偿

```cpp
void FFMuxer::compensate_invalid_ts(AVPacket *pkt, int streamIndex)
{
    auto& state = stream_states[streamIndex];

    if (pkt->pts == AV_NOPTS_VALUE) {
        pkt->pts = state.next_expected_pts;
    }
    if (pkt->dts == AV_NOPTS_VALUE) {
        pkt->dts = pkt->pts;  // 音频通常 dts == pts
    }
    state.next_expected_pts = pkt->pts + pkt->duration;
}
```

**预期收益**：

- ✅ 防止进度条跳变
- ✅ 减少 "dropped packets" 现象
- ⏱️ 实现时间：1 小时

---

### 🟡 Phase 2（稳定性，提升可靠性）

#### 2.1 文件大小限制检测

```cpp
// 在 mux() 中
if (avio_size(fmtCtx->pb) >= max_filesize) {
    qWarning() << "File size limit, stopping";
    return -2;
}
```

- ⏱️ 实现时间：30 分钟
- 📊 优先级：高（生产环境必需）

---

#### 2.2 统计信息收集

```cpp
struct MuxStats {
    int64_t video_size, audio_size;
    int64_t video_frames, audio_frames;
};
```

- ⏱️ 实现时间：1 小时
- 📊 优先级：中（调试和监控）

---

#### 2.3 改进错误恢复

```cpp
// 在 FFMuxerThread::run() 中
int error_count = 0;
const int MAX_ERRORS = 10;

if (ret < 0) {
    error_count++;
    if (error_count >= MAX_ERRORS) break;
}
```

- ⏱️ 实现时间：30 分钟
- 📊 优先级：中（防止无限循环）

---

### ⚪ Phase 3（完善性，与官方对齐）

#### 3.1 BSF 链支持（可选）

- 当前编码器输出已格式化，暂无需求
- 保留接口，后续 passthrough 编码时实现
- ⏱️ 实现时间：3 小时

---

#### 3.2 高级同步机制（可选）

- 当前自适应阈值已足够
- 若需多文件同步再考虑全局队列
- ⏱️ 实现时间：4 小时

---

## 四、测试计划

### 验证 DTS 单调性

```bash
# 使用 ffprobe 检查时间戳
ffprobe -v error -select_streams v:0 -show_frames \
  -print_format json output.mp4 | \
  jq '.frames[] | .pkt_dts' | sort -c  # 应无报错
```

### 验证文件可播放

```bash
# 多个播放器测试
ffplay output.mp4              # FFplay
vlc output.mp4                 # VLC
"C:\Program Files\MPC-HC\mpc-hc64.exe" output.mp4  # MPC-HC
```

### 验证统计准确性

```bash
# 使用 ffmpeg 对比
ffmpeg -i output.mp4 -f null -  2>&1 | grep -E "video|audio"
```

---

## 五、集成清单

- [ ] 添加 `stream_states` 成员变量
- [ ] 实现 `mux_fixup_ts()` 方法
- [ ] 实现 `compensate_invalid_ts()` 方法
- [ ] 修改 `mux()` 集成新流程
- [ ] 添加 `MuxStats` 结构和收集逻辑
- [ ] 改进 `FFMuxerThread::run()` 错误处理
- [ ] 编写单元测试（时间戳单调性、大小限制等）
- [ ] 完整回归测试（各分辨率、帧率、音频采样率）
- [ ] 性能基准测试（CPU/内存开销）
- [ ] 更新代码注释和文档

---

## 六、预期收益总结

| 改进项       | 风险 | 收益               | 优先级 |
| ------------ | ---- | ------------------ | ------ |
| DTS 单调性   | 低   | 高（避免容器错误） | 🔴     |
| 时间戳补偿   | 低   | 中（改善稳定性）   | 🔴     |
| 文件大小限制 | 低   | 中（防止磁盘满）   | 🟡     |
| 统计信息     | 低   | 中（调试/监控）    | 🟡     |
| BSF 链       | 中   | 低（当前无需求）   | ⚪     |
| 全局同步     | 中   | 低（自适应已足够） | ⚪     |

**总体改进方向**：从 "能用" → "生产级可靠"

---

## 七、参考资源

- FFmpeg 源码：`fftools/ffmpeg_mux.c` (lines 195-600)
- 时间戳处理：`mux_fixup_ts()` 和 `mux_packet_filter()`
- 同步队列：`sync_queue` 机制
- Bandicam 实现：`ffmuxer.cpp` 和 `ffmuxerthread.cpp`
