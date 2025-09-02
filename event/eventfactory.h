#ifndef EVENTFACTORY_H
#define EVENTFACTORY_H

#include "abstracteventfactory.h"
#include "eventcategory.h"

#include <memory>

class FFStartEvent;
class FFStopEvent;
class FFPauseEvent;
class FFReadyEvent;
class FFEndEvent;

class ControlEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFCaptureContext *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::CONTROL; }
};

class SourceEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFCaptureContext *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::SOURCE; }
};

class ParameterEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFCaptureContext *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::PARAMETER; }
};

class ProcessEventFactory : public AbstractEventFactory
{
public:
    std::unique_ptr<FFEvent> createEvent(FFCaptureContext *context,
                                         const EventParameters &params) override;
    EventCategory getCategory() const override { return EventCategory::PROCESS; }
};

#endif // EVENTFACTORY_H
