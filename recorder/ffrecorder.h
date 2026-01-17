#ifndef FFRECORDER_H
#define FFRECORDER_H

#include "ffrecorder_p.h"

#include "event/ffeventloop.h"
#include "thread/ffthreadpool.h"

#include <QObject>
#include <atomic>
#include <mutex>

using namespace FFRecordContextType;

class FFRecorder : public QObject {
  Q_OBJECT;
  Q_PROPERTY(QString captureTimeText READ captureTimeText NOTIFY
                 captureTimeTextChanged)
  Q_PROPERTY(QObject *audioSampler READ audioSampler NOTIFY audioSamplerChanged)
  Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
  Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY
                 outputPathChanged)
  Q_PROPERTY(QString outputFileName READ outputFileName WRITE setOutputFileName
                 NOTIFY outputFileNameChanged)
  Q_PROPERTY(QString videoResolution READ videoResolution WRITE
                 setVideoResolution NOTIFY videoResolutionChanged)

public:
  static FFRecorder &getInstance();
  static void destoryInstance();

  FFRecorder(const FFRecorder &) = delete;
  FFRecorder &operator=(const FFRecorder &) = delete;
  void initialize();
  void startRecord();
  void stopRecord();

  class FFRecorderPrivate *getContext();

  FFADecoder *getADecoder(int index);
  FFVDecoder *getVDecoder(int index);
  Demuxer *getADemuxer(int index);
  Demuxer *getVDemuxer(int index);
  FFVFilter *getVFilter();
  FFAFilter *getAFilter();

  FFAEncoder *getAEncoder();
  FFVEncoder *getVEncoder();
  FFMuxer *getMuxer();

  FFAudioSampler *getSampler();

  FFDemuxerThread *getADemuxerThread(int index);
  FFDemuxerThread *getVDemuxerThread(int index);
  FFADecoderThread *getADecoderThread(int index);
  FFVDecoderThread *getVDecoderThread(int index);
  FFAEncoderThread *getAEncoderThread();
  FFVEncoderThread *getVEncoderThread();
  FFVFilterThread *getVFilterThread();
  FFAFilterThread *getAFilterThread();
  FFMuxerThread *getMuxerThread();

  FFAPacketQueue *getADecoderPktQueue(int index);
  FFVPacketQueue *getVDecoderPktQueue(int index);

  FFVPacketQueue *getVEncoderPktQueue();
  FFAPacketQueue *getAEncoderPktQueue();

  FFAFrameQueue *getADecoderFrmQueue(int index);
  FFVFrameQueue *getVDecoderFrmQueue(int index);

  FFVFrameQueue *getVFilterEncoderFrmQueue();
  FFAFrameQueue *getAFilterEncoderFrmQueue();

  FFThreadPool *getThreadPool();
  FFEventLoop *getEventLoop();

  QString captureTimeText() const;
  bool isRecording() const {
    return m_isRecording.load(std::memory_order_acquire);
  }
  QObject *audioSampler() const;

  QString outputPath() const { return m_outputPath; }
  void setOutputPath(const QString &path);

  QString outputFileName() const { return m_outputFileName; }
  void setOutputFileName(const QString &fileName);

  QString videoResolution() const { return m_videoResolution; }

public slots:
  void setVideoResolution(const QString &resolution);
  void setCaptureTimeText(const QString &timeText);

signals:
  void captureTimeTextChanged();
  void audioSamplerChanged();
  void isRecordingChanged();
  void outputPathChanged();
  void outputFileNameChanged();
  void videoResolutionChanged();

private:
  void initCoreComponents();
  void registerMetaTypes();

private:
  explicit FFRecorder(QObject *parent = nullptr);
  ~FFRecorder();

  FFRecorderPrivate *d = nullptr;
  static FFRecorder *m_instance;
  static std::mutex m_mutex;

  FFThreadPool *m_threadPool = nullptr;
  FFEventLoop *m_eventLoop = nullptr;

  std::atomic<bool> m_isRecording{false};
  std::atomic<bool> m_initialized{false};

  QString m_captureTimeText = "00:00.0";
  QString m_outputPath = "E:/Videos";
  QString m_outputFileName = "output.mp4";
  QString m_videoResolution = "0 X 0 : Screen";
};

#endif
