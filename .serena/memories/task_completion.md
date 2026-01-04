# 任务完成后的检查
1. 在 PowerShell 中运行 `cmake --build build\windows --target appbandicam`（或对应构建目录）确保编译通过。
2. 启动 `build\windows\appbandicam.exe`，手动验证 UI 能启动、录制控制信号能触发（特别是 start/pause/stop）。
3. 如改动涉及 FFmpeg/线程模块，观察控制台日志，确认没有长时间阻塞或异常断流。
4. 若新增依赖或资源，更新 `CMakeLists.txt` 及相关子模块，确保 POST_BUILD 复制 DLL 的逻辑仍有效。
5. 根据需要更新文档/QML 资源，保持中文注释同步。