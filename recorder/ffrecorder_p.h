#ifndef FFRECORDER_P_H
#define FFRECORDER_P_H

#include <QObject>
#include <QtWidgets/QApplication>

// 前向声明
class FFCapWindow;
class FFDemuxer;
class FFADecoder;
class FFVDecoder;
class FFVRender;
class FFMuxer;
class FFAEncoder;
class FFVEncoder;
class FFVFilter;
class FFAFilter;
class FFEventLoop;
class FFThreadPool;

namespace FFRecordURLS {
static std::string CAMERA1_URL = "video=Integrated Camera";
static std::string CAMERA2_URL = "video=USB2.0 Camera";
static std::string SCREEN_URL = "video=screen-capture-recorder";
static std::string AUDIO_URL = "audio=virtual-audio-capturer";
static std::string MICROPHONE_URL = "audio=麦克风阵列 (Realtek(R) Audio)";
}; // namespace FFRecordURLS

namespace FFRecordContextType {
constexpr size_t A_DECODER_SIZE = 2;
constexpr size_t A_DEMUXER_SIZE = 2;
constexpr size_t V_DECODER_SIZE = 3;
constexpr size_t V_DEMUXER_SIZE = 3;

enum demuxerType { SCREEN, CAMERA, VIDEO, AUDIO, MICROPHONE, NOTYPE };

constexpr int demuxerIndex[A_DEMUXER_SIZE + V_DEMUXER_SIZE + 1]{0, 1, 2, 0, 1, -1};

class FFRecorderPrivate
{
public:
    FFRecorderPrivate();
    ~FFRecorderPrivate();

    //音频解码线程：【声卡】、【麦克风】
    class FFADecoderThread *aDecoderThread[FFRecordContextType::A_DECODER_SIZE];

    //视频解码线程：【屏幕】、【摄像头】、【视频】
    class FFVDecoderThread *vDecoderThread[FFRecordContextType::V_DECODER_SIZE];

    //音频解复用线程：【音频】、【麦克风】
    class FFDemuxerThread *aDemuxerThread[FFRecordContextType::A_DEMUXER_SIZE];
    //视频解复用线程 【屏幕】、【摄像头】、【视频】
    class FFDemuxerThread *vDemuxerThread[FFRecordContextType::V_DEMUXER_SIZE];

    //音频解码packet包队列
    class FFAPacketQueue *aDecoderPktQueue[FFRecordContextType::A_DECODER_SIZE];

    //视频解码packet包队列
    class FFVPacketQueue *vDecoderPktQueue[FFRecordContextType::V_DECODER_SIZE];

    //视频编码packet包队列
    class FFVPacketQueue *vEncoderPktQueue;

    //音频编码packet包队列
    class FFAPacketQueue *aEncoderPktQueue;

    //音频解码帧队列
    class FFAFrameQueue *aDecoderFrmQueue[FFRecordContextType::A_DECODER_SIZE];

    //视频解码帧队列
    class FFVFrameQueue *vDecoderFrmQueue[FFRecordContextType::V_DECODER_SIZE];

    //视频filter编码Frame队列
    class FFVFrameQueue *vFilterEncoderFrmQueue;

    //音频filter编码Frame队列
    class FFAFrameQueue *aFilterEncoderFrmQueue;

    //视频渲染帧队列：【视频文件】
    class FFVFrameQueue *vRenderFrmQueue;

    //音频解复用器
    FFDemuxer *aDemuxer[FFRecordContextType::A_DEMUXER_SIZE];

    //视频解复用器
    FFDemuxer *vDemuxer[FFRecordContextType::V_DEMUXER_SIZE];

    //音频解码器
    FFADecoder *aDecoder[FFRecordContextType::A_DECODER_SIZE];

    //视频解码器
    FFVDecoder *vDecoder[FFRecordContextType::V_DECODER_SIZE];

    //视频渲染器
    FFVRender *vRender;

    //复用器
    FFMuxer *muxer;

    //复用线程
    class FFMuxerThread *muxerThread;

    //音频编码器
    FFAEncoder *aEncoder;

    //音频编码线程
    class FFAEncoderThread *aEncoderThread;

    //视频编码器
    FFVEncoder *vEncoder;

    //视频编码线程
    class FFVEncoderThread *vEncoderThread;

    //视频过滤器
    FFVFilter *vFilter;

    //视频过滤线程
    class FFVFilterThread *vFilterThread;

    //音频过滤器
    FFAFilter *aFilter;

    //音频滤镜线程
    class FFAFilterThread *aFilterThread;

    //UI窗口
    FFCapWindow *capWindow;
};

} // namespace FFRecordContextType

#endif // FFRECORDER_P_H
