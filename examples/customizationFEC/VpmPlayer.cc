
#include "examples/customizationFEC/VpmPlayer.h"

#include <stdint.h>

#include <memory>

//TODO:XZG
#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

VpmPlayer::VpmPlayer() : vpm_(nullptr) {}

bool VpmPlayer::Init(size_t width, size_t height, size_t target_fps, const char* file_path){
  width_ = width;
  height_ = height;

  vpm_ = webrtc::VideoPlayerFactory::Create(file_path);
  if (!vpm_) {
    return false;
  }
  vpm_->RegisterPlayerDataCallback(this);

  capability_.width = static_cast<int32_t>(width);
  capability_.height = static_cast<int32_t>(height);
  capability_.maxFPS = static_cast<int32_t>(target_fps);
  capability_.videoType = VideoType::kI420;

  if (vpm_->StartPlay(capability_) != 0) {
    Destroy();
    return false;
  }

  RTC_CHECK(vpm_->PlayStarted());

  return true;
}

VpmPlayer* VpmPlayer::Create(size_t width, size_t height, size_t target_fps, const char* file_path){
  std::unique_ptr<VpmPlayer> vpm_player(new VpmPlayer());
  if (!vpm_player->Init(width, height, target_fps, file_path)) {
    RTC_LOG(LS_WARNING) << "Failed to create VpmPlayer(w = " << width
                        << ", h = " << height << ", fps = " << target_fps << ")";
    return nullptr;
  }
  return vpm_player.release();
}

void VpmPlayer::Destroy(){
    if (!vpm_) return;

    vpm_->StopPlay();
    vpm_->DeRegisterPlayerDataCallback();
    // Release reference to VPM.
    vpm_ = nullptr;
}

VpmPlayer::~VpmPlayer() {
    Destroy();
}

void VpmPlayer::OnFrame(const VideoFrame& frame){
    MP4VideoPlayer::OnFrame(frame);
}

}
}
