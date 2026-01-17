#ifndef FFOPENSOURCEEVENT_H
#define FFOPENSOURCEEVENT_H

#include "ffevent.h"
#include <string>

class FFOpenSourceEvent : public FFEvent
{
public:
    // 构造函数：同时打开视频和音频源
    FFOpenSourceEvent(FFRecorder *recorderCtx,
                      enum FFRecordContextType::demuxerType videoSourceType_,
                      std::string const &videoUrl_,
                      enum FFRecordContextType::demuxerType audioSourceType_,
                      std::string const &audioUrl_,
                      std::string const &format_);

    virtual void work() override;
    virtual bool requiresSerialization() const override { return true; }

private:
    bool initVideo();
    bool initAudio();
    void startVideo();
    void startAudio();

    std::string videoUrl;
    std::string audioUrl;
    std::string format;

    int videoSourceType;
    int audioSourceType;
    int videoIndex;
    int audioIndex;
};

#endif // FFOPENSOURCEEVENT_H
