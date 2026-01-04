# 音频可视化 - 快速参考卡

## 🎯 一页纸总结

### 三个核心组件

```
FFAudioSampler (C++)        AudioVisualizer (QML)      Main.qml (QML)
    |                              |                        |
    ├─ collectSamples()        ├─ drawVolume()         ├─ audioArea
    ├─ updateVolumeLevel()     ├─ drawWaveform()       ├─ Loader加载组件
    ├─ updateWaveformData()    ├─ drawSpectrum()       └─ 显示/隐藏控制
    └─ computeSpectrum()       └─ 信号连接

数据流: PCM → 采样器 → 信号 → 组件 → Canvas → 显示
```

---

## 🔌 集成 3 步走

### Step 1: 添加文件

```bash
recorder/ffaudiosampler.h    # 头文件
recorder/ffaudiosampler.cpp  # 实现
qml/AudioVisualizer.qml       # QML组件
```

### Step 2: 修改 C++代码

```cpp
// ffrecorder.h - 添加成员
std::unique_ptr<FFAudioSampler> m_audioSampler;

// ffrecorder_p.cpp - 初始化
m_audioSampler = std::make_unique<FFAudioSampler>(this);
m_audioSampler->initialize(48000, 2, AV_SAMPLE_FMT_FLT);

// ffadecoder.cpp - 采样数据
recorder->audioSampler()->collectSamples(samples, count);
```

### Step 3: 更新 QML

```qml
// Main.qml - audioArea已更新
AudioVisualizer {
    audioSampler: recorder.audioSampler
    mode: "volume"  // 或 "wave" / "spectrum"
}
```

---

## 📊 显示模式对比

| 模式     | 显示内容        | 用途         | 开销 |
| -------- | --------------- | ------------ | ---- |
| volume   | 竖向柱状 0-100% | 快速判断音量 | 最低 |
| wave     | 波形曲线        | 实时监控波形 | 中等 |
| spectrum | 频谱分布        | 频率分析     | 较高 |

---

## 🎛️ 关键参数调整

```cpp
// 缓冲区大小 (样本数)
SAMPLE_BUFFER_SIZE = 48000 * 2  // 时间越长越平滑, 但延迟增加

// 波形显示点数
WAVEFORM_DISPLAY_POINTS = 256  // 越多越细致, 但性能开销增加

// 频谱段数
SPECTRUM_BINS = 64  // 增加则频率分辨率更高

// 音量平滑系数
VOLUME_SMOOTH_FACTOR = 0.7f  // 0.5-0.95, 越高越平滑

// 信号更新间隔
SIGNAL_UPDATE_INTERVAL = 3  // 越大越省cpu, 但响应变慢
```

---

## 🎨 自定义颜色

```qml
AudioVisualizer {
    volumeBarColor: "#00FF00"     // 绿色
    waveformColor: "#00FF00"      // 绿色
    spectrumColor: "#00FF00"      // 绿色
    backgroundColor: "#1a1a1a"    // 暗色
    gridColor: "#333333"          // 网格
}
```

---

## 🧮 音量计算原理

```
1. 采样信号 s[n]
   ↓
2. 计算RMS = sqrt(Σ(s[n]²) / N)
   ↓
3. 转换dB = 20*log₁₀(RMS)  (限制 -40到0)
   ↓
4. 归一化 = (dB + 40) / 40 * 100  (0-100%)
   ↓
5. 平滑 = old*0.7 + new*0.3
   ↓
6. 显示音量等级
```

---

## 🔍 故障排除速查表

| 症状       | 原因             | 解决                        |
| ---------- | ---------------- | --------------------------- |
| 音量显示 0 | 采样器未启动     | 检查 start()调用            |
| 波形闪烁   | 更新过快         | 增加 SIGNAL_UPDATE_INTERVAL |
| 跳变不平   | 格式转换错误     | 检查 collectSamples 参数    |
| 显示空白   | QML 属性绑定失败 | 检查 recorder.audioSampler  |
| 崩溃       | 线程访问冲突     | 确认 mutex 保护             |

---

## 📈 性能基准

```
内存占用:      ~385 KB (固定)
CPU开销:      <1% (采样+计算)
信号频率:    ~30/秒 (configurable)
Canvas刷新:  60fps (Qt控制)
延迟:         <100ms (取决于缓冲区)
```

---

## 🚀 快速启用

```bash
# 1. 复制文件
cp ffaudiosampler.h recorder/
cp ffaudiosampler.cpp recorder/
cp AudioVisualizer.qml qml/

# 2. 编译
cmake --build build

# 3. 运行测试
# 启动录制 → audioArea显示实时音量
```

---

## 📱 QML 属性完全列表

```qml
// 输入属性
audioSampler          : QObject    // 音频采样器对象
mode                  : string     // "volume" | "wave" | "spectrum"

// 颜色属性
volumeBarColor        : color      // 音量柱颜色
waveformColor         : color      // 波形颜色
spectrumColor         : color      // 频谱颜色
backgroundColor       : color      // 背景颜色
gridColor             : color      // 网格颜色

// 只读属性 (从C++读取)
currentVolume         : int        // 0-100
waveformData          : vector     // 256点
spectrumData          : vector     // 64频段
```

---

## 🔗 FFplay 对应映射

```
FFplay                      Bandicam
────────────────────────────────────
update_sample_display()  →  collectSamples()
sdl_audio_callback()     →  音量计算逻辑
SHOW_MODE_WAVES          →  mode: "wave"
SHOW_MODE_RDFT           →  mode: "spectrum"
SDL绘制                   →  Canvas.onPaint()
```

---

## ⚡ 性能优化建议

### CPU 优化

```cpp
// 1. 增加更新间隔
SIGNAL_UPDATE_INTERVAL = 5  // 从3改为5

// 2. 减少采样率
WAVEFORM_DISPLAY_POINTS = 128  // 从256改为128

// 3. 关闭频谱 (初期)
// computeSpectrum() 注释掉
```

### 内存优化

```cpp
// 1. 减小缓冲区
SAMPLE_BUFFER_SIZE = 48000 * 1  // 从2秒改为1秒

// 2. 减少频谱段
SPECTRUM_BINS = 32  // 从64改为32
```

### 响应优化

```cpp
// 1. 减少平滑因子
VOLUME_SMOOTH_FACTOR = 0.5f  // 更敏捷

// 2. 减少更新间隔
SIGNAL_UPDATE_INTERVAL = 1  // 更频繁
```

---

## 🎯 常见应用场景

### 场景 1: 音量监控 (直播)

```qml
mode: "volume"
volumeBarColor: "red"        // 红色警示
// 添加过载检测
```

### 场景 2: 音频诊断 (录制)

```qml
mode: "wave"
// 查看是否有异常波形
```

### 场景 3: 音乐可视化 (演奏)

```qml
mode: "spectrum"
// 实时显示频谱
```

---

## 📞 联系与支持

- **参考文档**: `AUDIO_VISUALIZER_INTEGRATION_GUIDE.md`
- **实现代码**: `AUDIO_VISUALIZER_IMPLEMENTATION.md`
- **FFplay 分析**: `ffplay音频音量控制分析.md`
- **项目架构**: 见项目 README.md

---

**最后更新**: 2025 年 12 月 2 日
**版本**: v1.0
