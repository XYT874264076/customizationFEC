
#ifndef EXAMPLES_MYFECEXP_MP4VIDEOTRACKSOURCE_H_
#define EXAMPLES_MYFECEXP_MP4VIDEOTRACKSOURCE_H_

#include "third_party/ffmpeg/libavcodec/avcodec.h"
#include "third_party/ffmpeg/libavformat/avformat.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"


class FFmpegVideoDecoder : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
    public:
        FFmpegVideoDecoder(const std::string& file_path);
        ~FFmpegVideoDecoder();
        
        bool GetNextFrame(webrtc::VideoFrame* video_frame);
    private:
        void FillWebRTCFrame(AVFrame* frame, webrtc::VideoFrame& video_frame);

        AVFormatContext* format_context_;
        AVCodecContext* codec_context_;
        int video_stream_index_;
};

#endif  //EXAMPLES_MYFECEXP_MP4VIDEOTRACKSOURCE_H_


