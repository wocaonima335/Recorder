#ifndef FFDEMUXERTHREAD_H
#define FFDEMUXERTHREAD_H

#include "ffthread.h"

#include <condition_variable>
#include <mutex>

class Demuxer;
class FFAPacketQueue;
class FFVPacketQueue;
class FFPlayerContext;

class FFDemuxerThread : public FFThread
{
public:
    FFDemuxerThread();
    virtual ~FFDemuxerThread() override;

    void init(Demuxer *demuxer_);
    void wakeAllThread();

    void close();

    bool peekStop();

protected:
    virtual void run() override;

private:
    Demuxer *demuxer = nullptr;
    FFPlayerContext *playerCtx = nullptr;
    std::condition_variable cond;
    std::mutex mutex;
    std::atomic<bool> stopFlag;
};

#endif // FFDEMUXERTHREAD_H
