#include "ffaudiosourcechangeevent.h"

#include "recorder/ffrecorder.h"

std::atomic<bool> FFAudioSourceChangeEvent::useSystemAudio{true};

FFAudioSourceChangeEvent::FFAudioSourceChangeEvent(FFRecorder *recorderCtx, bool useSystemAudio_)
    : FFEvent(recorderCtx)
    , m_useSystemAudio(useSystemAudio_)
{}

void FFAudioSourceChangeEvent::work()
{
    bool current = useSystemAudio.load(std::memory_order_acquire);
    if (current == m_useSystemAudio) {
        return;
    }

    if (recoderContext && recoderContext->isRecording()) {
        return;
    }

    useSystemAudio.store(m_useSystemAudio, std::memory_order_release);
}
