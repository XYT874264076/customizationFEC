
#ifndef EXAMPLES_MYFECEXP_RAW_VIDEO_SINK_PLAYER_INTERFACE_H_
#define EXAMPLES_MYFECEXP_RAW_VIDEO_SINK_PLAYER_INTERFACE_H_

#include "examples/MyFECExp/video_player_defines.h"

namespace webrtc{

class RawVideoSinkPlayerInterface {
 public:
  virtual ~RawVideoSinkPlayerInterface() = default;

  virtual int32_t OnRawFrame(uint8_t* videoFrame,
                             size_t videoFrameLength,
                             const webrtc::VideoPlayerCapability& frameInfo,
                             VideoRotation rotation,
                             int64_t captureTime) = 0;
};

}  // namespace webrtc

#endif  // EXAMPLES_MYFECEXP_RAW_VIDEO_SINK_PLAYER_INTERFACE_H_