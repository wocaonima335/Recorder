#ifndef FFAUDIOSOURCECHANGEEVENT_H
#define FFAUDIOSOURCECHANGEEVENT_H

#include "ffevent.h"

#include <atomic>

class FFAudioSourceChangeEvent : public FFEvent
{
public:
    FFAudioSourceChangeEvent(FFRecorder *recorderCtx, bool useSystemAudio_);
    void work() override;
    static std::atomic<bool> useSystemAudio; // true: 系统音频, false: 麦克风

private:
    bool m_useSystemAudio;
};

#endif // FFAUDIOSOURCECHANGEEVENT_H
