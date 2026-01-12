
#ifndef MODULES_VIDEO_PLAYER_CONFIG_H_
#define MODULES_VIDEO_PLAYER_CONFIG_H_

namespace webrtc {
namespace videoplayermodule {
enum { kDefaultWidth = 640 };     // Start width
enum { kDefaultHeight = 480 };    // Start heigt
enum { kDefaultFrameRate = 30 };  // Start frame rate

enum { kMaxFrameRate = 60 };  // Max allowed frame rate of the start image

enum { kDefaultCaptureDelay = 120 };
enum {
  kMaxCaptureDelay = 270
};  // Max capture delay allowed in the precompiled capture delay values.

enum { kFrameRateCallbackInterval = 1000 };
enum { kFrameRateCountHistorySize = 90 };
enum { kFrameRateHistoryWindowMs = 2000 };
}  // namespace videoplayermodule
}  // namespace webrtc


#endif  // MODULES_VIDEO_PLAYER_CONFIG_H_