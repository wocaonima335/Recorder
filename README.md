# XRecord

FusionRec 是一款基于 Qt6 Quick 与 FFmpeg 的跨平台录屏工具，整合屏幕、摄像头、系统音频与麦克风采集能力，采用事件驱动和线程池调度在解复用、解码、滤镜、编码、复用各阶段保持流畅的音视频管线，并通过 QML 界面与 OpenGL 预览提供可视化控制与实时反馈。

## 核心特性

- **多源同步**：同时管理屏幕、摄像头、系统声卡、麦克风等输入，UI 中可一键切换视频源。
- **事件驱动流水线**：自定义事件队列与线程池（FFEventQueue / FFEventLoop / FFThreadPool）串联打开、关闭、控制、进度事件，保证各模块解耦。
- **暂停与时间轴修正**：暂停/恢复事件会同步到音视频编码线程并扣除停顿时长，避免 PTS 跳变。
- **实时预览**：FFVEncoderThread 将 YUV420 帧上传到 FFGLItem，FFGLRenderer 使用 GLSL Shader 渲染，实现低延迟预览。
- **进度反馈**：复用线程定期派发 FFCaptureProcessEvent 更新 QML 的计时文本，界面状态与后台同步。

## 技术栈

- **GUI / 交互**：Qt 6.5+ Quick、QML、QtQuick Controls、Material 风格组件。
- **媒体处理**：FFmpeg（avcodec、avformat、avutil、avdevice、swscale、swresample、avfilter）。
- **渲染**：QQuickFramebufferObject、OpenGL ES、GLSL Shader。
- **并发**：C++17、std::thread、std::mutex、自定义线程池、FFBoundedQueue。

## 架构概览

```
QML交互 -> QmlBridge -> EventFactoryManager -> FFEventQueue
    -> (SOURCE事件) Demuxer & Decoder
    -> (PROCESS事件) Filter & Encoder
    -> FFMuxerThread -> MP4 输出 + 进度事件
    -> FFVEncoderThread -> OpenGL 预览
```

核心对象 FFRecorder 作为上下文，统一持有解复用、解码、滤镜、编码、复用以及线程、队列资源。事件工厂根据类别生成 FFOpenSourceEvent、FFCloseSourceEvent、FFPauseEvent、FFCaptureProcessEvent、FFSourceChangeEvent 等，业务层与底层音视频管线保持解耦。

## 目录速览

- `qml/`：主界面 `Main.qml`、设置列表、功能页面、`GLView` 预览容器。
- `recorder/`：`FFRecorder` 单例与上下文，初始化解复用、解码、滤镜、编码、复用、线程、队列。
- `event/`：事件定义、工厂、队列以及控制/源/进度事件实现。
- `demuxer` / `decoder` / `filter` / `encoder` / `muxer`：FFmpeg 能力的分层封装。
- `thread/`：各模块线程实现（解复用、解码、滤镜、编码、复用、线程池等）。
- `queue/`：音视频包/帧队列与通用有界阻塞队列。
- `opengl/`：`FFGLItem` 与 `FFGLRenderer`，负责 QML 预览。
- `3rdparty/ffmpeg-amf/`：预编译 FFmpeg 头文件、库与 DLL。

## 构建与运行

1. **准备依赖**
   - 安装 Qt 6.5 及以上版本（包含 Quick、QML、Multimedia、Widgets、Gui 模块）。
   - 准备 FFmpeg Windows 预编译包，放置于 `3rdparty/ffmpeg-amf`（需包含 include、lib、bin）。
   - 准备 MSVC 或 MinGW C++17 编译器，并安装 CMake ≥ 3.16。
2. **配置工程**
   ```powershell
   cd d:\Qtprogram\bandicam
   cmake -B build -S . -G "Ninja" -DCMAKE_PREFIX_PATH="Qt/6.5/msvc2019_64"
   ```
3. **编译与复制 DLL**
   ```powershell
   cmake --build build --config Release
   ```
   构建完成后，CMake 会自动将 `3rdparty/ffmpeg-amf/bin/*.dll` 复制到输出目录。
4. **运行**
   ```powershell
   cd build
   .\appbandicam.exe
   ```
   首次运行请确保已安装屏幕采集（screen-capture-recorder）与虚拟音频设备。

## 使用说明

1. 启动应用后，可在顶部切换屏幕/摄像头输入，触发 `openScreen` / `openCamera` 事件。
2. 红色圆形按钮用于开始/结束录制，右侧按钮可暂停/恢复。
3. 左侧菜单包含“主页”“设置”“视频”三类页面，可切换不同功能面板。
4. 录制过程中，界面上方计时文本显示实时进度，确认录制状态。

## 开发者提示

- `FFRecorder::initialize()` 负责创建全部解码/编码/滤镜/线程等资源，新增能力应在此注册。
- 引入新事件时需更新 `EventCategory`、`AbstractEventFactory` 及 `EventFactoryManager`。
- OpenGL 预览使用三纹理上传 YUV 数据，扩展特效可在 `shaderSource` 中增加 GLSL。
- 若需支持更多采集协议，可在 `Demuxer::init` 中扩展 URL/format 解析。

## 规划与 TODO
- [ ] 处理二次录制时导致的音视频不同步问题。
- [ ] 启用视频和音频滤镜模块。

## 参与贡献

1. Fork 仓库并创建 `feat/xxx` 分支。
2. 保持 C++17 代码风格与中文注释，提交前 clang-format。
3. PR 中附上构建与功能验证说明，便于快速 Review。
