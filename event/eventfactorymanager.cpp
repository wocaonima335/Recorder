#include "eventfactorymanager.h"

void EventFactoryManager::registerFactory(EventCategory cateGory,
                                          std::unique_ptr<AbstractEventFactory> factory)
{
    if (factory) {
        factories_.emplace(cateGory, std::move(factory));
    }
}

std::unique_ptr<FFEvent> EventFactoryManager::createEvent(EventCategory category,
                                                          FFRecorder *context,
                                                          const EventParameters &params)
{
    auto factory = factories_.find(category);
    if (factory == factories_.end()) {
        return nullptr;
    }
    return factory->second->createEvent(context, params);
}
