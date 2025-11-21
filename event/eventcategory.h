#ifndef EVENTCATEGORY_H
#define EVENTCATEGORY_H

// 事件类型枚举
enum class EventCategory {
    CONTROL,   // 控制类事件
    SOURCE,    // 源管理事件
    PARAMETER, // 参数调节事件
    PROCESS    // 处理类事件
};

enum class ControlEventType { START, STOP, PAUSE, READY, END, SOURCECHANGE };

enum class SourceEventType { OPEN_SOURCE, CLOSE_SOURCE };

enum class ParameterEventType { VOLUME, BEAUTY, SPEED, SEEK };

enum class ProcessEventType { CAPTURE_PROCESS };

#endif // EVENTCATEGORY_H
