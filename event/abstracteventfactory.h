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
};

struct SourceEventParams : public EventParameters
{
    SourceEventType type;
    demuxerType sourceType;
    std::string url;
    std::string format;
};

struct ParameterEventParams : public EventParameters
{
    ParameterEventType type;
    std::variant<double, int, std::pair<int, int>> value;
    demuxerType sourceType = CAMERA;
};

struct ProcessEventParams : public EventParameters
{
    ParameterEventType type;
    int curSec;
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
