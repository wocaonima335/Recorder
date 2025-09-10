qt_add_library(recorder_module STATIC
    ffrecorder.h
    ffrecorder_p.h
    ffrecoder_p.cpp
    ffrecoder.cpp

)
target_link_libraries(recorder_module PRIVATE
    PRIVATE 
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        ${FFMPEG_LIBRARIES}
        queue_module
        timer_module
        muxer_module
        demuxer_module
        encoder_module
        decoder_module
        filter_module
)

target_include_directories(recorder_module PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # 添加项目根目录，使得可以找到queue/ffapacketqueue.h
)
