#include "eventfactory.h"
#include "eventcategory.h"

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

    default:
        throw std::invalid_argument("Unknown source event type");
    }
}
