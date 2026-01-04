#include "ffsourcechangeevent.h"

#include "abstracteventfactory.h"
#include "eventcategory.h"
#include "eventfactorymanager.h"
#include "queue/ffeventqueue.h"
#include "recorder/ffrecorder.h"
#include "recorder/ffrecorder_p.h"

std::atomic<bool> FFSourceChangeEvent::sourceChanged{true};

FFSourceChangeEvent::FFSourceChangeEvent(FFRecorder *recorderCtx, bool useScreen_)
    : FFEvent(recorderCtx)
    , useScreen(useScreen_)
{}

void FFSourceChangeEvent::work()
{
    bool current = sourceChanged.load(std::memory_order_acquire);
    if (current == useScreen) {
        return;
    }

    sourceChanged.store(useScreen, std::memory_order_release);

    if (!recoderContext || !recoderContext->isRecording()) {
        return;
    }

    using namespace FFRecordContextType;

    demuxerType closeType = current ? demuxerType::SCREEN : demuxerType::CAMERA;
    SourceEventParams closeParams;
    closeParams.type = SourceEventType::CLOSE_SOURCE;
    closeParams.sourceType = closeType;
    auto closeEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                      recoderContext,
                                                                      closeParams);
    if (closeEvent) {
        FFEventQueue::getInstance().enqueue(closeEvent.release());
    }

    demuxerType openType = useScreen ? demuxerType::SCREEN : demuxerType::CAMERA;
    SourceEventParams openParams;
    openParams.type = SourceEventType::OPEN_SOURCE;
    openParams.sourceType = openType;
    openParams.url = useScreen ? FFRecordURLS::SCREEN_URL : FFRecordURLS::CAMERA_URL;
    openParams.format = "dshow";
    auto openEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                     recoderContext,
                                                                     openParams);
    if (openEvent) {
        FFEventQueue::getInstance().enqueue(openEvent.release());
    }
}
