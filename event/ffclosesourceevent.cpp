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

        // Stop demuxer thread and release resources
        if (aDemuxerThread[index]) {
            aDemuxerThread[index]->stop();
            aDemuxerThread[index]->wakeAllThread();
            aDemuxerThread[index]->wait();
            aDemuxerThread[index]->close();
        }

        // Stop decoder thread and release resources
        if (aDecoderThread[index]) {
            aDecoderThread[index]->stop();
            aDecoderThread[index]->wakeAllThread();
            aDecoderThread[index]->wait();
            aDecoderThread[index]->close();
        }

        // Close queues
        if (aPktQueue[index]) {
            aPktQueue[index]->close();
        }
        if (aFrmQueue[index]) {
            aFrmQueue[index]->close();
        }

        return;
    }

    // Close video sources (SCREEN / CAMERA / VIDEO)
    {
        // Stop muxer thread first to finalize trailer after consuming remaining packets
        if (muxerThread) {
            muxerThread->stop();
            muxerThread->wakeAllThread();
            muxerThread->wait();
            muxerThread->close();
        }

        // Stop encoder thread and release encoder resources
        if (vEncoderThread) {
            vEncoderThread->stop();
            vEncoderThread->wakeAllThread();
            vEncoderThread->wait();
            vEncoderThread->close();
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

        // Close queues (encoder, filter, decoder)
        if (recoderContext) {
            if (recoderContext->getVFilterEncoderFrmQueue()) {
                recoderContext->getVFilterEncoderFrmQueue()->close();
            }
            if (recoderContext->getVEncoderPktQueue()) {
                recoderContext->getVEncoderPktQueue()->close();
            }
        }

        if (vPktQueue[index]) {
            vPktQueue[index]->close();
        }
        if (vFrmQueue[index]) {
            vFrmQueue[index]->close();
        }
    }
}
