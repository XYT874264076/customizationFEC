
#ifndef TEST_VPM_PLAYER_H_
#define TEST_VPM_PLAYER_H_

#include <memory>
#include <vector>

#include "api/scoped_refptr.h"

#include "examples/customizationFEC/video_player.h"
#include "rtc_base/logging.h"
#include "examples/customizationFEC/MP4VideoPlayer.h"

namespace webrtc {
namespace test {

class VpmPlayer : public MP4VideoPlayer,
                  public rtc::VideoSinkInterface<VideoFrame> {
 public:
  static VpmPlayer* Create(size_t width, size_t height, size_t target_fps, const char* file_path);
  virtual ~VpmPlayer();

  void Start() override {
    RTC_LOG(LS_WARNING) << "Capturer doesn't support resume/pause and always "
                           "produces the video";
  }
  void Stop() override {
    RTC_LOG(LS_WARNING) << "Capturer doesn't support resume/pause and always "
                           "produces the video";
  }

  void OnFrame(const VideoFrame& frame) override;

  int GetFrameWidth() const override { return static_cast<int>(width_); }
  int GetFrameHeight() const override { return static_cast<int>(height_); }

 private:
  VpmPlayer();
  bool Init(size_t width, size_t height, size_t target_fps, const char* file_path);
  void Destroy();

  size_t width_;
  size_t height_;
  rtc::scoped_refptr<VideoPlayerModule> vpm_;
  VideoPlayerCapability capability_;
};

}
}

#endif  // TEST_VPM_PLAYER_H_