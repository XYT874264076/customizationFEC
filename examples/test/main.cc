#include <iostream>
#include <cstdio>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
// #include "third_party/ffmpeg/libavcodec/avcodec.h"
// #include "third_party/ffmpeg/libavformat/avformat.h"
// #include "third_party/ffmpeg/libswscale/swscale.h"

}

int main(int argc, char* argv[]){
  printf("Hello Tencent!\n");
  AVFormatContext* format_ctx_ = nullptr;
  AVCodecContext* codec_ctx_ = nullptr;
  int video_stream_index_ = -1;
  int ret = 0;
  const char* file_path="/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo.mp4";

  avformat_network_init();

  ret = avformat_open_input(&format_ctx_, file_path, nullptr, nullptr);

  if (!format_ctx_) {
    std::cout << "Failed to open file: " << file_path << std::endl;
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, err_buf, sizeof(err_buf));
    std::cerr << "Error opening file: " << err_buf << std::endl;
    std::cout << ret << std::endl;
    return -1;
  }

  av_dump_format(format_ctx_, 0, file_path, 0);

  printf("Before find_stream_info: num = %d, den = %d\n",
       format_ctx_->streams[0]->time_base.num,
       format_ctx_->streams[0]->time_base.den);

  if ((ret = avformat_find_stream_info(format_ctx_, nullptr)) < 0) {
    std::cout << "Failed to find stream info" << std::endl;
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, err_buf, sizeof(err_buf));
    return -1;
  }

  printf("=====================================\n");
  // new_av_dump_format(format_ctx_, 0, file_path, 0);

  printf("After find_stream_info: num = %d, den = %d\n",
       format_ctx_->streams[0]->time_base.num,
       format_ctx_->streams[0]->time_base.den);

  for (unsigned int i = 0; i < format_ctx_->nb_streams; ++i) {
    if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index_ = i;
      break;
    }
  }

  if (video_stream_index_ == -1) {
    std::cout << "No video stream found" << std::endl;
    return -1;
  }

  const AVCodec* codec = avcodec_find_decoder(format_ctx_->streams[video_stream_index_]->codecpar->codec_id);
  if (!codec) {
    std::cout << "Failed to find codec" <<std::endl;
    return -1;
  }

  codec_ctx_ = avcodec_alloc_context3(codec);
  if (!codec_ctx_) {
    std::cout << "Failed to allocate codec context" << std::endl;
    return -1;
  }

  if (avcodec_parameters_to_context(codec_ctx_, format_ctx_->streams[video_stream_index_]->codecpar) < 0) {
    std::cout << "Failed to copy codec parameters" << std::endl;
    return -1;
  }

  if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
    std::cout << "Failed to open codec" << std::endl;
    return -1;
  }

  std::cout<< video_stream_index_ << std::endl;
  std::cout<< format_ctx_->streams[0]->codecpar->codec_type <<std::endl;
  std::cout<< format_ctx_->streams[1]->codecpar->codec_type <<std::endl;

  std::cout<< format_ctx_->streams[video_stream_index_]->time_base.num<< std::endl;
  std::cout<< format_ctx_->streams[video_stream_index_]->time_base.den<< std::endl;
  std::cout<< "number of frames:"<<format_ctx_->streams[video_stream_index_]->nb_frames<<std::endl;
  std::cout<< "r_frame_rate.den:"<<format_ctx_->streams[video_stream_index_]->r_frame_rate.den<<std::endl;
  std::cout<< "r_frame_rate.num:"<<format_ctx_->streams[video_stream_index_]->r_frame_rate.num<<std::endl;

  AVFrame* frame_ = av_frame_alloc();
  AVPacket* packet_ = av_packet_alloc();
  SwsContext* sws_ctx_;

  std::cout<<&frame_<<std::endl;
  std::cout<<&packet_<<std::endl;
  std::cout<<&sws_ctx_<<std::endl;

  return 0;
}