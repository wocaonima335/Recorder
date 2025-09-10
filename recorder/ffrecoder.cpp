#include "ffrecorder.h"

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

FFCaptureContext *FFRecorder::getCaptureContext() {}

void FFRecorder::initCoreComponents()
{
    for (int i = 0; i < FFRecordContextType::A_DECODER_SIZE; i++) {
        d->aDecoderPktQueue[i] = new FFAPacketQueue;
        d->aDecoder[i] = new FFADecoder;
        d->aDecoderFrmQueue[i] = new FFAFrameQueue;
        d->aDecoderThread[i] = new FFADecoderThread;
        d->aDemuxer[i] = new Demuxer;
        d->aDemuxerThread[i] = new FFDemuxerThread;
    }

    for (int i = 0; i < FFRecordContextType::V_DECODER_SIZE; i++) {
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
    delete m_eventPool;
    delete d;
}
