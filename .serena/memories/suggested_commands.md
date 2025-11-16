# 常用命令
- **列出文件/目录**：在 PowerShell 中使用 `Get-ChildItem`（或别名 `ls`）。
- **查看文件**：`Get-Content path\to\file`（别名 `cat`）。
- **配置构建**：`cmake -S . -B build\windows -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DQt6_DIR=C:\Qt\6.5.3\msvc2019_64\lib\cmake\Qt6`。
- **编译**：`cmake --build build\windows --target appbandicam --config RelWithDebInfo`。
- **运行**：`build\windows\appbandicam.exe`（确保 FFmpeg DLL 已随 POST_BUILD 复制或手动拷贝）。
- **清理输出**：删除 `build\windows` 目录或使用 `cmake --build build\windows --target clean`。
- **调试 QML**：使用 Qt Creator 打开 `CMakeLists.txt`，选择 Qt 6 kit 后直接运行。