#include "ffclosesourceevent.h"

FFCloseSourceEvent::FFCloseSourceEvent(FFRecorder *recordCtx, demuxerType sourceType_)
    : FFEvent(recordCtx)
{
    index = FFRecordContextType::demuxerIndex[sourceType_];
    sourceType = sourceType_;
}

void FFCloseSourceEvent::work()
{
    close();
}

void FFCloseSourceEvent::close()
{
    using namespace FFRecordContextType;

    // Close audio sources
    if (sourceType == AUDIO || sourceType == MICROPHONE) {
        // Reset filter thread flags for the specific audio source
        if (aFilterThread) {
            aFilterThread->closeAudioSource(sourceType);
        }

        if (aEncoderThread) {
            aEncoderThread->stop();
            aEncoderThread->wakeAllThread();
            aEncoderThread->wait();
            aEncoderThread->close();
        }

        // Stop decoder thread and release resources
        if (aDecoderThread[index]) {
            aDecoderThread[index]->stop();
            aDecoderThread[index]->wakeAllThread();
            aDecoderThread[index]->wait();
            aDecoderThread[index]->close();
        }

        // Stop demuxer thread and release resources
        if (aDemuxerThread[index]) {
            aDemuxerThread[index]->stop();
            aDemuxerThread[index]->wakeAllThread();
            aDemuxerThread[index]->wait();
            aDemuxerThread[index]->close();
        }

        // Clear queue residual data
        if (aPktQueue[index]) {
            aPktQueue[index]->clearQueue();
        }
        if (aFrmQueue[index]) {
            aFrmQueue[index]->clearQueue();
        }

    } else {
        // Stop video encoder thread
        if (vEncoderThread) {
            vEncoderThread->stop();
            vEncoderThread->wakeAllThread();
            vEncoderThread->wait();
            vEncoderThread->close();
        }

        // Stop muxer thread to finalize trailer after consuming remaining packets
        if (muxerThread) {
            muxerThread->stop();
            muxerThread->wakeAllThread();
            muxerThread->wait();
            muxerThread->close();
        }

        // Stop decoder thread
        if (vDecoderThread[index]) {
            vDecoderThread[index]->stop();
            vDecoderThread[index]->wakeAllThread();
            vDecoderThread[index]->wait();
            vDecoderThread[index]->close();
        }

        // Stop demuxer thread
        if (vDemuxerThread[index]) {
            vDemuxerThread[index]->stop();
            vDemuxerThread[index]->wakeAllThread();
            vDemuxerThread[index]->wait();
            vDemuxerThread[index]->close();
        }

        // Clear queue residual data
        if (vPktQueue[index]) {
            vPktQueue[index]->clearQueue();
        }
        if (vFrmQueue[index]) {
            vFrmQueue[index]->clearQueue();
        }
    }
    return;
}
