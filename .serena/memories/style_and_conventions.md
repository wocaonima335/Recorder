# 代码/风格约定
- **语言**：全仓 C++17；UI 使用 QML。注释多为中文，QML 代码也保持中文命名/注释。
- **命名**：FFmpeg 相关核心以 `FF` 前缀（如 `FFVEncoderThread`、`FFEventQueue`）；Qt/FFmpeg 类型混用，保持驼峰命名。
- **模块化**：每个子目录提供一个 `qt_add_library(..._module STATIC)`，在 `appbandicam` 中统一链接。
- **线程与队列**：通过自定义 `FFBoundedQueue`、`FFThreadPool` 以及事件机制串联；编码/滤镜等线程依赖各自的 `*_FrameQueue`。
- **资源管理**：FFmpeg 对象普遍使用裸指针，函数末尾显式 `av_frame_unref/av_frame_free`；Qt 对象通过 `QObject` 生命周期管理。
- **UI**：QML 中约定使用 `GLView`（绑定 `FFGLItem`），控制信号以 `startRecording/stopRecording` 等形式从 UI 传回 C++。