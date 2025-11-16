#include "event/eventfactorymanager.h"
#include "opengl/ffglitem.h"
#include "recorder/ffrecorder.h"

#include "event/ffclosesourceevent.h"
#include "event/ffopensourceevent.h"
#include "queue/ffeventqueue.h"

#include <QDebug>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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

// 录制控制函数
void startRecording()
{
    qDebug() << "开始录制...";

    // 初始化录制器
    FFRecorder::getInstance().initialize();

    // 开始录制
    FFRecorder::getInstance().startRecord();

    SourceEventParams cameraParams;
    cameraParams.type = SourceEventType::OPEN_SOURCE;
    cameraParams.sourceType = demuxerType::SCREEN;
    cameraParams.url = FFRecordURLS::SCREEN_URL;
    cameraParams.format = "dshow";

    auto cameraEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                      &FFRecorder::getInstance(),
                                                                      cameraParams);
    FFEventQueue::getInstance().enqueue(cameraEvent.release());

    SourceEventParams audioParams;
    audioParams.type = SourceEventType::OPEN_SOURCE;
    audioParams.sourceType = demuxerType::AUDIO;
    audioParams.url = FFRecordURLS::AUDIO_URL;
    audioParams.format = "dshow";

    auto audioEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                     &FFRecorder::getInstance(),
                                                                     audioParams);
    FFEventQueue::getInstance().enqueue(audioEvent.release());
}

void stopRecording()
{
    qDebug() << "停止录制...";
    
    // 关闭摄像头源
    SourceEventParams closeCameraParams;
    closeCameraParams.type = SourceEventType::CLOSE_SOURCE;
    closeCameraParams.sourceType = demuxerType::SCREEN;
    auto closeCameraEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                           &FFRecorder::getInstance(),
                                                                           closeCameraParams);
    FFEventQueue::getInstance().enqueue(closeCameraEvent.release());

    // 关闭麦克风源
    SourceEventParams closeAudioParams;
    closeAudioParams.type = SourceEventType::CLOSE_SOURCE;
    closeAudioParams.sourceType = demuxerType::AUDIO;
    auto closeAudioEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                          &FFRecorder::getInstance(),
                                                                          closeAudioParams);
    closeAudioEvent->work();
    
    // 停止录制
    FFEventQueue::getInstance().enqueue(closeAudioEvent.release());

    FFRecorder::getInstance().stopRecord();
}

void pauseRecording()
{
    ControlEventParams pauseRecordParams;
    pauseRecordParams.type = ControlEventType::PAUSE;
    pauseRecordParams.paused = true;
    pauseRecordParams.ts_us = av_gettime_relative();

    auto pauseRecordEvent = EventFactoryManager::getInstance().createEvent(EventCategory::CONTROL,
                                                                           &FFRecorder::getInstance(),
                                                                           pauseRecordParams);
    FFEventQueue::getInstance().enqueue(pauseRecordEvent.release());
}

void readyRecording()
{
    ControlEventParams readyRecordParams;
    readyRecordParams.type = ControlEventType::READY;
    readyRecordParams.paused = false;
    readyRecordParams.ts_us = av_gettime_relative();

    auto readyRecordEvent = EventFactoryManager::getInstance().createEvent(EventCategory::CONTROL,
                                                                           &FFRecorder::getInstance(),
                                                                           readyRecordParams);
    FFEventQueue::getInstance().enqueue(readyRecordEvent.release());
}

class QmlBridge : public QObject
{
    Q_OBJECT
public:
    explicit QmlBridge(QObject *parent = nullptr)
        : QObject(parent)
    {}

public slots:
    void onStopRecording() { stopRecording(); }
    void onPauseRecording() { pauseRecording(); }
    void onStartRecording() { startRecording(); }
    void onReadyRecording() { readyRecording(); }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    init();

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    
    // Register the recorder object with QML context before loading the module
    qmlRegisterType<FFGLItem>("GLPreview", 1, 0, "GLView");
    engine.rootContext()->setContextProperty("recorder", &FFRecorder::getInstance());
    engine.loadFromModule("bandicam", "Main");

    if (!engine.rootObjects().isEmpty()) {
        auto root = engine.rootObjects().first();

        // 绑定预览项
        QObject *obj = root->findChild<QObject *>("previewView", Qt::FindChildrenRecursively);
        auto previewItem = qobject_cast<FFGLItem *>(obj);
        PreviewBridge::instance().setPreviewItem(previewItem);
        qDebug() << "PreviewItem set:" << previewItem;

        static QmlBridge bridge;
        QObject::connect(root, SIGNAL(startRecording()), &bridge, SLOT(onStartRecording()));
        QObject::connect(root, SIGNAL(stopRecording()), &bridge, SLOT(onStopRecording()));
        QObject::connect(root, SIGNAL(pauseRecording()), &bridge, SLOT(onPauseRecording()));
        QObject::connect(root, SIGNAL(readyRecording()), &bridge, SLOT(onReadyRecording()));

    } else {
        qDebug() << "Failed to load QML root object";
    }

    return app.exec();
}
#include "main.moc"
