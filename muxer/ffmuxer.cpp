#include "ffmuxer.h"
#include <libavutil/timestamp.h>

#include <QDebug>
#include <QString>

FFMuxer::FFMuxer() {}

FFMuxer::~FFMuxer() {
  std::lock_guard<std::shared_mutex> lock(mutex);
  if (!headerFlag) {
    writeTrailer();
  }

  if (fmtCtx) {
    avformat_close_input(&fmtCtx);
  }
  url.clear();
}

void FFMuxer::init(const std::string &url_, const std::string &format_) {
  std::lock_guard<std::shared_mutex> lock(mutex);
  url = url_;
  format = format_;
  initMuxer();
}

void FFMuxer::addStream(AVCodecContext *codecCtx) {
  std::lock_guard<std::shared_mutex> lock(mutex);

  // 检查 fmtCtx 是否有效，防止在 muxer 未初始化时崩溃
  if (!fmtCtx) {
    qDebug().noquote()
        << "[Muxer] Cannot add stream: fmtCtx is null! Call init() first.";
    return;
  }

  // 验证编码器上下文
  if (!codecCtx || !codecCtx->codec) {
    qDebug().noquote() << "[Muxer] Invalid codec context!";
    return;
  }

  // 验证时间基
  if (codecCtx->time_base.num <= 0 || codecCtx->time_base.den <= 0) {
    qDebug().noquote() << "[Muxer] Invalid time base:"
                       << codecCtx->time_base.num << "/"
                       << codecCtx->time_base.den;
    return;
  }

  AVStream *stream = avformat_new_stream(fmtCtx, nullptr);

  if (!stream) {
    qDebug().noquote() << "[Muxer] New Stream Fail!";
    return;
  }

  int ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);

  if (ret < 0) {
    qDebug().noquote() << "[Muxer] Copy Parameters From Context Fail!";
    printError(ret);
    return;
  }

  stream->time_base = codecCtx->time_base;
  if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
    aCodecCtx = codecCtx;
    aStream = stream;
    aStreamIndex = stream->index;

  } else if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
    vCodecCtx = codecCtx;
    vStream = stream;
    vStreamIndex = stream->index;
  }

  streamCount++;
  //    qDebug().noquote() << "[Muxer] streamCount =" << streamCount;
  if (streamCount == 2) {
    readyFlag = true; // 直接赋值，由锁保证可见性
  }
}

int FFMuxer::mux(AVPacket *packet) {
  if (trailerFlag.load())
    return 1;

  int streamIndex = packet->stream_index;

  // === 步骤1：打印输入包信息 ===
  const char *streamType = (streamIndex == aStreamIndex)   ? "Audio"
                           : (streamIndex == vStreamIndex) ? "Video"
                                                           : "Unknown";

  // 使用静态缓冲区替代 av_ts2str 宏
  char pts_buf[AV_TS_MAX_STRING_SIZE];
  char dts_buf[AV_TS_MAX_STRING_SIZE];
  char duration_buf[AV_TS_MAX_STRING_SIZE];

  av_ts_make_string(pts_buf, packet->pts);
  av_ts_make_string(dts_buf, packet->dts);
  av_ts_make_string(duration_buf, packet->duration);

  qDebug().noquote() << "[Mux Input]" << streamType
                     << "pkt stream_index=" << streamIndex << "pts=" << pts_buf
                     << "dts=" << dts_buf << "duration=" << duration_buf
                     << "size=" << packet->size;

  AVRational srcTimeBase, dstTimeBase;
  {
    std::lock_guard<std::shared_mutex> lock(mutex);
    if (streamIndex == aStreamIndex) {
      srcTimeBase = aCodecCtx->time_base;
      dstTimeBase = aStream->time_base;
    } else if (streamIndex == vStreamIndex) {
      srcTimeBase = vCodecCtx->time_base;
      dstTimeBase = vStream->time_base;
    } else {
      return -1;
    }
  }

  packet->pts = av_rescale_q(packet->pts, srcTimeBase, dstTimeBase);
  packet->dts = av_rescale_q(packet->dts, srcTimeBase, dstTimeBase);
  packet->duration = av_rescale_q(packet->duration, srcTimeBase, dstTimeBase);

  if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE ||
      packet->pts < 0) {
    av_ts_make_string(pts_buf, packet->pts);
    av_ts_make_string(dts_buf, packet->dts);
    qDebug().noquote() << "[Mux Drop]" << streamType
                       << "pkt dropped: invalid pts/dts (pts=" << pts_buf
                       << ", dts=" << dts_buf << ")";
    return 0;
  }

  // 写入操作（独占锁）
  {
    std::lock_guard<std::shared_mutex> lock(mutex);
    if (fmtCtx == nullptr) {
      return 0;
    }

    // === 步骤4：打印最终写入的包信息 ===
    av_ts_make_string(pts_buf, packet->pts);
    av_ts_make_string(dts_buf, packet->dts);

    qDebug().noquote()
        << "[Mux Write]" << streamType
        << "pkt stream_index=" << packet->stream_index << "pts=" << pts_buf
        << "(" << QString::number(av_q2d(dstTimeBase) * packet->pts, 'f', 6)
        << "s)"
        << "dts=" << dts_buf << "("
        << QString::number(av_q2d(dstTimeBase) * packet->dts, 'f', 6) << "s)"
        << "size=" << packet->size;

    int ret = av_interleaved_write_frame(fmtCtx, packet);
    if (ret < 0) {
      qDebug().noquote() << "[Muxer] Mux Fail!";
      printError(ret);
      return -1;
    }
  }
  return 0;
}

void FFMuxer::writeHeader() {
  while (!readyFlag) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::lock_guard<std::shared_mutex> lock(mutex);

  if (headerFlag) {
    return;
  }

  headerFlag = true;
  if (fmtCtx == nullptr) {
    qDebug().noquote() << "[Muxer] fmtCtx is nullptr";
    return;
  }

  int ret = avformat_write_header(fmtCtx, nullptr);
  if (ret < 0) {
    qDebug().noquote() << "[Muxer] Write Header Fail!";
    printError(ret);
    headerFlag = false; // 恢复状态以防后续错误
  }
}

void FFMuxer::writeTrailer() {
  std::lock_guard<std::shared_mutex> lock(mutex); // 保护尾文件写入状态
  if (trailerFlag) {                              // 直接判断bool变量
    return;
  }
  trailerFlag = true; // 设置状态后再执行写入

  if (fmtCtx == nullptr) {
    return;
  }
  int ret = av_write_trailer(fmtCtx);
  if (ret < 0) {
    qDebug().noquote() << "[Muxer] Write Trailer Fail!";
    printError(ret);
    trailerFlag = false; // 恢复状态以防后续错误
  }
}

int FFMuxer::getAStreamIndex() {
  std::lock_guard<std::shared_mutex> lock(mutex);
  return aStreamIndex;
}

int FFMuxer::getVStreamIndex() {
  std::lock_guard<std::shared_mutex> lock(mutex);
  return vStreamIndex;
}

void FFMuxer::close() {
  qDebug() << "[Muxer] close() called";

  if (fmtCtx) {
    if (fmtCtx->pb)
      avio_closep(&fmtCtx->pb);
    avformat_free_context(fmtCtx);
    fmtCtx = nullptr;
  }

  url.clear();
  format.clear();

  headerFlag = false;
  trailerFlag = false;
  readyFlag = false;

  aStreamIndex = vStreamIndex = -1;
  streamCount = 0;

  aStream = nullptr;
  vStream = nullptr;

  aCodecCtx = nullptr;
  vCodecCtx = nullptr;

  qDebug() << "[Muxer] close() done";
}

void FFMuxer::initMuxer() {
  int ret = avformat_alloc_output_context2(&fmtCtx, nullptr, format.c_str(),
                                           url.c_str());
  if (ret < 0) {
    qDebug().noquote() << "[Muxer] Alloc Output Context Fail!";
    printError(ret);
    return;
  }

  if (format == "rtsp") {
    AVDictionary *opts = nullptr;
    ret = av_opt_set(&opts, "rtsp_transport", "tcp", 0);
    if (ret < 0) {
      qDebug().noquote() << "[Muxer] av_opt_set:rtsp_transport fail";
    }
    ret = av_dict_set(&opts, "stimeout", "5000000", 0);
    if (ret < 0) {
      qDebug().noquote() << "[Muxer] av_dict_set: stimeout fail";
    }

    ret = av_opt_set_dict(fmtCtx, &opts);
    if (ret < 0) {
      qDebug().noquote() << "[Muxer] av_opt_set_dict fail";
    }

    av_dict_free(&opts);

  } else {
    ret = avio_open(&fmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      qDebug().noquote() << "[Muxer] Open File Fail!";
      printError(ret);
      return;
    }
  }
}

void FFMuxer::printError(int ret) {
  char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
  int res = av_strerror(ret, errorBuffer, sizeof(errorBuffer));
  if (res < 0) {
    qDebug().noquote() << "[Muxer] Unknown Error";
  } else {
    qDebug().noquote() << "[Muxer] Error:" << errorBuffer;
  }
}
