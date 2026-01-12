
#ifndef MODULE_MP4_VIDEO_DECODER_H_
#define MODULE_MP4_VIDEO_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <chrono>
#include <thread>

#include "examples/MyFECExp/video_player_defines.h"
#include "examples/MyFECExp/video_player_impl.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"

extern "C" {
// #include "third_party/ffmpeg/libavcodec/avcodec.h"
// #include "third_party/ffmpeg/libavformat/avformat.h"
// #include "third_party/ffmpeg/libswscale/swscale.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

namespace webrtc{
namespace videoplayermodule{

class MP4VideoDecoder : public VideoPlayerImpl {
 public:
  MP4VideoDecoder();
  ~MP4VideoDecoder() override;

  int32_t Init(const char* file_path);
  int32_t StartPlay(const VideoPlayerCapability& capability) override;
  int32_t StopPlay() override;
  bool PlayStarted() override;
  int32_t PlaySettings(VideoPlayerCapability& settings) override;

 private:
  static void PlayThread(void* obj);
  bool PlayProcess();
  int32_t OpenVideoStream(const char* file_path);
  int32_t CloseVideoStream();

  rtc::PlatformThread _playThread RTC_GUARDED_BY(api_checker_);
  Mutex capture_lock_ RTC_ACQUIRED_BEFORE(api_lock_);
  bool quit_ RTC_GUARDED_BY(capture_lock_);

  VideoPlayerCapability configured_capability_; 
  bool _streaming;
  bool _playstarted RTC_GUARDED_BY(api_checker_);

  const char* file_path_;

  AVFormatContext* format_ctx_;
  AVCodecContext* codec_ctx_;
  AVFrame* frame_;
  AVPacket* packet_;
  SwsContext* sws_ctx_;

  int video_stream_index_;
  std::chrono::steady_clock::time_point start_time;
  bool start_time_initialize;
};

}  // namespace videoplayermodule
}  // namespace webrtc



#endif  // MODULE_MP4_VIDEO_DECODER_H_