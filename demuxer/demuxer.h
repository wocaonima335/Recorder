#ifndef DEMUXER_H
#define DEMUXER_H

#include <QObject>
#include <QString>
#include <mutex>

using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class FFVPacketQueue;
class FFAPacketQueue;

class Demuxer
{
public:
    explicit Demuxer();
    Demuxer(const Demuxer &) = delete;
    Demuxer &operator=(const Demuxer &) = delete;
    ~Demuxer();

    void init(string const &url,
              string const &format_,
              FFAPacketQueue *aPktQueue_,
              FFVPacketQueue *vPktQueue_,
              int type);
    int demux();
    AVStream *getAStream();
    AVStream *getVStream();
    void wakeAllThread();
    void close();
    void initDemuxer();
    int getType();

private:
    void printError(int ret);

private:
    string url;
    string format;

    AVFormatContext *fmtCtx = nullptr;
    AVDictionary *opts = nullptr;
    AVStream *aStream = nullptr;
    AVStream *vStream = nullptr;

    FFVPacketQueue *vPktQueue = nullptr;
    FFAPacketQueue *aPktQueue = nullptr;

    AVRational aTimeBase;
    AVRational vTimeBase;

    const AVInputFormat *inputFmt = nullptr;
    int type;
    bool offsetFlag = false;

    std::mutex mutex;
    bool stopFlag = false;
};

#endif // DEMUXER_H
