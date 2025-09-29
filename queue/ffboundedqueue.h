#pragma once

#include "ffpacket.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <atomic>
#include <condition_variable>
#include <corecrt.h>
#include <mutex>
#include <queue>

using namespace std;

extern "C" {
#include <libavformat/avformat.h>
}

struct FFPacketTraits
{
    static FFPacket *allocateFromSrc(AVPacket *src)
    {
        if (!src)
            return nullptr;
        FFPacket *pkt = new FFPacket();
        av_packet_move_ref(&pkt->packet, src);
        pkt->serial = 0; // 可按需设置
        pkt->type = 0;   // NORMAL
        av_packet_unref(src);
        return pkt;
    }

    static void releaseSrc(AVPacket *src)
    {
        if (src)
            av_packet_unref(src);
    }

    static FFPacket *allocateNull()
    {
        FFPacket *pkt = new FFPacket();
        pkt->serial = 0;
        pkt->type = 2; // NULLP
        return pkt;
    }

    static void release(FFPacket *&pkt)
    {
        if (pkt) {
            av_packet_unref(&pkt->packet);
            delete pkt;
            pkt = nullptr;
        }
    }

    static bool isNull(const FFPacket *pkt)
    {
        return pkt && pkt->type == 2; // NULLP
    }
};

struct AVFrameTraits
{
    static AVFrame *allocateFromSrc(AVFrame *src)
    {
        if (!src)
            return nullptr;
        AVFrame *dest = av_frame_alloc();
        if (!dest)
            return nullptr;
        av_frame_move_ref(dest, src);
        av_frame_unref(src);
        return dest;
    }

    static void releaseSrc(AVFrame *src)
    {
        if (src)
            av_frame_unref(src);
    }

    static AVFrame *allocateNull()
    {
        AVFrame *f = av_frame_alloc();
        if (!f)
            return nullptr;
        f->data[0] = nullptr;
        f->data[1] = nullptr;
        f->data[2] = nullptr;
        return f;
    }

    static void release(AVFrame *&frame)
    {
        if (frame) {
            av_frame_unref(frame);
            av_frame_free(&frame);
        }
    }

    static bool isNull(const AVFrame *frame)
    {
        if (!frame)
            return false;
        return frame->data[0] == nullptr && frame->data[1] == nullptr && frame->data[2] == nullptr;
    }
};

template<typename T, typename Traits>

class FFBoundedQueue
{
public:
    explicit FFBoundedQueue(size_t maxSize = 3);

    void start();
    void wakeAllThread();
    void close();
    void enqueueFromSrc(T *src);
    void enqueueNull();
    T *dequeue();
    bool peekEmpty() const;
    AVFrame *peekQueue() const;
    AVFrame *peekBack() const;
    size_t length() const;
    static bool isNull(const T *item);
    void clearQueue();
    void flushQueue();

private:
    mutable std::mutex mutex;
    std::condition_variable cond;
    std::queue<T *> q;
    std::atomic<bool> m_stop;
    size_t m_maxSize;
};
