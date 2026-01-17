#include "ffrecorder.h"

#include "event/ffaudiosourcechangeevent.h"
#include "event/ffsourcechangeevent.h"
#include "queue/ffeventqueue.h"

#include <QDebug>

FFRecorder *FFRecorder::m_instance = nullptr;
std::mutex FFRecorder::m_mutex;

FFRecorder &FFRecorder::getInstance() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_instance) {
    m_instance = new FFRecorder();
  }
  return *m_instance;
}

void FFRecorder::destoryInstance() {
  std::lock_guard<std::mutex> lock(m_mutex);
  delete m_instance;
  m_instance = nullptr;
}

void FFRecorder::initialize() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_isRecording.load(std::memory_order_acquire) ||
      m_initialized.load(std::memory_order_acquire))
    return;
  av_log_set_level(AV_LOG_QUIET);

  registerMetaTypes();
  initCoreComponents();
  m_initialized.store(true, std::memory_order_release);
  QMetaObject::invokeMethod(this, "audioSamplerChanged", Qt::QueuedConnection);
}

void FFRecorder::startRecord() {
  if (!m_initialized.load(std::memory_order_acquire))
    initialize();

  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_isRecording.load(std::memory_order_acquire))
    return;

  // 根据当前源重新绑定 vFilter 和 vFilterThread，修复源切换后绑定不更新的问题
  bool useScreen =
      FFSourceChangeEvent::sourceChanged.load(std::memory_order_acquire);
  demuxerType currentSource =
      useScreen ? demuxerType::SCREEN : demuxerType::CAMERA;
  d->vFilterThread->init(d->vDecoderFrmQueue[currentSource], d->vFilter);
  d->vFilter->init(d->vDecoderFrmQueue[currentSource],
                   d->vDecoder[currentSource]);

  bool useSystemAudio =
      FFAudioSourceChangeEvent::useSystemAudio.load(std::memory_order_acquire);
  if (useSystemAudio) {
    d->aFilterThread->openAudioSource(demuxerType::AUDIO);
    d->aFilterThread->closeAudioSource(demuxerType::MICROPHONE);
  } else {
    d->aFilterThread->closeAudioSource(demuxerType::AUDIO);
    d->aFilterThread->openAudioSource(demuxerType::MICROPHONE);
  }

  // 清空所有解码队列，防止残留数据导致时间戳混乱
  for (size_t i = 0; i < FFRecordContextType::V_DECODER_SIZE; i++) {
    if (d->vDecoderFrmQueue[i])
      d->vDecoderFrmQueue[i]->clearQueue();
    if (d->vDecoderPktQueue[i])
      d->vDecoderPktQueue[i]->clearQueue();
  }
  for (size_t i = 0; i < FFRecordContextType::A_DECODER_SIZE; i++) {
    if (d->aDecoderFrmQueue[i])
      d->aDecoderFrmQueue[i]->clearQueue();
    if (d->aDecoderPktQueue[i])
      d->aDecoderPktQueue[i]->clearQueue();
  }
  // 清空编码队列
  if (d->vEncoderPktQueue)
    d->vEncoderPktQueue->clearQueue();
  if (d->aEncoderPktQueue)
    d->aEncoderPktQueue->clearQueue();

  int64_t t0 = av_gettime_relative(); // 统一墙钟起点
  d->aEncoderThread->setStartTimeUs(t0);
  d->vEncoderThread->setStartTimeUs(t0);
  d->audioSampler->clear();
  d->audioSampler->start();
  FFEventQueue::getInstance().start();
  m_eventLoop->start();
  m_isRecording.store(true, std::memory_order_release);
  QMetaObject::invokeMethod(this, "isRecordingChanged", Qt::QueuedConnection);
}

void FFRecorder::stopRecord() {
  if (!m_isRecording.load(std::memory_order_acquire))
    return;

  // 先停止事件循环（不持锁，避免死锁）
  if (m_eventLoop) {
    m_eventLoop->stop();
    m_eventLoop->wakeAllThread();
    m_eventLoop->wait();
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  FFEventQueue::getInstance().clearQueue();
  d->audioSampler->stop();
  d->audioSampler->clear();

  setCaptureTimeText("00:00.0");
  m_isRecording.store(false, std::memory_order_release);
  QMetaObject::invokeMethod(this, "isRecordingChanged", Qt::QueuedConnection);
}

FFRecorderPrivate *FFRecorder::getContext() { return d; }

void FFRecorder::initCoreComponents() {
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

  d->audioSampler = new FFAudioSampler(this);

  d->audioSampler->initialize(48000, 2, AV_SAMPLE_FMT_FLTP);

  d->aFilterThread->init(d->aDecoderFrmQueue[aDecoderType::A_MICROPHONE],
                         d->aDecoderFrmQueue[aDecoderType::A_AUDIO],
                         d->aFilter);

  bool useSystemAudio = FFAudioSourceChangeEvent::useSystemAudio.load();
  if (useSystemAudio) {
    d->aFilterThread->openAudioSource(demuxerType::AUDIO);
    d->aFilterThread->closeAudioSource(demuxerType::MICROPHONE);
  } else {
    d->aFilterThread->closeAudioSource(demuxerType::AUDIO);
    d->aFilterThread->openAudioSource(demuxerType::MICROPHONE);
  }

  bool useScreen = FFSourceChangeEvent::sourceChanged.load();
  demuxerType initialSource =
      useScreen ? demuxerType::SCREEN : demuxerType::CAMERA;

  d->vFilterThread->init(d->vDecoderFrmQueue[initialSource], d->vFilter);

  d->vFilter->init(d->vDecoderFrmQueue[initialSource],
                   d->vDecoder[initialSource]);

  m_threadPool = new FFThreadPool();
  m_threadPool->init(4);

  m_eventLoop = new FFEventLoop();
  m_eventLoop->init(&FFEventQueue::getInstance(), m_threadPool);
}

void FFRecorder::registerMetaTypes() {
  qRegisterMetaType<uint8_t *>("uint8_t*");
  qRegisterMetaType<AVFrame *>("AVFrame*");
  qRegisterMetaType<int64_t>("int64_t");
}

FFRecorder::FFRecorder(QObject *parent) {
  Q_UNUSED(parent);
  avdevice_register_all();
  avformat_network_init();
  d = new FFRecorderPrivate();
}

FFRecorder::~FFRecorder() {
  stopRecord();
  delete m_threadPool;
  delete m_eventLoop;
  delete d;
}

FFVFilter *FFRecorder::getVFilter() { return d ? d->vFilter : nullptr; }

FFAFilter *FFRecorder::getAFilter() { return d ? d->aFilter : nullptr; }

FFAEncoder *FFRecorder::getAEncoder() { return d ? d->aEncoder : nullptr; }

FFVEncoder *FFRecorder::getVEncoder() { return d ? d->vEncoder : nullptr; }

FFMuxer *FFRecorder::getMuxer() { return d ? d->muxer : nullptr; }

FFAudioSampler *FFRecorder::getSampler() {
  return d ? d->audioSampler : nullptr;
}

QObject *FFRecorder::audioSampler() const {
  return d ? d->audioSampler : nullptr;
}

FFADecoder *FFRecorder::getADecoder(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
    return nullptr;
  return d->aDecoder[index];
}

FFVDecoder *FFRecorder::getVDecoder(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
    return nullptr;
  return d->vDecoder[index];
}

Demuxer *FFRecorder::getADemuxer(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::A_DEMUXER_SIZE)
    return nullptr;
  return d->aDemuxer[index];
}

Demuxer *FFRecorder::getVDemuxer(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::V_DEMUXER_SIZE)
    return nullptr;
  return d->vDemuxer[index];
}

FFDemuxerThread *FFRecorder::getADemuxerThread(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::A_DEMUXER_SIZE)
    return nullptr;
  return d->aDemuxerThread[index];
}

FFDemuxerThread *FFRecorder::getVDemuxerThread(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::V_DEMUXER_SIZE)
    return nullptr;
  return d->vDemuxerThread[index];
}

FFADecoderThread *FFRecorder::getADecoderThread(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
    return nullptr;
  return d->aDecoderThread[index];
}

FFVDecoderThread *FFRecorder::getVDecoderThread(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
    return nullptr;
  return d->vDecoderThread[index];
}

FFAEncoderThread *FFRecorder::getAEncoderThread() {
  return d ? d->aEncoderThread : nullptr;
}

FFVEncoderThread *FFRecorder::getVEncoderThread() {
  return d ? d->vEncoderThread : nullptr;
}

FFVFilterThread *FFRecorder::getVFilterThread() {
  return d ? d->vFilterThread : nullptr;
}

FFAFilterThread *FFRecorder::getAFilterThread() {
  return d ? d->aFilterThread : nullptr;
}

FFMuxerThread *FFRecorder::getMuxerThread() {
  return d ? d->muxerThread : nullptr;
}

FFAPacketQueue *FFRecorder::getADecoderPktQueue(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
    return nullptr;
  return d->aDecoderPktQueue[index];
}

FFVPacketQueue *FFRecorder::getVDecoderPktQueue(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
    return nullptr;
  return d->vDecoderPktQueue[index];
}

FFVPacketQueue *FFRecorder::getVEncoderPktQueue() {
  return d ? d->vEncoderPktQueue : nullptr;
}

FFAPacketQueue *FFRecorder::getAEncoderPktQueue() {
  return d ? d->aEncoderPktQueue : nullptr;
}

FFAFrameQueue *FFRecorder::getADecoderFrmQueue(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::A_DECODER_SIZE)
    return nullptr;
  return d->aDecoderFrmQueue[index];
}

FFVFrameQueue *FFRecorder::getVDecoderFrmQueue(int index) {
  if (!d || index < 0 ||
      static_cast<size_t>(index) >= FFRecordContextType::V_DECODER_SIZE)
    return nullptr;
  return d->vDecoderFrmQueue[index];
}

FFVFrameQueue *FFRecorder::getVFilterEncoderFrmQueue() {
  return d ? d->vFilterEncoderFrmQueue : nullptr;
}

FFAFrameQueue *FFRecorder::getAFilterEncoderFrmQueue() {
  return d ? d->aFilterEncoderFrmQueue : nullptr;
}

FFThreadPool *FFRecorder::getThreadPool() { return m_threadPool; }

FFEventLoop *FFRecorder::getEventLoop() { return m_eventLoop; }

QString FFRecorder::captureTimeText() const { return m_captureTimeText; }

void FFRecorder::setCaptureTimeText(const QString &timeText) {
  if (m_captureTimeText != timeText) {
    m_captureTimeText = timeText;
    emit captureTimeTextChanged();
  }
}

void FFRecorder::setOutputPath(const QString &path) {
  if (m_outputPath != path) {
    m_outputPath = path;
    emit outputPathChanged();
  }
}

void FFRecorder::setOutputFileName(const QString &fileName) {
  if (m_outputFileName != fileName) {
    m_outputFileName = fileName;
    emit outputFileNameChanged();
  }
}

void FFRecorder::setVideoResolution(const QString &resolution) {
  if (m_videoResolution != resolution) {
    m_videoResolution = resolution;
    emit videoResolutionChanged();
  }
}
