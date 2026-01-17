#include "eventfactory.h"
#include "eventcategory.h"

#include "event/ffclosesourceevent.h"
#include "event/ffopensourceevent.h"
#include "event/ffpauseevent.h"
#include "event/ffprocessevent.h"
#include "event/ffaudiosourcechangeevent.h"
#include "event/ffsourcechangeevent.h"

std::unique_ptr<FFEvent> SourceEventFactory::createEvent(FFRecorder *context,
                                                         const EventParameters &params)
{
    const auto &sourceParams = static_cast<const SourceEventParams &>(params);

    switch (sourceParams.type)
    {
    case SourceEventType::OPEN_SOURCE:
        // OPEN_SOURCE事件现在直接在main.cpp中创建，不再通过工厂
        // 因为需要同时打开视频和音频源，避免并行执行导致的资源竞态
        throw std::invalid_argument("OPEN_SOURCE event should be created directly, not through factory");
    case SourceEventType::CLOSE_SOURCE:
        return std::make_unique<FFCloseSourceEvent>(context,
                                                    sourceParams.sourceType);
    default:
        throw std::invalid_argument("Unknown source event type");
    }
}

std::unique_ptr<FFEvent> ProcessEventFactory::createEvent(FFRecorder *context,
                                                          const EventParameters &params)
{
    const auto &processParams = static_cast<const ProcessEventParams &>(params);

    switch (processParams.type)
    {
    case ProcessEventType::CAPTURE_PROCESS:
        return std::make_unique<FFCaptureProcessEvent>(context, processParams.curSec);
    default:
        throw std::invalid_argument("Unknown process event type");
    }
}

std::unique_ptr<FFEvent> ControlEventFactory::createEvent(FFRecorder *context,
                                                          const EventParameters &params)
{
    const auto &ctrl = static_cast<const ControlEventParams &>(params);
    switch (ctrl.type)
    {
    case ControlEventType::PAUSE:
        return std::make_unique<FFPauseEvent>(context, true, ctrl.ts_us);
    case ControlEventType::READY:
         return std::make_unique<FFPauseEvent>(context, false, ctrl.ts_us);
    case ControlEventType::SOURCECHANGE:
        return std::make_unique<FFSourceChangeEvent>(context, ctrl.useScreen);
    case ControlEventType::AUDIO_SOURCECHANGE:
        return std::make_unique<FFAudioSourceChangeEvent>(context, ctrl.useSystemAudio);
    default:
        throw std::invalid_argument("Unknown control event type");
    }
}
