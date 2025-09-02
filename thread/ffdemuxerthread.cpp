#include "ffdemuxerthread.h"
#include "demuxer/demuxer.h"

FFDemuxerThread::FFDemuxerThread()
{
    stopFlag.store(true);
}

FFDemuxerThread::~FFDemuxerThread()
{
    if (demuxer) {
        delete demuxer;
        demuxer = nullptr;
    }
}

void FFDemuxerThread::init(Demuxer *demuxer_)
{
    demuxer = demuxer_;
}

void FFDemuxerThread::wakeAllThread()
{
    if (demuxer) {
        demuxer->wakeAllThread();
    }
    cond.notify_all();
}

void FFDemuxerThread::close()
{
    if (demuxer) {
        demuxer->close();
    }

    cond.notify_all();
    stopFlag.store(true, std::memory_order_release);
}

bool FFDemuxerThread::peekStop()
{
    return m_stop.load(std::memory_order_release);
}

void FFDemuxerThread::run()
{
    while (!m_stop) {
        stopFlag.store(false, std::memory_order_release);
        int ret = demuxer->demux();
        if (ret != 0) {
            m_stop = true;
        }

        if (m_stop) {
            break;
        }
    }
}
