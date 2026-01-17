#include "ffsourcechangeevent.h"

#include "recorder/ffrecorder.h"

std::atomic<bool> FFSourceChangeEvent::sourceChanged{true};

FFSourceChangeEvent::FFSourceChangeEvent(FFRecorder *recorderCtx, bool useScreen_)
    : FFEvent(recorderCtx)
    , useScreen(useScreen_)
{}

void FFSourceChangeEvent::work()
{
    bool current = sourceChanged.load(std::memory_order_acquire);
    if (current == useScreen) {
        return;
    }

    // 录制中：禁止切换，直接返回
    if (recoderContext && recoderContext->isRecording()) {
        return;
    }

    // 未录制时：仅更新源标志，录制开始时根据此标志选择源
    sourceChanged.store(useScreen, std::memory_order_release);
}
