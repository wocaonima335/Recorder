cmake_minimum_required(VERSION 3.16)

project(bandicam VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS Quick Qml Multimedia Widgets Gui)

qt_standard_project_setup(REQUIRES 6.5)

# FFmpeg库配置
set(FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg-amf")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")



# 添加FFmpeg头文件目录
include_directories(${FFMPEG_INCLUDE_DIR})

# 查找FFmpeg库文件
find_library(AVCODEC_LIB avcodec HINTS ${FFMPEG_LIB_DIR})
find_library(AVFORMAT_LIB avformat HINTS ${FFMPEG_LIB_DIR})
find_library(AVUTIL_LIB avutil HINTS ${FFMPEG_LIB_DIR})
find_library(SWSCALE_LIB swscale HINTS ${FFMPEG_LIB_DIR})
find_library(SWRESAMPLE_LIB swresample HINTS ${FFMPEG_LIB_DIR})
find_library(AVFILTER_LIB avfilter HINTS ${FFMPEG_LIB_DIR})
find_library(AVDEVICE_LIB avdevice HINTS ${FFMPEG_LIB_DIR})

# 创建FFmpeg库目标
set(FFMPEG_LIBRARIES
    ${AVCODEC_LIB}
    ${AVFORMAT_LIB}
    ${AVUTIL_LIB}
    ${SWSCALE_LIB}
    ${SWRESAMPLE_LIB}
    ${AVFILTER_LIB}
    ${AVDEVICE_LIB}
)

# 添加子目录

add_subdirectory(demuxer)
add_subdirectory(qml)
add_subdirectory(queue)
add_subdirectory(decoder)
add_subdirectory(resampler)
add_subdirectory(filter)
add_subdirectory(muxer)
add_subdirectory(event)
add_subdirectory(thread)
add_subdirectory(recorder)
add_subdirectory(encoder)
add_subdirectory(opengl)

# 主可执行文件
qt_add_executable(appbandicam
    main.cpp
    res.qrc
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appbandicam PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appbandicam
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appbandicam PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})


# 链接库（包括子模块）
target_link_libraries(appbandicam
    PRIVATE 
        Qt6::Qml
        Qt6::Quick
        qml_module
        opengl_module
        demuxer_module
        queue_module
        decoder_module
        resampler_module
        filter_module
        muxer_module
        event_module
        thread_module
        recorder_module
        encoder_module
)

# 复制FFmpeg DLL文件到输出目录
set(FFMPEG_DLL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg-amf/bin")
file(GLOB FFMPEG_DLLS "${FFMPEG_DLL_DIR}/*.dll")

# 在构建后复制DLL文件
add_custom_command(TARGET appbandicam POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${FFMPEG_DLLS}
    $<TARGET_FILE_DIR:appbandicam>
    COMMENT "Copying FFmpeg DLLs to output directory"
)

include(GNUInstallDirs)
install(TARGETS appbandicam
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 安装FFmpeg DLL文件
install(FILES ${FFMPEG_DLLS}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)


