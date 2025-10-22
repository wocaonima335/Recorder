#ifndef EVENTFACTORY_H
#define EVENTFACTORY_H

#include "abstracteventfactory.h"
#include "eventcategory.h"

#include "event/ffopensourceevent.h"
#include "event/ffclosesourceevent.h"

#include <memory>

class SourceEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFRecorder *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::SOURCE; }
};

class ProcessEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFRecorder *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::PROCESS; }
};

#endif // EVENTFACTORY_H
