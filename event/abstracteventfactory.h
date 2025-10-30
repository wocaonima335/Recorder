#ifndef ABSTRACTEVENTFACTORY_H
#define ABSTRACTEVENTFACTORY_H

#include "eventcategory.h"

#include "recorder/ffrecorder.h"

#include <memory>
#include <string>

class FFEvent;

// 事件参数基类
struct EventParameters
{
    virtual ~EventParameters() = default;
};

// 具体参数类型
struct ControlEventParams : public EventParameters
{
    ControlEventType type;
    std::string url;
    std::string format;
    bool paused = false;
    int64_t ts_us = 0;
};

struct SourceEventParams : public EventParameters
{
    SourceEventType type;
    demuxerType sourceType;
    std::string url;
    std::string format;
};

struct ProcessEventParams : public EventParameters
{
    ProcessEventType type;
    int curSec = 0;
};

class AbstractEventFactory
{
public:
    virtual ~AbstractEventFactory() = default;
    virtual std::unique_ptr<FFEvent> createEvent(FFRecorder *context, const EventParameters &params)
        = 0;
    virtual EventCategory getCategory() const = 0;
};

#endif // ABSTRACTEVENTFACTORY_H
