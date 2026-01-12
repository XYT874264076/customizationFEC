#include "examples/customizationFEC/mp4_video_track_source.h"

#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"

//
// FFmpegVideoDecoder Defination.
//

FFmpegVideoDecoder::FFmpegVideoDecoder(const std::string& file_path)
    : format_context_(nullptr), codec_context_(nullptr), video_stream_index_(-1) {
    avformat_open_input(&format_context_, file_path.c_str(), nullptr, nullptr);
    avformat_find_stream_info(format_context_, nullptr);

    // 查找视频流
    for (int i = 0; i < format_context_->nb_streams; ++i) {
        if (format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    // 获取解码器
    codec_context_ = avcodec_alloc_context3(nullptr);
    avcodec_parameters_to_context(codec_context_, format_context_->streams[video_stream_index_]->codecpar);
    avcodec_open2(codec_context_, avcodec_find_decoder(codec_context_->codec_id), nullptr);
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
    if (codec_context_) {
        avcodec_free_context(&codec_context_);
    }
    if (format_context_) {
        avformat_close_input(&format_context_);
    }
}

// 从文件中获取下一帧视频并将其传递给 WebRTC
bool FFmpegVideoDecoder::GetNextFrame(webrtc::VideoFrame* video_frame) {
    AVPacket packet;
    AVFrame* frame = av_frame_alloc();

    // 读取下一帧
    if (av_read_frame(format_context_, &packet) < 0) {
        return false;  // 读取失败，文件结束
    }

    if (packet.stream_index == video_stream_index_) {
        // 解码视频帧
        avcodec_send_packet(codec_context_, &packet);
        if (avcodec_receive_frame(codec_context_, frame) == 0) {
            // 将解码后的视频帧转为 WebRTC 格式
            // 假设视频是 YUV 格式
            // 创建 WebRTC VideoFrameBuffer
            video_frame->set_video_frame_buffer(I420Buffer::Create(frame->width, frame->height));  
            video_frame->set_timestamp(frame->pts);
            video_frame->set_rotation(webrtc::kVideoRotation_0);

            // 这里需要手动将帧数据填充到 WebRTC 格式的帧中
            // 例如，YUV 格式的填充
            FillWebRTCFrame(frame, *video_frame);

            return true;
        }
    }

    return false;
}




