#ifndef EVENTFACTORY_H
#define EVENTFACTORY_H

#include "abstracteventfactory.h"
#include "eventcategory.h"

#include "event/ffopensourceevent.h"
#include "event/ffclosesourceevent.h"

#include <memory>

class FFStartEvent;
class FFStopEvent;
class FFPauseEvent;
class FFReadyEvent;
class FFEndEvent;

class SourceEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFRecorder *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::SOURCE; }
};

#endif // EVENTFACTORY_H
