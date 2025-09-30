#ifndef FFAPACKETQUEUE_H
#define FFAPACKETQUEUE_H

#include "ffboundedqueue.h"

extern "C" {
#include <libavformat/avformat.h>
}

class FFPacket;

class FFAPacketQueue
{
public:
    explicit FFAPacketQueue();
    ~FFAPacketQueue();

    FFPacket *dequeue();
    FFPacket *peekQueue() const;
    FFPacket *peekBack() const;
    void enqueue(AVPacket *pkt);
    void enqueueFlush();
    void enqueueNull();
    void flushQueue();

    size_t getSerial();
    void clearQueue();
    void wakeAllThread();
    void close();
    void start();
    int length();

private:
    std::atomic<size_t> serial;
    FFBoundedQueue<FFPacket, FFPacketTraits> *impl;
};
#endif
