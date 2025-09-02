#include "eventfactory.h"
#include "eventcategory.h"

std::unique_ptr<FFEvent> ControlEventFactory::createEvent(FFCaptureContext *context,
                                                          const EventParameters &params)
{
    const auto &controlParams = static_cast<const ControlEventParams &>(params);
}
