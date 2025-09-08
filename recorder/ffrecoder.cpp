#include "ffrecorder.h"

#include <QDebug>

FFRecorder *FFRecorder::m_instance = nullptr;
std::mutex FFRecorder::m_mutex;

FFRecorder &FFRecorder::getInstance()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_instance) {
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

void FFRecorder::initCoreComponents() {}

void FFRecorder::initRecorderContext() {}

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
