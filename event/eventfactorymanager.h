#ifndef EVENTFACTORYMANAGER_H
#define EVENTFACTORYMANAGER_H

#include "abstracteventfactory.h"
#include "eventcategory.h"
#include "eventfactory.h"

#include <unordered_map>

class EventFactoryManager
{
public:
    static EventFactoryManager &getInstance()
    {
        static EventFactoryManager instance;
        return instance;
    }

    void registerFactory(EventCategory cateGory, std::unique_ptr<AbstractEventFactory> factory);
    std::unique_ptr<FFEvent> createEvent(EventCategory category,
                                         FFCaptureContext *context,
                                         const EventParameters &params);

private:
    EventFactoryManager()
    {
        // 注册默认工厂
        registerFactory(EventCategory::CONTROL, std::make_unique<ControlEventFactory>());
        registerFactory(EventCategory::SOURCE, std::make_unique<SourceEventFactory>());
        registerFactory(EventCategory::PARAMETER, std::make_unique<ParameterEventFactory>());
        registerFactory(EventCategory::PROCESS, std::make_unique<ProcessEventFactory>());
    }

private:
    std::unordered_map<EventCategory, std::unique_ptr<AbstractEventFactory>> factories_;
};

#endif // EVENTFACTORYMANAGER_H
