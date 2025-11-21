#ifndef FFSOURCECHANGEEVENT_H
#define FFSOURCECHANGEEVENT_H

#include "ffevent.h"

// 控制层触发的源切换/打开事件，内部再派发 Source 类事件
class FFSourceChangeEvent : public FFEvent
{
public:
    FFSourceChangeEvent(FFRecorder *recorderCtx, bool useScreen_);
    void work() override;
    static bool sourceChanged; // true：屏幕，false：摄像头

private:
    bool useScreen;
};

#endif // FFSOURCECHANGEEVENT_H
