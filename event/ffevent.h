#ifndef FFEVENT_H
#define FFEVENT_H
#include "decoder/ffadecoder.h"
#include "decoder/ffvdecoder.h"
#include "demuxer/demuxer.h"

#include "filter/ffafilter.h"
#include "filter/ffvfilter.h"

#include "muxer/ffmuxer.h"

#include "queue/ffaframequeue.h"
#include "queue/ffapacketqueue.h"
#include "queue/ffvframequeue.h"
#include "queue/ffvpacketqueue.h"

#include <QMetaObject>

constexpr size_t A_DECODER_SIZE = 2;
constexpr size_t A_DEMUXER_SIZE = 2;
constexpr size_t V_DECODER_SIZE = 3;
constexpr size_t V_DEMUXER_SIZE = 3;

class FFAFilterThread;
class FFVFilterThread;
class FFCapWindow;
class FFCaptureContext;
class FFAEncoderThread;
class FFVEncoderThread;
class FFMuxerThread;
class FFADecoderThread;
class FFVDecoderThread;
class FFDemuxerThread;
class FFDemuxerThread;
class FFAEncoder;
class FFVEncoder;
class FFVRender;

class FFEvent
{
public:
    FFEvent(FFCaptureContext *captureContext);
    virtual ~FFEvent();
    virtual void work() = 0;

protected:
    //=== 1. 上下文管理 ===
    FFCaptureContext *captureContext = nullptr; // 全局上下文

    //=== 2. 队列管理 ===
    // 2.1 音视频包队列
    FFAPacketQueue *aPktQueue[A_DEMUXER_SIZE]; // 音频包队列数组
    FFVPacketQueue *vPktQueue[V_DEMUXER_SIZE]; // 视频包队列数组
    FFAPacketQueue *aEncoderPktQueue;          // 音频编码包队列
    FFVPacketQueue *vEncoderPktQueue;          // 视频编码包队列

    // 2.2 音视频帧队列
    FFAFrameQueue *aFrmQueue[A_DECODER_SIZE]; // 音频帧队列数组
    FFVFrameQueue *vFrmQueue[V_DECODER_SIZE]; // 视频帧队列数组
    FFVFrameQueue *vRenderFrmQueue;           // 视频渲染帧队列
    FFAFrameQueue *aFilterEncoderFrmQueue;    // 音频滤镜编码帧队列
    FFVFrameQueue *vFilterEncoderFrmQueue;    // 视频滤镜编码帧队列

    //=== 3. 处理器组件 ===
    // 3.1 解复用器
    Demuxer *aDemuxer[A_DEMUXER_SIZE]; // 音频解复用器数组
    Demuxer *vDemuxer[V_DEMUXER_SIZE]; // 视频解复用器数组

    // 3.2 解码器
    FFADecoder *aDecoder[A_DECODER_SIZE]; // 音频解码器数组
    FFVDecoder *vDecoder[V_DECODER_SIZE]; // 视频解码器数组

    // 3.3 过滤器
    FFAFilter *aFilter; // 音频混合过滤器
    FFVFilter *vFilter; // 视频过滤器

    // 3.4 编码器
    FFAEncoder *aEncoder; // 音频编码器
    FFVEncoder *vEncoder; // 视频编码器

    // 3.5 复用器
    FFMuxer *muxer; // 复用器

    //=== 4. 线程管理 ===
    // 4.1 解复用线程
    FFDemuxerThread *aDemuxerThread[A_DEMUXER_SIZE]; // 音频解复用线程数组
    FFDemuxerThread *vDemuxerThread[V_DEMUXER_SIZE]; // 视频解复用线程数组

    // 4.2 解码线程
    FFADecoderThread *aDecoderThread[A_DECODER_SIZE]; // 音频解码线程数组
    FFVDecoderThread *vDecoderThread[V_DECODER_SIZE]; // 视频解码线程数组

    // 4.3 过滤线程
    FFAFilterThread *aFilterThread; // 音频过滤线程
    FFVFilterThread *vFilterThread; // 视频过滤线程

    // 4.4 编码线程
    FFAEncoderThread *aEncoderThread; // 音频编码线程
    FFVEncoderThread *vEncoderThread; // 视频编码线程

    // 4.5 复用线程
    FFMuxerThread *muxerThread; // 复用线程

    //=== 5. 渲染和UI ===
    FFVRender *vRender = nullptr; // 视频渲染器
    FFCapWindow *capWindow;       // UI窗口
};

#endif // FFEVENT_H
