#include "event/eventfactorymanager.h"
#include "recorder/ffrecorder.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

void init()
{
    FFRecorder::getInstance().initialize();
}

void recordTest()
{
    SourceEventParams params;
    params.type = SourceEventType::OPEN_SOURCE;

    params.sourceType = demuxerType::CAMERA;
    params.url = FFRecordURLS::CAMERA_URL;
    params.format = "dshow";

    auto event = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                &FFRecorder::getInstance(),
                                                                params);
    event->work();

    SourceEventParams audioParams;
    audioParams.type = SourceEventType::OPEN_SOURCE;
    audioParams.sourceType = demuxerType::MICROPHONE;
    audioParams.url = FFRecordURLS::MICROPHONE_URL;
    audioParams.format = "dshow";

    auto audioevent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                     &FFRecorder::getInstance(),
                                                                     audioParams);
    audioevent->work();

    FFRecorder::getInstance().startRecord();

    // Schedule closing the same source after 10 seconds
    QTimer::singleShot(10000, []() {
        SourceEventParams closeParams;
        closeParams.type = SourceEventType::CLOSE_SOURCE;
        closeParams.sourceType = demuxerType::CAMERA;
        auto closeEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                         &FFRecorder::getInstance(),
                                                                         closeParams);
        closeEvent->work();

        SourceEventParams closeAudioParams;
        closeAudioParams.type = SourceEventType::CLOSE_SOURCE;
        closeAudioParams.sourceType = demuxerType::MICROPHONE;
        auto closeAudioEvent = EventFactoryManager::getInstance()
                                   .createEvent(EventCategory::SOURCE,
                                                &FFRecorder::getInstance(),
                                                closeAudioParams);
        closeAudioEvent->work();
    });
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    init();
    recordTest();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []()
        { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("bandicam", "Main");

    return app.exec();
}
