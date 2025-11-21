#include "event/eventfactorymanager.h"
#include "opengl/ffglitem.h"
#include "recorder/ffrecorder.h"

#include "event/ffclosesourceevent.h"
#include "event/ffopensourceevent.h"
#include "event/ffsourcechangeevent.h"
#include "queue/ffeventqueue.h"

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

QFile logFile;

void qtLogHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    static std::mutex m;
    std::lock_guard<std::mutex> lk(m);
    if (!logFile.isOpen())
    {
        logFile.setFileName("bandicam.log");
        logFile.open(QIODevice::Append | QIODevice::Text);
    }
    QTextStream ts(&logFile);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << msg << Qt::endl;
}

void init()
{
    FFRecorder::getInstance().initialize();
}

// 录制控制函数
void startRecording()
{
    qDebug() << "开始录制...";

    // 初始化录制器
    FFRecorder::getInstance().initialize();

    // 开始录制
    FFRecorder::getInstance().startRecord();

    const bool useScreen = FFSourceChangeEvent::sourceChanged;
    SourceEventParams videoParams;
    videoParams.type = SourceEventType::OPEN_SOURCE;
    videoParams.sourceType = useScreen ? demuxerType::SCREEN : demuxerType::CAMERA;
    videoParams.url = useScreen ? FFRecordURLS::SCREEN_URL : FFRecordURLS::CAMERA_URL;
    videoParams.format = "dshow";

    auto videoEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                     &FFRecorder::getInstance(),
                                                                     videoParams);
    FFEventQueue::getInstance().enqueue(videoEvent.release());

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

    // 关闭视频源
    const bool useScreen = FFSourceChangeEvent::sourceChanged;
    SourceEventParams closeVideoParams;
    closeVideoParams.type = SourceEventType::CLOSE_SOURCE;
    closeVideoParams.sourceType = useScreen ? demuxerType::SCREEN : demuxerType::CAMERA;
    auto closeVideoEvent = EventFactoryManager::getInstance().createEvent(EventCategory::SOURCE,
                                                                          &FFRecorder::getInstance(),
                                                                          closeVideoParams);
    FFEventQueue::getInstance().enqueue(closeVideoEvent.release());

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

// 点击屏幕/摄像头区域时触发的源打开控制事件
void openSourceByControl(bool useScreen)
{
    ControlEventParams params;
    params.type = ControlEventType::SOURCECHANGE;
    params.useScreen = useScreen;
    params.sourceType = useScreen ? demuxerType::SCREEN : demuxerType::CAMERA;
    params.url = useScreen ? FFRecordURLS::SCREEN_URL : FFRecordURLS::CAMERA_URL;
    params.format = "dshow";

    auto evt = EventFactoryManager::getInstance().createEvent(EventCategory::CONTROL,
                                                              &FFRecorder::getInstance(),
                                                              params);

    evt->work();
}

class QmlBridge : public QObject
{
    Q_OBJECT
public:
    explicit QmlBridge(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public slots:
    void onStopRecording() { stopRecording(); }
    void onPauseRecording() { pauseRecording(); }
    void onStartRecording() { startRecording(); }
    void onReadyRecording() { readyRecording(); }
    void onOpenScreen() { openSourceByControl(true); }
    void onOpenCamera() { openSourceByControl(false); }
};

int main(int argc, char *argv[])
{
    qInstallMessageHandler(qtLogHandler);
    QGuiApplication app(argc, argv);

    init();

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []()
        { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // Register the recorder object with QML context before loading the module
    qmlRegisterType<FFGLItem>("GLPreview", 1, 0, "GLView");
    engine.rootContext()->setContextProperty("recorder", &FFRecorder::getInstance());
    engine.loadFromModule("bandicam", "Main");

    if (!engine.rootObjects().isEmpty())
    {
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
        QObject::connect(root, SIGNAL(openScreen()), &bridge, SLOT(onOpenScreen()));
        QObject::connect(root, SIGNAL(openCamera()), &bridge, SLOT(onOpenCamera()));
    }
    else
    {
        qDebug() << "Failed to load QML root object";
    }

    return app.exec();
}
#include "main.moc"

