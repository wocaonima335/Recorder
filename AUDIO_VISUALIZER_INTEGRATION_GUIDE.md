# 音频可视化完整实现指南

## 📋 概述

本文档说明如何将基于 FFplay 音频显示机制的实时音频可视化功能集成到 Bandicam 应用中。该实现包括：

1. **C++后端音频采样器** (`FFAudioSampler`)
2. **QML 前端可视化组件** (`AudioVisualizer.qml`)
3. **Main.qml 中的集成**

---

## 🏗️ 架构设计

### 数据流

```
录制音频帧 (PCM数据)
    ↓
FFAudioSampler::collectSamples()  [C++线程安全采样]
    ↓
环形缓冲区 (SAMPLE_BUFFER_SIZE = 96000样本)
    ↓
分析计算:
  - updateVolumeLevel()    → 计算RMS音量 (0-100)
  - updateWaveformData()   → 采样波形数据 (256点)
  - computeSpectrum()      → 计算频谱 (64频谱段)
    ↓
Qt信号发送:
  - volumeLevelChanged()
  - waveformDataChanged()
  - spectrumDataChanged()
    ↓
QML Canvas绘制
    ↓
UI实时显示 (60fps更新)
```

### 设计特性

| 特性             | 说明                               |
| ---------------- | ---------------------------------- |
| **线程安全**     | 使用`std::mutex`保护共享数据       |
| **环形缓冲**     | 固定大小缓冲区，避免内存膨胀       |
| **对数刻度**     | 参考 ffplay 使用分贝(dB)单位       |
| **音量平滑**     | VOLUME_SMOOTH_FACTOR=0.7，防止抖动 |
| **三种显示模式** | volume/wave/spectrum               |
| **低开销**       | 信号每 3 次更新才发送一次          |

---

## 📝 集成步骤

### 第 1 步: 集成 C++采样器

#### 1.1 在 CMakeLists.txt 添加源文件

编辑 `recorder/CMakeLists.txt`:

```cmake
add_library(recorder STATIC
    ffrecorder.cpp
    ffrecorder_p.cpp
    ffaudiosampler.cpp      # 添加此行
)
```

#### 1.2 在 FFRecorder 中添加采样器实例

编辑 `recorder/ffrecorder.h`:

```cpp
#include "ffaudiosampler.h"

class FFRecorder : public QObject
{
    Q_OBJECT

    // ... 其他代码 ...

public:
    // 获取音频采样器
    FFAudioSampler* audioSampler() const { return m_audioSampler.get(); }

private:
    std::unique_ptr<FFAudioSampler> m_audioSampler;
};
```

编辑 `recorder/ffrecorder_p.cpp` (构造函数):

```cpp
FFRecorder::FFRecorder(QObject *parent)
    : QObject(parent)
    , m_audioSampler(std::make_unique<FFAudioSampler>(this))
{
    // 初始化其他成员...
}
```

#### 1.3 在音频解码器中调用 collectSamples()

编辑 `decoder/ffadecoder.cpp`:

```cpp
// 在processAudioFrame()或相似的解码输出函数中

// 获取全局FFRecorder实例
FFRecorder *recorder = FFRecorder::getInstance();
if (recorder && recorder->audioSampler()) {
    // 假设frame是解码后的AVFrame

    if (frame->format == AV_SAMPLE_FMT_FLT) {
        // Float格式
        float *samples = (float *)frame->data[0];
        int sampleCount = frame->nb_samples;
        recorder->audioSampler()->collectSamples(samples, sampleCount);
    }
    else if (frame->format == AV_SAMPLE_FMT_S16) {
        // Int16格式
        int16_t *samples = (int16_t *)frame->data[0];
        int sampleCount = frame->nb_samples;
        recorder->audioSampler()->collectSamples(samples, sampleCount);
    }
}
```

#### 1.4 启动/停止采样

在开始录制时：

```cpp
// 在FFEventLoop::onOpenSourceEvent()或类似地方
void FFRecorder::startCapture() {
    // ... 其他初始化 ...

    // 初始化采样器
    FFAudioSampler *sampler = m_audioSampler.get();
    sampler->initialize(48000, 2, AV_SAMPLE_FMT_FLT);  // 根据实际参数调整
    sampler->start();
}

void FFRecorder::stopCapture() {
    // ... 清理代码 ...

    if (m_audioSampler) {
        m_audioSampler->stop();
        m_audioSampler->clear();
    }
}
```

### 第 2 步: 在 QML 中集成 AudioVisualizer

#### 2.1 在 Main.qml 中导入组件

在 Main.qml 顶部添加组件注册（或使用 Loader 动态加载）：

```qml
// 已在audioArea中使用Loader动态加载
// 如需提前编译，需在CMakeLists.txt的qml资源中注册
```

#### 2.2 Main.qml 中的 audioArea 已更新

audioArea 现在包含：

- `AudioVisualizer` 组件（录制时显示）
- 备用图标（非录制时显示）

### 第 3 步: 在 QML Bridge 中暴露接口

编辑 `qml/main.cpp` (QmlBridge 部分):

```cpp
#include "ffaudiosampler.h"

class QmlBridge : public QObject
{
    Q_OBJECT

    // ... 其他接口 ...

    Q_PROPERTY(QObject* audioSampler READ getAudioSampler NOTIFY audioSamplerChanged)

public slots:
    QObject* getAudioSampler() {
        return FFRecorder::getInstance()->audioSampler();
    }

signals:
    void audioSamplerChanged();
};
```

在 QML 中访问：

```qml
AudioVisualizer {
    audioSampler: recorder.audioSampler  // 从C++暴露的接口
    mode: "volume"
}
```

---

## 🎨 自定义显示模式

### AudioVisualizer 属性

```qml
AudioVisualizer {
    // 输入属性
    audioSampler: recorder.audioSampler
    mode: "volume"  // "volume" | "wave" | "spectrum"

    // 颜色自定义
    volumeBarColor: "#00FF00"
    waveformColor: "#00FF00"
    spectrumColor: "#00FF00"
    backgroundColor: "#1a1a1a"
    gridColor: "#333333"
}
```

### 三种显示模式详解

#### 模式 1: "volume" - 音量指示器

- **显示内容**：竖向柱状图，显示 0-100%的音量
- **颜色含义**：
  - <30%: 绿色 (#00FF00)
  - 30-70%: 黄色 (#FFFF00)
  - > 70%: 红色 (#FF0000)
- **用途**：快速判断录制音量是否合适

```
┌─────────────────┐
│                 │ ← 空闲
│                 │
├─────────────────┤ ← 50% 音量线
│ ██████████████  │
│ ██████████████  │
└─────────────────┘
```

#### 模式 2: "wave" - 波形显示

- **显示内容**：256 点采样的波形曲线
- **原理**：参考 ffplay 的 SHOW_MODE_WAVES
- **用途**：实时监控音频波形，判断是否有故障

```
        /\      /\
       /  \    /  \
──────/────\──/────\──── 中心线
     /      \/
```

#### 模式 3: "spectrum" - 频谱显示

- **显示内容**：64 个频谱段的能量分布
- **原理**：简化的频率分析（可扩展为 FFT）
- **用途**：直观显示音频的频率特征

```
██  ████  ██  ██  ████  ██
█████████████████████████
低频                    高频
```

---

## 🔧 性能优化

### 缓冲区大小调整

根据需求调整 `FFAudioSampler` 中的常量：

```cpp
// 增加缓冲时间（更平滑但延迟更高）
static constexpr int SAMPLE_BUFFER_SIZE = 48000 * 3;  // 3秒

// 减少波形显示点数（性能更好）
static constexpr int WAVEFORM_DISPLAY_POINTS = 128;

// 调整频谱段数
static constexpr int SPECTRUM_BINS = 32;
```

### 信号更新频率

```cpp
// 减少信号发送频率（降低UI更新开销）
static constexpr int SIGNAL_UPDATE_INTERVAL = 5;  // 每5次采样更新一次

// 增加平滑因子（更稳定但响应更慢）
static constexpr float VOLUME_SMOOTH_FACTOR = 0.85f;
```

### Canvas 渲染优化

```qml
Canvas {
    renderStrategy: Canvas.Cooperative  // 后台线程渲染
    // 或
    renderStrategy: Canvas.Threaded     // 多线程渲染
}
```

---

## 📊 FFplay 对应关系

| Bandicam 组件          | FFplay 对应                          | 用途                 |
| ---------------------- | ------------------------------------ | -------------------- |
| FFAudioSampler         | update_sample_display()              | 采样音频数据到缓冲区 |
| updateVolumeLevel()    | sdl_audio_callback() + 音量计算      | 计算 RMS 音量        |
| updateWaveformData()   | video_audio_display(SHOW_MODE_WAVES) | 波形数据提取         |
| computeSpectrum()      | video_audio_display(SHOW_MODE_RDFT)  | 频谱数据计算         |
| AudioVisualizer Canvas | SDL 绘制函数                         | 前端 Canvas 绘制     |

---

## 🐛 故障排除

### 问题 1: 音量显示为 0 不变

**原因**：采样器未收到音频数据或未启动
**解决**：

1. 检查`collectSamples()`是否被调用
2. 验证`audioSampler->start()`已执行
3. 查看日志输出 `"FFAudioSampler started"`

### 问题 2: 波形显示闪烁

**原因**：缓冲区更新过于频繁或采样率不匹配
**解决**：

1. 增加 `SIGNAL_UPDATE_INTERVAL` 值
2. 核实采样率配置 `initialize(48000, ...)`
3. 增加 `VOLUME_SMOOTH_FACTOR`

### 问题 3: 音量显示跳变

**原因**：采样器初始化参数错误或音频格式转换出错
**解决**：

1. 确保 `initialize()` 参数与实际音频匹配
2. 检查 `collectSamples()` 中的格式转换逻辑
3. 验证 RMS 计算中的归一化系数

### 问题 4: 显示模式不切换

**原因**：QML 属性绑定失败
**解决**：

```qml
// 确保recorder对象已正确暴露
console.log("audioSampler:", recorder.audioSampler)
console.log("volumeLevel:", recorder.audioSampler.volumeLevel)
```

---

## 🚀 未来扩展

### Phase 1: 增强型音量计量

```cpp
// 支持LUFS (Loudness Units relative to Full Scale) 标准
// 用于广播级别的音量计量
class LUFSMeter : public FFAudioSampler {
    // 实现ITU-R BS.1770标准
};
```

### Phase 2: 实时频谱分析

```cpp
// 集成FFmpeg avfft模块进行真正的FFT
// 支持64-4096点FFT分辨率
// 实时计算频谱中心频率、带宽等
```

### Phase 3: 音频特效链

```qml
// 在audioArea中集成EQ、压缩器等实时显示
EqualizerVisualizer { /* ... */ }
CompressorMeter { /* ... */ }
LimiterIndicator { /* ... */ }
```

---

## 📚 参考文献

1. **FFplay 源码分析** - 见 `ffplay音频音量控制分析.md`
2. **SDL_MixAudioFormat** - SDL 官方文档
3. **FFmpeg PCM 处理** - FFmpeg API 参考
4. **Qt Canvas 渲染** - Qt Quick Controls 文档

---

## 📝 实现检查清单

- [ ] 添加 `ffaudiosampler.h` 和 `ffaudiosampler.cpp`
- [ ] 在 `recorder/CMakeLists.txt` 中注册源文件
- [ ] 在 `FFRecorder` 中添加采样器成员
- [ ] 在音频解码器中调用 `collectSamples()`
- [ ] 在启动/停止录制时调用 `start()/stop()`
- [ ] 创建 `AudioVisualizer.qml` 组件
- [ ] 更新 `Main.qml` 的 `audioArea`
- [ ] 在 QML Bridge 中暴露采样器接口
- [ ] 编译验证，运行测试
- [ ] 调整显示模式和性能参数

---

**最后更新**: 2025 年 12 月 2 日
