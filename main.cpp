#include "event/eventfactorymanager.h"
#include "recorder/ffrecorder.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

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
