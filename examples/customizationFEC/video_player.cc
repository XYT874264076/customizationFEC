
#include "examples/customizationFEC/video_player.h"

#include "examples/customizationFEC/video_player_impl.h"

namespace webrtc{

rtc::scoped_refptr<VideoPlayerModule> VideoPlayerFactory::Create(const char* file_path) {
  return videoplayermodule::VideoPlayerImpl::Create(file_path);
}

}  // namespace webrtc