#ifndef FFRECORDER_H
#define FFRECORDER_H

#include "ffrecorder_p.h"

#include "event/ffeventloop.h"

#include "thread/ffthreadpool.h"

#include <QObject>
#include <mutex>

using namespace FFRecordContextType;

class FFRecorder : public QObject
{
    Q_OBJECT;

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

    bool m_isRecording = false;
};

; // namespace FFRecordContextType

#endif
