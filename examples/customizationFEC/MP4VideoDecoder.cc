
#include "examples/customizationFEC/MP4VideoDecoder.h"

#include <iostream>

#include "rtc_base/logging.h"
#include "api/video/i420_buffer.h"
#include "examples/customizationFEC/Params.h"

namespace webrtc{
namespace videoplayermodule{

MP4VideoDecoder::MP4VideoDecoder()
    : quit_(false),
      _playstarted(false),
      format_ctx_(nullptr),
      codec_ctx_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      sws_ctx_(nullptr),
      video_stream_index_(-1),
      start_time_initialize (false) {}

MP4VideoDecoder::~MP4VideoDecoder() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  StopPlay();
}

int32_t MP4VideoDecoder::OpenVideoStream(const char* file_path) { 
  RTC_DCHECK_RUN_ON(&api_checker_);

  avformat_open_input(&format_ctx_, file_path, nullptr, nullptr);
  if (!format_ctx_) {
    RTC_LOG(LS_ERROR) << "Failed to open file: " << file_path;
    return -1;
  }

  if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to find stream info";
    return -1;
  }

  for (unsigned int i = 0; i < format_ctx_->nb_streams; ++i) {
    if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index_ = i;
      break;
    }
  }

  if (video_stream_index_ == -1) {
    RTC_LOG(LS_ERROR) << "No video stream found";
    return -1;
  }

  const AVCodec* codec = avcodec_find_decoder(format_ctx_->streams[video_stream_index_]->codecpar->codec_id);
  if (!codec) {
    RTC_LOG(LS_ERROR) << "Failed to find codec";
    return -1;
  }

  codec_ctx_ = avcodec_alloc_context3(codec);
  if (!codec_ctx_) {
    RTC_LOG(LS_ERROR) << "Failed to allocate codec context";
    return -1;
  }

  if (avcodec_parameters_to_context(codec_ctx_, format_ctx_->streams[video_stream_index_]->codecpar) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to copy codec parameters";
    return -1;
  }

  if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to open codec";
    return -1;
  }

  frame_ = av_frame_alloc();
  packet_ = av_packet_alloc();

  return 0;
}

int32_t MP4VideoDecoder::CloseVideoStream() {
  if (format_ctx_) avformat_close_input(&format_ctx_);
  if (codec_ctx_) avcodec_free_context(&codec_ctx_);
  if (frame_) av_frame_free(&frame_);
  if (packet_) av_packet_free(&packet_);
  if (sws_ctx_) sws_freeContext(sws_ctx_);
  return 0;
}

int32_t MP4VideoDecoder::Init(const char* file_path) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  MutexLock lock(&capture_lock_);

  file_path_ = file_path;
  format_ctx_ = nullptr;
  quit_ = false;
  _playstarted = false;
  format_ctx_ = nullptr;
  codec_ctx_ = nullptr;
  frame_ = nullptr;
  packet_ = nullptr;
  sws_ctx_ = nullptr;
  video_stream_index_ = -1;
  start_time_initialize  = false;

  if (OpenVideoStream(file_path_)==-1) return -1;
  
  return 0;
}

int32_t MP4VideoDecoder::StartPlay(const VideoPlayerCapability& capability) {
  RTC_DCHECK_RUN_ON(&api_checker_);

  if (_playstarted) {
    if (capability == _requestedCapability){
        return 0;
    } else {
        StopPlay();
    }
    return 0;
  }

  // We don't want members above to be guarded by capture_checker_ as
  // it's meant to be for members that are accessed on the API thread
  // only when we are not capturing. The code above can be called many
  // times while sharing instance of VideoCaptureV4L2 between websites
  // and therefore it would not follow the requirements of this checker.
  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  // Set a baseline of configured parameters. It is updated here during
  // configuration, then read from the capture thread.
  configured_capability_ = capability;

  MutexLock lock(&capture_lock_);

  _requestedCapability = capability;
  _playstarted = true;
  _streaming = true;

  if (_playThread.empty()){
    quit_ = false;
    _playThread = rtc::PlatformThread::SpawnJoinable(
        [this] {
            while (PlayProcess()){              
            }
        },
        "MP4PlayThread",
        rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh)
    );
  }

  return 0;
}

int32_t MP4VideoDecoder::StopPlay() {
  RTC_DCHECK_RUN_ON(&api_checker_);

  if (!_playThread.empty()){
    {
      MutexLock lock(&capture_lock_);
      quit_ = true;
    }
  // Make sure the play thread stops using the mutex.
    _playThread.Finalize();
  }

  _playstarted = false;
  start_time_initialize = false;

  RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);
  MutexLock lock(&capture_lock_);

  if (_streaming) {
    _streaming = false;
    _requestedCapability = configured_capability_ = VideoPlayerCapability();
  }

  CloseVideoStream();

  return 0;
}

bool MP4VideoDecoder::PlayStarted() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  return _playstarted;
}

int32_t MP4VideoDecoder::PlaySettings(VideoPlayerCapability& settings){
  RTC_DCHECK_RUN_ON(&api_checker_);
  settings = _requestedCapability;

  return 0;
}

bool MP4VideoDecoder::PlayProcess() {
  // RTC_CHECK_RUNS_SERIALIZED(&capture_checker_);

  {
    MutexLock lock(&capture_lock_);

    if (quit_) {
      return false;
    }

    if (av_read_frame(format_ctx_, packet_) >= 0) {
        if (packet_->stream_index == video_stream_index_) {
            if (avcodec_send_packet(codec_ctx_, packet_) == 0) {
                while (avcodec_receive_frame(codec_ctx_, frame_) == 0) {
                    rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(configured_capability_.width, configured_capability_.height);
                    // Convert YUV to I420

                    sws_ctx_ = sws_getCachedContext(
                        sws_ctx_, frame_->width, frame_->height, codec_ctx_->pix_fmt,
                        configured_capability_.width, configured_capability_.height, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);

                    uint8_t* dest[3] = {buffer->MutableDataY(), buffer->MutableDataU(), buffer->MutableDataV()};
                    int dest_stride[3] = {buffer->StrideY(), buffer->StrideU(), buffer->StrideV()};

                    sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0, frame_->height, dest, dest_stride);

                    auto now = std::chrono::steady_clock::now();
                    if (!start_time_initialize){
                      start_time = now;
                      start_time_initialize=true;
                    }
                    double elapsed_time = std::chrono::duration<double>(now-start_time).count();
                    double frame_time = frame_->pts * av_q2d(format_ctx_->streams[video_stream_index_]->time_base);

                    if (elapsed_time < frame_time){
                      std::this_thread::sleep_for(std::chrono::duration<double>(frame_time-elapsed_time));
                    }

                    // IncomingFrame(buffer->MutableDataY(), frame_->linesize[0] * frame_->height, configured_capability_);
                    IncomingFrame(buffer->MutableDataY(), configured_capability_.width * configured_capability_.height*2, configured_capability_);
                }
            }
        }
        av_packet_unref(packet_);
    } else {
      return false;  // EOF
    }
  }

  return true;
}

}  // namespace videoplayermodule
}  // namespace webrtc