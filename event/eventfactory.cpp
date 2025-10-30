#include "eventfactory.h"
#include "eventcategory.h"

#include "event/ffclosesourceevent.h"
#include "event/ffopensourceevent.h"
#include "event/ffpauseevent.h"
#include "event/ffprocessevent.h"

std::unique_ptr<FFEvent> SourceEventFactory::createEvent(FFRecorder *context,
                                                         const EventParameters &params)
{
    const auto &sourceParams = static_cast<const SourceEventParams &>(params);

    switch (sourceParams.type) {
    case SourceEventType::OPEN_SOURCE:
        return std::make_unique<FFOpenSourceEvent>(context,
                                                   sourceParams.sourceType,
                                                   sourceParams.url,
                                                   sourceParams.format);
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

    switch (processParams.type) {
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
    switch (ctrl.type) {
    case ControlEventType::PAUSE:
        return std::make_unique<FFPauseEvent>(context, true, ctrl.ts_us);
    case ControlEventType::READY:
        return std::make_unique<FFPauseEvent>(context, false, ctrl.ts_us);
    default:
        throw std::invalid_argument("Unknown control event type");
    }
}
