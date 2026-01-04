#include "ffrecorder_p.h"

using namespace FFRecordContextType;
FFRecorderPrivate::FFRecorderPrivate()
{
    for (size_t i = 0; i < FFRecordContextType::A_DECODER_SIZE; ++i) {
        aDecoderThread[i] = nullptr;
        aDecoderPktQueue[i] = nullptr;
        aDecoderFrmQueue[i] = nullptr;
        aDecoder[i] = nullptr;
    }
    for (size_t i = 0; i < FFRecordContextType::V_DECODER_SIZE; ++i) {
        vDecoderThread[i] = nullptr;
        vDecoderPktQueue[i] = nullptr;
        vDecoderFrmQueue[i] = nullptr;
        vDecoder[i] = nullptr;
    }

    for (size_t i = 0; i < FFRecordContextType::A_DEMUXER_SIZE; ++i) {
        aDemuxerThread[i] = nullptr;
        aDemuxer[i] = nullptr;
    }

    for (size_t i = 0; i < FFRecordContextType::V_DEMUXER_SIZE; ++i) {
        vDemuxerThread[i] = nullptr;
        vDemuxer[i] = nullptr;
    }

    vEncoderPktQueue = nullptr;
    aEncoderPktQueue = nullptr;
    vFilterEncoderFrmQueue = nullptr;
    aFilterEncoderFrmQueue = nullptr;

    muxer = nullptr;
    muxerThread = nullptr;
    aEncoder = nullptr;
    aEncoderThread = nullptr;
    vEncoder = nullptr;
    vEncoderThread = nullptr;
    vFilter = nullptr;
    vFilterThread = nullptr;
    aFilter = nullptr;
    aFilterThread = nullptr;

    audioSampler = nullptr;
}

FFRecorderPrivate::~FFRecorderPrivate()
{
    vEncoderPktQueue = nullptr;
    aEncoderPktQueue = nullptr;
    vFilterEncoderFrmQueue = nullptr;
    aFilterEncoderFrmQueue = nullptr;

    muxer = nullptr;
    muxerThread = nullptr;
    aEncoder = nullptr;
    aEncoderThread = nullptr;
    vEncoder = nullptr;
    vEncoderThread = nullptr;
    vFilter = nullptr;
    vFilterThread = nullptr;
    aFilter = nullptr;
    aFilterThread = nullptr;

    audioSampler = nullptr;

    for (size_t i = 0; i < FFRecordContextType::A_DECODER_SIZE; ++i) {
        delete aDecoderThread[i];
        delete aDecoderPktQueue[i];
        delete aDecoderFrmQueue[i];
        delete aDecoder[i];
    }

    for (size_t i = 0; i < FFRecordContextType::V_DECODER_SIZE; ++i) {
        delete vDecoderThread[i];
        delete vDecoderPktQueue[i];
        delete vDecoderFrmQueue[i];
        delete vDecoder[i];
    }

    for (size_t i = 0; i < FFRecordContextType::A_DEMUXER_SIZE; ++i) {
        delete aDemuxerThread[i];
        delete aDemuxer[i];
    }

    for (size_t i = 0; i < FFRecordContextType::V_DEMUXER_SIZE; ++i) {
        delete vDemuxerThread[i];
        delete vDemuxer[i];
    }

    delete muxer;
    delete vEncoderPktQueue;
    delete aEncoderPktQueue;
    delete vFilterEncoderFrmQueue;
    delete aFilterEncoderFrmQueue;
}
