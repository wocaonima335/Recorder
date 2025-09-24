#include "ffopensourceevent.h"

using namespace FFRecordContextType;

FFOpenSourceEvent::FFOpenSourceEvent(FFRecorder *recorderCtx,
                                     demuxerType sourceType_,
                                     const string &url_,
                                     const string &format_)
    : FFEvent(recorderCtx)
{
    sourceType = sourceType_;
    url = url_;
    format = format_;
    index = demuxerIndex[sourceType];
}

void FFOpenSourceEvent::work()
{
    init();
    start();
}

void FFOpenSourceEvent::init()
{
    if (sourceType == AUDIO || sourceType == MICROPHONE)
    {
        aDemuxerThread[index]->close();
        aDemuxer[index]->init(url, format, aPktQueue[index], nullptr, sourceType);
        aDemuxerThread[index]->init(aDemuxer[index]);

        aDecoderThread[index]->close();
        aDecoder[index]->init(aDemuxer[index]->getAStream(), aFrmQueue[index]);
        aDecoderThread[index]->init(aDecoder[index], aPktQueue[index]);
    }
    else
    {
        vDemuxerThread[index]->close();
        vDemuxer[index]->init(url, format, nullptr, vPktQueue[index], sourceType);
        vDemuxerThread[index]->init(vDemuxer[index]);

        vDecoderThread[index]->close();
        vDecoder[index]->init(vDemuxer[index]->getVStream(), vFrmQueue[index]);
        vDecoderThread[index]->init(vDecoder[index], vPktQueue[index]);

        vEncoderThread->close();
        vEncoder->init(vEncoderPktQueue);
        vEncoderThread->init(vFilter, vEncoder, muxer, vFrmQueue[index]);

        muxer->init("E:/Videos/output.mp4");
        muxerThread
            ->init(aEncoderPktQueue, vEncoderPktQueue, muxer, aEncoder, vEncoder, recoderContext);
    }
    std::cout << "init opensource" << std::endl;
}

void FFOpenSourceEvent::start()
{
    if (sourceType == AUDIO || sourceType == MICROPHONE)
    {
        aDemuxerThread[index]->wakeAllThread();
        aDemuxerThread[index]->start();

        aDecoderThread[index]->wakeAllThread();
        aDecoderThread[index]->start();

        aPktQueue[index]->start();
        aFrmQueue[index]->start();

        aFilterThread->openAudioSource(sourceType);
    }
    else
    {
        vDemuxerThread[index]->wakeAllThread();
        vDemuxerThread[index]->start();

        vDecoderThread[index]->wakeAllThread();
        vDecoderThread[index]->start();

        // std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // vFilterThread->openVideoSource(sourceType);
        // vFilterThread->startEncoder();
        // vFilterThread->start();

        vEncoderThread->start();

        // std::this_thread::sleep_for(std::chrono::milliseconds(300));

        muxerThread->start();

        recoderContext->getVFilterEncoderFrmQueue()->start();
        recoderContext->getVEncoderPktQueue()->start();

        vPktQueue[index]->start();
        vFrmQueue[index]->start();
    }
    std::cout << "start opensource " << std::endl;
}
