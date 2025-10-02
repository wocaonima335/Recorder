#include "ffafilterthread.h"

#include "filter/ffafilter.h"
#include "queue/ffaframequeue.h"
#include "recorder/ffrecorder.h"

#include <iostream>

using namespace FFRecordContextType;

FFAFilterThread::FFAFilterThread()
{
    encoderFlag.store(false);
    audioFlag.store(false);
    microphoneFlag.store(false);
    pauseFlag.store(false);

    pauseTime = 0;
    lastPauseTime = 0;
}

FFAFilterThread::~FFAFilterThread()
{
    if (sysFrame) {
        av_frame_free(&sysFrame);
    }
    if (micFrame) {
        av_frame_free(&micFrame);
    }
}

void FFAFilterThread::openAudioSource(int audioType)
{
    cond.notify_all();
    enum demuxerType type = static_cast<demuxerType>(audioType);
    if (type == AUDIO) {
        audioFlag.store(true, std::memory_order_seq_cst);
    } else if (type == MICROPHONE) {
        microphoneFlag.store(true, std::memory_order_seq_cst);
    }
}

void FFAFilterThread::closeAudioSource(int audioType)
{
    cond.notify_all();
    enum demuxerType type = static_cast<demuxerType>(audioType);
    if (type == AUDIO) {
        audioFlag.store(false, std::memory_order_seq_cst);
    } else if (type == MICROPHONE) {
        microphoneFlag.store(false, std::memory_order_seq_cst);
    }
}

void FFAFilterThread::init(FFAFrameQueue *micFrmQueue_,
                           FFAFrameQueue *sysFrmQueue_,
                           FFAFilter *filter_)
{
    micFrmQueue = micFrmQueue_;
    sysFrmQueue = sysFrmQueue_;
    filter = filter_;
}

void FFAFilterThread::startEncoder()
{
    cond.notify_all();
    encoderFlag.store(true, std::memory_order_seq_cst);
}

void FFAFilterThread::stopEncoder()
{
    cond.notify_all();
    encoderFlag.store(false, std::memory_order_seq_cst);
    pauseFlag.store(false);
    pauseTime = 0;
    lastPauseTime = 0;
}

void FFAFilterThread::pauseEncoder()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (pauseFlag.load()) {
        pauseTime += av_gettime_relative() - lastPauseTime;
        pauseFlag.store(false);
    } else {
        pauseFlag.store(true);
        lastPauseTime = av_gettime_relative();
    }
}

void FFAFilterThread::setAudioVolume(double value)
{
    if (filter) {
        if (audioFlag.load() && microphoneFlag.load()) {
            filter->setAudioVolume(value, -1);
        } else {
            filter->setSystemAudioVolume(value);
        }
    }
}

void FFAFilterThread::setMicrophoneVolume(double value)
{
    if (filter) {
        if (audioFlag.load() && microphoneFlag.load()) {
            filter->setAudioVolume(-1, value);
        } else {
            filter->setMicrophoneAudioVolume(value);
        }
    }
}

bool FFAFilterThread::peekStart()
{
    return encoderFlag.load();
}

void FFAFilterThread::wakeAllThread()
{
    if (sysFrmQueue) {
        sysFrmQueue->wakeAllThread();
    }
    if (sysFrmQueue) {
        sysFrmQueue->wakeAllThread();
    }
}

void FFAFilterThread::run()
{
    while (!m_stop) {
        bool microphoneActive = microphoneFlag.load(std::memory_order_seq_cst);
        bool audioActive = audioFlag.load(std::memory_order_seq_cst);
        bool encoderActive = encoderFlag.load(std::memory_order_seq_cst);
        bool pauseActive = pauseFlag.load(std::memory_order_seq_cst);

        if (!microphoneActive && !audioActive && !encoderActive) {
            std::unique_lock<std::mutex> lock(mutex);
            cond.wait_for(lock, std::chrono::milliseconds(100));
            continue; // 重新读取所有标志
        }

        int64_t start = av_gettime_relative() * 10;

        if (audioActive) {
            sysFrame = sysFrmQueue->dequeue();
            if (encoderActive && !pauseActive) {
                filter->processSingleAudioFrame(sysFrame,
                                                start,
                                                pauseTime,
                                                AudioSourceType::AUDIO_SOURCE_SYSTEM);
            } else {
                if (sysFrame) {
                    av_frame_unref(sysFrame);
                    av_frame_free(&sysFrame);
                }
            }
        } else if (microphoneActive) {
            micFrame = micFrmQueue->dequeue();
            if (encoderActive && !pauseActive) {
                filter->processSingleAudioFrame(micFrame,
                                                start,
                                                pauseTime,
                                                AudioSourceType::AUDIO_SOURCE_MICROPHONE);
            } else {
                if (micFrame) {
                    av_frame_unref(micFrame);
                    av_frame_free(&micFrame);
                }
            }
        } else {
            if (encoderActive && !pauseActive) {
                AVFrame *muteFrame = generateMuteFrame();
                filter->forwardAudioFrame(muteFrame, start, pauseTime);
            }
        }
    }
}

AVFrame *FFAFilterThread::generateMuteFrame()
{
    AVFrame *frame = av_frame_alloc();

    frame->format = AUDIO_SAMPLE_RATE;
    frame->sample_rate = AUDIO_SAMPLE_RATE;
    frame->nb_samples = 1024;
    av_channel_layout_default(&frame->ch_layout, 2);

    int ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        std::cerr << "get mute Frame buffer error" << std::endl;
        av_frame_free(&frame);
        return nullptr;
    }

    ret = av_samples_set_silence(frame->extended_data,
                                 0,
                                 frame->nb_samples,
                                 frame->ch_layout.nb_channels,
                                 AUDIO_SAMPLE_FORMAT);

    if (ret < 0) {
        std::cerr << "Set Samples Silence Fail !" << std::endl;
        av_frame_free(&frame);
        return nullptr;
    }

    return frame;
}
