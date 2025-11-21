#include "ffsourcechangeevent.h"

#include "recorder/ffrecorder.h"

bool FFSourceChangeEvent::sourceChanged = true;

FFSourceChangeEvent::FFSourceChangeEvent(FFRecorder *recorderCtx, bool useScreen_)
    : FFEvent(recorderCtx)
    , useScreen(useScreen_)
{}

void FFSourceChangeEvent::work()
{
    sourceChanged = useScreen;
}
