#include "ffpauseevent.h"

FFPauseEvent::FFPauseEvent(FFRecorder *recordCtx, bool paused_, int64_t ts_us_)
    : FFEvent(recordCtx)
    , paused(paused_)
    , ts_us(ts_us_)
{}

void FFPauseEvent::work()
{
    if (auto *vThread = recoderContext->getVEncoderThread()) {
        vThread->onPauseChanged(paused, ts_us);
    }
    if (auto *aThread = recoderContext->getAEncoderThread()) {
        aThread->onPauseChanged(paused, ts_us);
    }
}
