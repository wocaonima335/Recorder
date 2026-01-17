#include "ffopensourceevent.h"

using namespace FFRecordContextType;

FFOpenSourceEvent::FFOpenSourceEvent(FFRecorder *recorderCtx,
                                     demuxerType videoSourceType_,
                                     const string &videoUrl_,
                                     demuxerType audioSourceType_,
                                     const string &audioUrl_,
                                     const string &format_)
    : FFEvent(recorderCtx)
{
  videoSourceType = videoSourceType_;
  audioSourceType = audioSourceType_;
  videoUrl = videoUrl_;
  audioUrl = audioUrl_;
  format = format_;
  videoIndex = demuxerIndex[videoSourceType];
  audioIndex = demuxerIndex[audioSourceType];
}

void FFOpenSourceEvent::work()
{

  bool audioOk = initAudio();

  bool videoOk = initVideo();

  if (audioOk && videoOk)
  {
    qDebug()
        << "[FFOpenSourceEvent] Both audio and video initialized, starting...";
    startAudio();
    startVideo();
  }
  else
  {
    qDebug() << "[FFOpenSourceEvent] Initialization failed, audioOk:" << audioOk
             << "videoOk:" << videoOk;
  }
}

bool FFOpenSourceEvent::initAudio()
{

  if (!aDemuxerThread[audioIndex] || !aDemuxer[audioIndex] ||
      !aDecoderThread[audioIndex] || !aDecoder[audioIndex] || !aEncoderThread ||
      !aEncoder || !muxer || !aPktQueue[audioIndex] || !aFrmQueue[audioIndex] ||
      !aEncoderPktQueue)
  {
    qDebug() << "[FFOpenSourceEvent] Audio components not initialized";
    return false;
  }

  aDemuxerThread[audioIndex]->stop();
  aDemuxerThread[audioIndex]->wakeAllThread();
  aDemuxerThread[audioIndex]->wait();
  aDemuxerThread[audioIndex]->close();
  aPktQueue[audioIndex]->clearQueue();

  aDecoderThread[audioIndex]->stop();
  aDecoderThread[audioIndex]->wakeAllThread();
  aDecoderThread[audioIndex]->wait();
  aDecoderThread[audioIndex]->close();
  aFrmQueue[audioIndex]->clearQueue();

  aEncoderThread->stop();
  aEncoderThread->wakeAllThread();
  aEncoderThread->wait();
  aEncoderThread->close();

  aDemuxer[audioIndex]->init(audioUrl, format, aPktQueue[audioIndex], nullptr,
                             audioSourceType);
  aDemuxerThread[audioIndex]->init(aDemuxer[audioIndex]);

  aDecoder[audioIndex]->init(aDemuxer[audioIndex]->getAStream(),
                             aFrmQueue[audioIndex]);
  aDecoderThread[audioIndex]->init(aDecoder[audioIndex], aPktQueue[audioIndex]);

  aEncoder->init(aEncoderPktQueue);
  aEncoderThread->init(aFilter, aEncoder, muxer, aFrmQueue[audioIndex]);
  return true;
}

bool FFOpenSourceEvent::initVideo()
{

  if (!vDemuxerThread[videoIndex] || !vDemuxer[videoIndex] ||
      !vDecoderThread[videoIndex] || !vDecoder[videoIndex] || !vEncoderThread ||
      !vEncoder || !muxer || !muxerThread || !vPktQueue[videoIndex] ||
      !vFrmQueue[videoIndex] || !vEncoderPktQueue)
  {
    qDebug() << "[FFOpenSourceEvent] Video components not initialized";
    return false;
  }

  qDebug() << "[initVideo] Step 1: Stopping vDemuxerThread...";
  vDemuxerThread[videoIndex]->stop();
  qDebug() << "[initVideo] Step 2: Waking vDemuxerThread...";
  vDemuxerThread[videoIndex]->wakeAllThread();
  qDebug() << "[initVideo] Step 3: Waiting vDemuxerThread...";
  vDemuxerThread[videoIndex]->wait();
  qDebug() << "[initVideo] Step 4: Closing vDemuxerThread...";
  vDemuxerThread[videoIndex]->close();
  qDebug() << "[initVideo] Step 5: Clearing vPktQueue...";
  vPktQueue[videoIndex]->clearQueue();

  qDebug() << "[initVideo] Step 6: Stopping vDecoderThread...";
  vDecoderThread[videoIndex]->stop();
  qDebug() << "[initVideo] Step 7: Waking vDecoderThread...";
  vDecoderThread[videoIndex]->wakeAllThread();
  qDebug() << "[initVideo] Step 8: Waiting vDecoderThread...";
  vDecoderThread[videoIndex]->wait();
  qDebug() << "[initVideo] Step 9: Closing vDecoderThread (will call "
              "vDecoder->close())...";
  vDecoderThread[videoIndex]->close();
  qDebug()
      << "[initVideo] Step 10: vDecoderThread closed, clearing vFrmQueue...";
  vFrmQueue[videoIndex]->clearQueue();

  qDebug() << "[initVideo] Step 11: Stopping vEncoderThread...";
  vEncoderThread->stop();
  qDebug() << "[initVideo] Step 12: Waking vEncoderThread...";
  vEncoderThread->wakeAllThread();
  qDebug() << "[initVideo] Step 13: Waiting vEncoderThread...";
  vEncoderThread->wait();
  qDebug() << "[initVideo] Step 14: Closing vEncoderThread...";
  vEncoderThread->close();

  qDebug() << "[initVideo] Step 15: Stopping muxerThread...";
  muxerThread->stop();
  qDebug() << "[initVideo] Step 16: Waking muxerThread...";
  muxerThread->wakeAllThread();
  qDebug() << "[initVideo] Step 17: Waiting muxerThread...";
  muxerThread->wait();
  qDebug() << "[initVideo] Step 18: Closing muxerThread...";
  muxerThread->close();

  qDebug() << "[initVideo] Step 19: Initializing vDemuxer...";
  vDemuxer[videoIndex]->init(videoUrl, format, nullptr, vPktQueue[videoIndex],
                             videoSourceType);
  qDebug() << "[initVideo] Step 20: Initializing vDemuxerThread...";
  vDemuxerThread[videoIndex]->init(vDemuxer[videoIndex]);

  qDebug() << "[initVideo] Step 21: Initializing vDecoder...";
  vDecoder[videoIndex]->init(vDemuxer[videoIndex]->getVStream(),
                             vFrmQueue[videoIndex]);
  qDebug() << "[initVideo] Step 22: Initializing vDecoderThread...";
  vDecoderThread[videoIndex]->init(vDecoder[videoIndex], vPktQueue[videoIndex]);

  qDebug() << "[initVideo] Step 23: Initializing vEncoder...";
  vEncoder->init(vEncoderPktQueue);
  qDebug() << "[initVideo] Step 24: Initializing vEncoderThread...";
  vEncoderThread->init(vFilter, vEncoder, muxer, vFrmQueue[videoIndex]);

  qDebug() << "[initVideo] Step 25: Getting video stream info...";
  // 获取视频流信息并更新分辨率显示
  AVStream *vStream = vDemuxer[videoIndex]->getVStream();
  if (vStream && vStream->codecpar)
  {
    int width = vStream->codecpar->width;
    int height = vStream->codecpar->height;
    QString resolutionStr = QString("%1 X %2 : Screen").arg(width).arg(height);
    qDebug() << "screen size:" << width << height;
    // 在主线程更新 QML 属性
    QMetaObject::invokeMethod(recoderContext, "setVideoResolution",
                              Qt::QueuedConnection,
                              Q_ARG(QString, resolutionStr));
  }

  qDebug() << "[initVideo] Step 26: Initializing muxer...";
  QString fullPath =
      recoderContext->outputPath() + "/" + recoderContext->outputFileName();
  muxer->init(fullPath.toStdString());
  qDebug() << "[initVideo] Step 27: Initializing muxerThread...";
  muxerThread->init(aEncoderPktQueue, vEncoderPktQueue, muxer, aEncoder,
                    vEncoder, recoderContext);
  qDebug() << "[initVideo] Step 28: All done!";
  std::cout << "init opensource" << std::endl;
  return true;
}

void FFOpenSourceEvent::startAudio()
{
  if (!aDemuxerThread[audioIndex] || !aDecoderThread[audioIndex] ||
      !aEncoderThread || !recoderContext)
    return;

  auto *aEncPktQueue = recoderContext->getAEncoderPktQueue();
  if (!aEncPktQueue || !aPktQueue[audioIndex] || !aFrmQueue[audioIndex])
    return;

  aEncPktQueue->start();

  aPktQueue[audioIndex]->start();
  aFrmQueue[audioIndex]->start();

  aDemuxerThread[audioIndex]->wakeAllThread();
  aDemuxerThread[audioIndex]->start();

  aDecoderThread[audioIndex]->wakeAllThread();
  aDecoderThread[audioIndex]->start();

  aEncoderThread->start();
}

void FFOpenSourceEvent::startVideo()
{
  if (!vDemuxerThread[videoIndex] || !vDecoderThread[videoIndex] ||
      !vEncoderThread || !muxerThread || !recoderContext)
    return;

  auto *vEncPktQueue = recoderContext->getVEncoderPktQueue();
  if (!vEncPktQueue || !vPktQueue[videoIndex] || !vFrmQueue[videoIndex])
    return;

  vDemuxerThread[videoIndex]->wakeAllThread();
  vDemuxerThread[videoIndex]->start();

  vDecoderThread[videoIndex]->wakeAllThread();
  vDecoderThread[videoIndex]->start();

  vEncoderThread->start();
  vEncPktQueue->start();

  vPktQueue[videoIndex]->start();
  vFrmQueue[videoIndex]->start();

  muxerThread->start();

  std::cout << "start opensource " << std::endl;
}
