#include "ffvencoderthread.h"

#include "decoder/ffvdecoder.h"
#include "encoder/ffvencoder.h"
#include "filter/ffvfilter.h"
#include "muxer/ffmuxer.h"
#include "queue/ffvframequeue.h"

#include <iostream>

FFVEncoderThread::FFVEncoderThread() {}

FFVEncoderThread::~FFVEncoderThread() {}

void FFVEncoderThread::init(FFVFilter *vFilter_,
                            FFVEncoder *vEncoder_,
                            FFMuxer *muxer_,
                            FFVFrameQueue *frmQueue_)
{
    vFilter = vFilter_;
    vEncoder = vEncoder_;
    muxer = muxer_;
    frmQueue = frmQueue_;
    std::cerr << "[VEncThread] init: vFilter=" << vFilter << " vEncoder=" << vEncoder
              << " muxer=" << muxer << " frmQueue=" << frmQueue << std::endl;
}

void FFVEncoderThread::wakeAllThread()
{
    if (frmQueue)
    {
        frmQueue->wakeAllThread();
    }
    if (vEncoder)
    {
        vEncoder->wakeAllThread();
    }
}

void FFVEncoderThread::close()
{
    if (vEncoder)
    {
        vEncoder->close();
    }
    firstFrame = true;
    firstFramePts = 0;
    streamIndex = -1;
    std::cerr << "[VEncThread] close: reset firstFrame and streamIndex" << std::endl;
}

void FFVEncoderThread::onPauseChanged(bool pausedFlag, int64_t ts_us)
{
    std::unique_lock<std::mutex> lk(pause_mutex);
    if (pausedFlag) {
        // 收到 paused=true
        pause_start_us = ts_us > 0 ? ts_us : av_gettime_relative();
        paused.store(true, std::memory_order_release);
        pause_cv.notify_all();
        std::cerr << "[VEncThread] paused at " << pause_start_us << "us" << std::endl;
    } else {
        // 收到 paused=false
        int64_t now = ts_us > 0 ? ts_us : av_gettime_relative();
        pause_accum_us += (now - pause_start_us);
        paused.store(false, std::memory_order_release);
        first_after_resume = true; // 恢复后首帧 I 帧
        pause_cv.notify_all();
        std::cerr << "[VEncThread] resumed at " << now << "us, total_pause=" << pause_accum_us
                  << "us" << std::endl;
    }
}

void FFVEncoderThread::run()
{
    int64_t frame_count = 0;

    while (!m_stop) {
        AVFrame *frame = frmQueue->dequeue();
        if (!frame)
            break;

        if (streamIndex == -1)
            initEncoder(frame);

        // 如果暂停：软暂停（丢帧+短暂等待），避免队列爆涨；如需硬暂停，可阻塞等待
        if (paused.load(std::memory_order_acquire)) {
            // 软暂停：直接释放帧并等待，避免忙等
            AVFrameTraits::release(frame);
            std::unique_lock<std::mutex> lk(pause_mutex);
            pause_cv.wait_for(lk, std::chrono::milliseconds(10), [this] {
                return !paused.load(std::memory_order_acquire) || m_stop;
            });
            continue;
        }

        // 恢复后的首帧请求关键帧（如果你在 filter 前）
        if (first_after_resume) {
            frame->pict_type = AV_PICTURE_TYPE_I;
            first_after_resume = false;
        }

        // 计算校正后的墙钟 PTS（扣除暂停累计时长）
        int64_t now_us = av_gettime_relative();
        int64_t wall_us = now_us - start_time_us - pause_accum_us;
        AVRational src_tb = {1, AV_TIME_BASE};
        int64_t wall_pts = av_rescale_q(wall_us, src_tb, timeBase);

        // 理想帧计数 PTS（单调保障）
        int64_t ideal_pts = frame_count;

        // 混合策略：以墙钟为主，确保单调
        int64_t vpts = std::max(wall_pts, ideal_pts);
        frame_count = vpts + 1;

        vEncoder->encode(frame, streamIndex, vpts, timeBase);
        AVFrameTraits::release(frame);
    }
}

// void FFVEncoderThread::run()
// {
//     int64_t frame_count = 0;
//     // double total_encode_time = 0.0;
//     // double total_dequeue_time = 0.0;

//     while (!m_stop)
//     {
//         // 监控dequeue时间
//         // auto dequeue_start = std::chrono::high_resolution_clock::now();
//         AVFrame *frame = frmQueue->dequeue();
//         // auto dequeue_end = std::chrono::high_resolution_clock::now();

//         if (!frame)
//             break;

//         // double dequeue_ms = std::chrono::duration<double, std::milli>(dequeue_end - dequeue_start)
//         //                         .count();
//         // total_dequeue_time += dequeue_ms;

//         if (streamIndex == -1)
//             initEncoder(frame);

//         // 1. 计算墙钟PTS
//         int64_t now_us = av_gettime_relative();
//         int64_t wall_us = now_us - start_time_us;
//         AVRational src_tb = {1, AV_TIME_BASE};
//         int64_t wall_pts = av_rescale_q(wall_us, src_tb, timeBase);

//         // 2. 计算理想帧计数PTS
//         int64_t ideal_pts = frame_count;

//         // 3. 混合策略：以墙钟为准，但确保单调递增
//         int64_t vpts = std::max(wall_pts, ideal_pts);

//         // 4. 更新帧计数为实际使用的PTS
//         frame_count = vpts + 1;

//         // 监控编码时间
//         // auto encode_start = std::chrono::high_resolution_clock::now();
//         vEncoder->encode(frame, streamIndex, vpts, timeBase);
//         // auto encode_end = std::chrono::high_resolution_clock::now();

//         // double encode_ms = std::chrono::duration<double, std::milli>(encode_end - encode_start)
//         //                        .count();
//         // total_encode_time += encode_ms;

//         // // 警告慢编码
//         // if (encode_ms > 50.0)
//         // {
//         //     std::cerr << "[VEncThread] WARNING: Slow encode detected! " << encode_ms
//         //               << "ms (target: <33ms for 30fps)" << std::endl;
//         // }

//         // // 警告长时间dequeue
//         // if (dequeue_ms > 50.0)
//         // {
//         //     std::cerr << "[VEncThread] WARNING: Long dequeue wait! " << dequeue_ms
//         //               << "ms (queue may be empty)" << std::endl;
//         // }

//         AVFrameTraits::release(frame);
//     }

//     // if (frame_count > 0)
//     // {
//     //     double avg_encode = total_encode_time / frame_count;
//     //     double avg_dequeue = total_dequeue_time / frame_count;
//     //     std::cerr << "[VEncThread] Final stats: " << frame_count << " frames processed"
//     //               << ", avg_encode=" << avg_encode << "ms"
//     //               << ", avg_dequeue=" << avg_dequeue << "ms"
//     //               << ", actual_fps=" << (frame_count * 1000.0 / total_encode_time) << std::endl;
//     // }
// }

void FFVEncoderThread::initEncoder(AVFrame *frame)
{
    frameRate = vFilter->getFrameRate();

    vEncoder->initVideo(frame, frameRate);
    timeBase = vEncoder->getCodecCtx()->time_base;

    muxer->addStream(vEncoder->getCodecCtx());
    streamIndex = muxer->getVStreamIndex();
}
