#include "ffvpacketqueue.h"

FFVPacketQueue::FFVPacketQueue()
    : m_stop(false)
{}

FFVPacketQueue::~FFVPacketQueue()
{
    close();
}

void FFVPacketQueue::enqueue(AVPacket *pkt) {}
