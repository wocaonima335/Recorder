#ifndef FFVPACKETQUEUE_H
#define FFVPACKETQUEUE_H

#include "ffboundedqueue.h"

extern "C" {
#include <libavformat/avformat.h>
}

class FFPacket;

class FFVPacketQueue
{
public:
    explicit FFVPacketQueue();
    ~FFVPacketQueue();

    FFPacket *dequeue();
    FFPacket *peekQueue() const;
    FFPacket *peekBack() const;
    void enqueue(AVPacket *pkt);
    void enqueueFlush();
    void enqueueNull();
    void flushQueue();

    size_t getSerial();

    void wakeAllThread();
    void clearQueue();
    void close();
    void start();
    int length();

private:
    std::atomic<size_t> serial;
    FFBoundedQueue<FFPacket, FFPacketTraits> *impl;
};

#endif
