#ifndef FFPAUSEEVENT_H
#define FFPAUSEEVENT_H

#include "ffevent.h"

class FFPauseEvent : public FFEvent
{
public:
    FFPauseEvent(FFRecorder *recordCtx, bool paused_, int64_t ts_us_);
    void work() override;

private:
    bool paused;
    int64_t ts_us;
};

#endif // FFPAUSEEVENT_H
