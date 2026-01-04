# FFAudioSampler 集成代码示例

## 📌 核心集成点说明

本文档提供具体的代码片段，说明如何将 FFAudioSampler 集成到 Bandicam 现有的架构中。

---

## 1️⃣ FFRecorder 中的集成

### ffrecorder.h 修改

```cpp
#ifndef FFRECORDER_H
#define FFRECORDER_H

#include "ffaudiosampler.h"
#include <memory>

class FFRecorder : public QObject
{
    Q_OBJECT

public:
    // ... 现有代码 ...

    /**
     * @brief 获取音频采样器实例
     * @return FFAudioSampler指针，供QML调用
     */
    FFAudioSampler* audioSampler() const {
        return m_audioSampler.get();
    }

    // ... 其他公有接口 ...

protected:
    // 在initialize()中初始化采样器
    void initializeAudioSampler();

    // 在cleanup()中清理采样器
    void cleanupAudioSampler();

private:
    // 音频采样器实例，用于音频可视化
    std::unique_ptr<FFAudioSampler> m_audioSampler;

    // ... 其他私有成员 ...
};

#endif // FFRECORDER_H
```

### ffrecorder_p.cpp 修改

在 FFRecorder 的 initialize()方法中添加：

```cpp
void FFRecorder::initialize()
{
    // ... 现有初始化代码 ...

    // 初始化音频采样器
    m_audioSampler = std::make_unique<FFAudioSampler>(this);

    // 假设音频采样率为48kHz, 2声道
    m_audioSampler->initialize(48000, 2, AV_SAMPLE_FMT_FLT);

    // 不在这里启动，在开始录制时启动
    // m_audioSampler->start();

    qDebug() << "Audio sampler initialized";
}
```

在开始录制时（onRecordingStart 事件）：

```cpp
void FFRecorder::onRecordingStart()
{
    // ... 现有代码 ...

    // 启动音频采样
    if (m_audioSampler) {
        m_audioSampler->clear();
        m_audioSampler->start();
        qDebug() << "Audio sampling started";
    }
}
```

在停止录制时（onRecordingStop 事件）：

```cpp
void FFRecorder::onRecordingStop()
{
    // ... 现有代码 ...

    // 停止音频采样
    if (m_audioSampler) {
        m_audioSampler->stop();
        m_audioSampler->clear();
        qDebug() << "Audio sampling stopped";
    }
}
```

---

## 2️⃣ 音频解码器中的集成

### ffadecoder.h 无需修改

保持现有接口不变。

### ffadecoder.cpp 修改

在 processFrame()或处理解码输出的地方添加采样收集：

```cpp
#include "ffrecorder.h"

// 在处理解码后的音频帧的地方
void FFAudioDecoder::processDecodedFrame(AVFrame *frame)
{
    if (!frame || !frame->nb_samples) {
        return;
    }

    // 获取全局FFRecorder实例
    FFRecorder *recorder = FFRecorder::getInstance();
    FFAudioSampler *sampler = recorder ? recorder->audioSampler() : nullptr;

    if (!sampler || !sampler->isActive()) {
        return;  // 采样器未启动，跳过
    }

    // 根据音频格式，调用相应的collectSamples方法
    switch (frame->format) {
        case AV_SAMPLE_FMT_FLT: {
            // 单通道或交错float格式
            float *samples = (float *)frame->data[0];
            int sampleCount = frame->nb_samples * frame->ch_layout.nb_channels;
            sampler->collectSamples(samples, sampleCount);
            break;
        }

        case AV_SAMPLE_FMT_FLTP: {
            // Planar float格式（多个通道分开存储）
            // 对于多通道情况，需要交错处理
            int numChannels = frame->ch_layout.nb_channels;
            int numSamples = frame->nb_samples;
            float **samples = (float **)frame->data;

            // 选项1: 只采集第一个声道（单声道分析）
            if (samples[0]) {
                sampler->collectSamples(samples[0], numSamples);
            }

            // 选项2: 混合所有声道（立体声分析）
            // 这需要在FFAudioSampler中添加专门的方法
            // std::vector<float*> channelData(samples, samples + numChannels);
            // sampler->collectSamplesPlanar(channelData, numSamples);
            break;
        }

        case AV_SAMPLE_FMT_S16: {
            // 16位有符号整数格式
            int16_t *samples = (int16_t *)frame->data[0];
            int sampleCount = frame->nb_samples * frame->ch_layout.nb_channels;
            sampler->collectSamples(samples, sampleCount);
            break;
        }

        case AV_SAMPLE_FMT_S16P: {
            // Planar 16位格式
            int numChannels = frame->ch_layout.nb_channels;
            int numSamples = frame->nb_samples;
            int16_t **samples = (int16_t **)frame->data;

            if (samples[0]) {
                sampler->collectSamples(samples[0], numSamples);
            }
            break;
        }

        default:
            qWarning() << "Unsupported audio format for sampling:" << frame->format;
            break;
    }
}
```

---

## 3️⃣ QML 集成

### Main.qml 已更新

audioArea 现在包含 AudioVisualizer 组件。无需再改动。

### 可选: 创建一个简化的设置页面

新建 `qml/AudioSettingsPage.qml`:

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: "#2C2F3C"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        Text {
            text: "音频显示设置"
            font.pixelSize: 16
            font.bold: true
            color: "#FFFFFF"
        }

        Rectangle {
            height: 1
            Layout.fillWidth: true
            color: "#40454F"
        }

        // 显示模式选择
        RowLayout {
            Text {
                text: "显示模式:"
                color: "#FFFFFF"
                font.pixelSize: 12
            }

            ComboBox {
                id: modeSelector
                model: ["音量指示", "波形图", "频谱图"]
                currentIndex: 0

                onCurrentIndexChanged: {
                    switch(currentIndex) {
                        case 0:
                            audioVisualizer.mode = "volume"
                            break
                        case 1:
                            audioVisualizer.mode = "wave"
                            break
                        case 2:
                            audioVisualizer.mode = "spectrum"
                            break
                    }
                }
            }
        }

        // 音量平滑度
        RowLayout {
            Text {
                text: "平滑度:"
                color: "#FFFFFF"
                font.pixelSize: 12
            }

            Slider {
                from: 0.5
                to: 0.95
                value: 0.7
                onMoved: {
                    // 该参数需要在FFAudioSampler中暴露为属性
                    // recorder.audioSampler.volumeSmoothFactor = value
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
```

---

## 4️⃣ CMakeLists.txt 修改

### 主 CMakeLists.txt

确保已在顶级文件中正确配置

### recorder/CMakeLists.txt

```cmake
add_library(recorder STATIC
    ffrecorder.cpp
    ffrecorder_p.cpp
    ffaudiosampler.cpp          # 添加此行
    # ... 其他源文件 ...
)

target_include_directories(recorder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # ... 其他包含目录 ...
)

target_link_libraries(recorder
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    # ... 其他库 ...
)
```

---

## 5️⃣ 编译验证步骤

### 构建命令

```powershell
cd d:\Qtprogram\bandicam
cmake -B build -S . -G "Ninja" -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2019_64"
cmake --build build --config Release 2>&1 | Tee-Object build.log
```

### 常见编译错误及解决

#### 错误 1: "ffaudiosampler.h: No such file"

**原因**: 包含路径不正确
**解决**:

```cpp
// 使用相对路径
#include "../recorder/ffaudiosampler.h"
// 或在decoder/CMakeLists.txt中添加
target_include_directories(decoder PRIVATE ${CMAKE_SOURCE_DIR}/recorder)
```

#### 错误 2: "undefined reference to FFAudioSampler::collectSamples"

**原因**: 链接库遗漏
**解决**: 确保 ffaudiosampler.cpp 已添加到 CMakeLists.txt

#### 错误 3: "std::mutex not found"

**原因**: C++标准版本过低
**解决**: 在 CMakeLists.txt 中设置

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

---

## 6️⃣ 运行时集成验证

### 添加调试日志

在 ffaudiosampler.cpp 中已包含 qDebug()输出，运行时应看到：

```
FFAudioSampler initialized: sampleRate= 48000 channels= 2 bufferSize= 96000
FFAudioSampler started
Audio sampler initialized
```

### 在 Main.qml 中验证

```qml
Component.onCompleted: {
    console.log("recorder object:", recorder)
    console.log("audioSampler:", recorder.audioSampler)
    if (recorder.audioSampler) {
        console.log("volumeLevel:", recorder.audioSampler.volumeLevel)
    }
}
```

### 检查信号是否正确传递

```qml
Connections {
    target: recorder.audioSampler

    onVolumeLevelChanged: {
        console.log("Volume changed:", newLevel)
    }

    onWaveformDataChanged: {
        console.log("Waveform updated")
    }
}
```

---

## 7️⃣ 性能监控

### 添加性能计时

在 ffaudiosampler.cpp 中添加（可选）：

```cpp
#include <chrono>

void FFAudioSampler::collectSamples(const float *samples, int sampleCount)
{
    auto start = std::chrono::high_resolution_clock::now();

    // ... 采样处理 ...

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 每1000次采集打印一次性能数据
    static int count = 0;
    if (++count >= 1000) {
        qDebug() << "Sample collection time:" << duration.count() << "μs";
        count = 0;
    }
}
```

### 内存监控

FFAudioSampler 的内存占用：

```
缓冲区: 96000 samples × 4 bytes (float) = 384 KB
波形数据: 256 ints × 4 bytes = 1 KB
频谱数据: 64 floats × 4 bytes = 256 bytes
总计: ~385 KB (固定)
```

---

## 8️⃣ 常见问题

### Q1: 如何切换显示模式？

A: 在 Main.qml 中修改 AudioVisualizer 的 mode 属性：

```qml
AudioVisualizer {
    audioSampler: recorder.audioSampler
    mode: "wave"  // 改为 "spectrum" 或 "volume"
}
```

### Q2: 如何改变音量显示的颜色？

A: 在 Main.qml 中自定义颜色属性：

```qml
AudioVisualizer {
    audioSampler: recorder.audioSampler
    volumeBarColor: "#FF6600"      // 橙色
    waveformColor: "#00FFFF"       // 青色
    spectrumColor: "#FF00FF"       // 洋红色
}
```

### Q3: 如何禁用音频采样来节省性能？

A: 在 ffaudiosampler.cpp 中注释掉 collectSamples 的调用，或添加条件：

```cpp
if (some_performance_flag) {
    sampler->collectSamples(samples, sampleCount);
}
```

---

## ✅ 集成完成检查清单

- [ ] 创建 `ffaudiosampler.h` 和 `ffaudiosampler.cpp`
- [ ] 修改 `ffrecorder.h` 添加采样器成员
- [ ] 修改 `ffrecorder_p.cpp` 初始化/清理采样器
- [ ] 修改 `ffadecoder.cpp` 调用 collectSamples()
- [ ] 创建 `AudioVisualizer.qml` 组件
- [ ] 修改 `Main.qml` audioArea
- [ ] 更新 `recorder/CMakeLists.txt`
- [ ] 编译验证，检查日志输出
- [ ] 运行测试，验证 UI 显示
- [ ] 性能监控，优化参数

---

**版本**: v1.0
**最后更新**: 2025 年 12 月 2 日
