#include "ffrecorder.h"

#include "queue/ffeventqueue.h"

#include <QDebug>

FFRecorder *FFRecorder::m_instance = nullptr;
std::mutex FFRecorder::m_mutex;

FFRecorder &FFRecorder::getInstance()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_instance)
    {
        m_instance = new FFRecorder();
    }
    return *m_instance;
}

void FFRecorder::destoryInstance()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    delete m_instance;
    m_instance = nullptr;
}

void FFRecorder::initialize()
{
    if (m_isRecording)
        return;
    av_log_set_level(AV_LOG_QUIET);

    registerMetaTypes();
    initCoreComponents();
}

void FFRecorder::startRecord()
{
    if (m_isRecording)
        return;

    d->vFilterThread->start();
    d->aFilterThread->start();

    m_eventLoop->start();
    m_isRecording = true;
}

void FFRecorder::stopRecord()
{
    if (!m_isRecording)
        return;

    if (d->vFilterThread) {
        d->vFilterThread->stop();
        d->vFilterThread->wakeAllThread();
        d->vFilterThread->wait();
    }

    if (d->aFilterThread) {
        d->aFilterThread->stop();
        d->aFilterThread->wakeAllThread();
        d->aFilterThread->wait();
    }

    if (m_eventLoop) {
        m_eventLoop->stop();
        m_eventLoop->wakeAllThread();
        m_eventLoop->wait();
    }

    m_isRecording = false;
}

FFRecorderPrivate *FFRecorder::getContext()
{
    return d;
}

void FFRecorder::initCoreComponents()
{
    for (size_t i = 0; i < FFRecordContextType::A_DECODER_SIZE; i++) {
        d->aDecoderPktQueue[i] = new FFAPacketQueue;
        d->aDecoder[i] = new FFADecoder;
        d->aDecoderFrmQueue[i] = new FFAFrameQueue;
        d->aDecoderThread[i] = new FFADecoderThread;
        d->aDemuxer[i] = new Demuxer;
        d->aDemuxerThread[i] = new FFDemuxerThread;
    }

    for (size_t i = 0; i < FFRecordContextType::V_DECODER_SIZE; i++) {
        d->vDecoderPktQueue[i] = new FFVPacketQueue;
        d->vDecoder[i] = new FFVDecoder;
        d->vDecoderFrmQueue[i] = new FFVFrameQueue;
        d->vDecoderThread[i] = new FFVDecoderThread;
        d->vDemuxer[i] = new Demuxer;
        d->vDemuxerThread[i] = new FFDemuxerThread;
    }

    d->aEncoderPktQueue = new FFAPacketQueue();
    d->vEncoderPktQueue = new FFVPacketQueue();

    d->aFilterEncoderFrmQueue = new FFAFrameQueue();
    d->vFilterEncoderFrmQueue = new FFVFrameQueue();

    d->aFilter = new FFAFilter();
    d->vFilter = new FFVFilter();
    d->muxer = new FFMuxer();
    d->aEncoder = new FFAEncoder();
    d->vEncoder = new FFVEncoder();

    d->aFilterThread = new FFAFilterThread;
    d->vFilterThread = new FFVFilterThread;

    d->muxerThread = new FFMuxerThread;
    d->aEncoderThread = new FFAEncoderThread;
    d->vEncoderThread = new FFVEncoderThread;

    d->aFilterThread->init(d->aDecoderFrmQueue[aDecoderType::A_MICROPHONE],
                           d->aDecoderFrmQueue[aDecoderType::A_AUDIO],
                           d->aFilter);
    d->vFilterThread->init(d->vDecoderFrmQueue[vDecoderType::V_SCREEN], d->vFilter);

    m_threadPool = new FFThreadPool();
    m_threadPool->init(4);

    m_eventLoop = new FFEventLoop();
    m_eventLoop->init(&FFEventQueue::getInstance(), m_threadPool);
}

void FFRecorder::registerMetaTypes()
{
    qRegisterMetaType<uint8_t *>("uint8_t*");
    qRegisterMetaType<AVFrame *>("AVFrame*");
    qRegisterMetaType<int64_t>("int64_t");
}

FFRecorder::FFRecorder(QObject *parent)
{
    avdevice_register_all();
    avformat_network_init();
}

FFRecorder::~FFRecorder()
{
    stopRecord();
    delete m_threadPool;
    delete m_eventLoop;
    delete d;
}

FFVFilter *FFRecorder::getVFilter()
{
    return d ? d->vFilter : nullptr;
}

FFAFilter *FFRecorder::getAFilter()
{
    return d ? d->aFilter : nullptr;
}

FFMuxer *FFRecorder::getMuxer()
{
    return d ? d->muxer : nullptr;
}

FFADecoder *FFRecorder::getADecoder(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
        return nullptr;
    return d->aDecoder[index];
}

FFVDecoder *FFRecorder::getVDecoder(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
        return nullptr;
    return d->vDecoder[index];
}

Demuxer *FFRecorder::getADemuxer(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::A_DEMUXER_SIZE)
        return nullptr;
    return d->aDemuxer[index];
}

Demuxer *FFRecorder::getVDemuxer(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::V_DEMUXER_SIZE)
        return nullptr;
    return d->vDemuxer[index];
}

FFDemuxerThread *FFRecorder::getADemuxerThread(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::A_DEMUXER_SIZE)
        return nullptr;
    return d->aDemuxerThread[index];
}

FFDemuxerThread *FFRecorder::getVDemuxerThread(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::V_DEMUXER_SIZE)
        return nullptr;
    return d->vDemuxerThread[index];
}

FFADecoderThread *FFRecorder::getADecoderThread(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
        return nullptr;
    return d->aDecoderThread[index];
}

FFVDecoderThread *FFRecorder::getVDecoderThread(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
        return nullptr;
    return d->vDecoderThread[index];
}

FFAEncoderThread *FFRecorder::getAEncoderThread()
{
    return d ? d->aEncoderThread : nullptr;
}

FFVEncoderThread *FFRecorder::getVEncoderThread()
{
    return d ? d->vEncoderThread : nullptr;
}

FFVFilterThread *FFRecorder::getVFilterThread()
{
    return d ? d->vFilterThread : nullptr;
}

FFAFilterThread *FFRecorder::getAFilterThread()
{
    return d ? d->aFilterThread : nullptr;
}

FFMuxerThread *FFRecorder::getMuxerThread()
{
    return d ? d->muxerThread : nullptr;
}

FFAPacketQueue *FFRecorder::getADecoderPktQueue(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
        return nullptr;
    return d->aDecoderPktQueue[index];
}

FFVPacketQueue *FFRecorder::getVDecoderPktQueue(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
        return nullptr;
    return d->vDecoderPktQueue[index];
}

FFVPacketQueue *FFRecorder::getVEncoderPktQueue()
{
    return d ? d->vEncoderPktQueue : nullptr;
}

FFAPacketQueue *FFRecorder::getAEncoderPktQueue()
{
    return d ? d->aEncoderPktQueue : nullptr;
}

FFAFrameQueue *FFRecorder::getADecoderFrmQueue(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
        return nullptr;
    return d->aDecoderFrmQueue[index];
}

FFVFrameQueue *FFRecorder::getVDecoderFrmQueue(int index)
{
    if (!d || index < 0 || static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
        return nullptr;
    return d->vDecoderFrmQueue[index];
}

FFVFrameQueue *FFRecorder::getVFilterEncoderFrmQueue()
{
    return d ? d->vFilterEncoderFrmQueue : nullptr;
}

FFAFrameQueue *FFRecorder::getAFilterEncoderFrmQueue()
{
    return d ? d->aFilterEncoderFrmQueue : nullptr;
}

FFVFrameQueue *FFRecorder::getVRenderFrmQueue()
{
    // Not explicitly managed in FFRecorderPrivate; return nullptr by default
    return nullptr;
}

FFThreadPool *FFRecorder::getThreadPool()
{
    return m_threadPool;
}

FFEventLoop *FFRecorder::getEventLoop()
{
    return m_eventLoop;
}
