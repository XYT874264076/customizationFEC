
#ifndef MODULES_VIDEO_PLAYER_IMPL_H_
#define MODULES_VIDEO_PLAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "examples/customizationFEC/video_player.h"
#include "examples/customizationFEC/video_player_config.h"
#include "examples/customizationFEC/video_player_defines.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc{

class VideoPlayerOptions;

namespace videoplayermodule{
// Class definitions
class RTC_EXPORT VideoPlayerImpl : public VideoPlayerModule {
 public:
  static rtc::scoped_refptr<VideoPlayerModule> Create(const char* file_path);

  // Helpers for converting between (integral) degrees and
  // VideoRotation values.  Return 0 on success.
  static int32_t RotationFromDegrees(int degrees, VideoRotation* rotation);
  static int32_t RotationInDegrees(VideoRotation rotation, int* degrees);

  // Call backs
  void RegisterPlayerDataCallback(rtc::VideoSinkInterface<VideoFrame>* dataCallback) override;
  virtual void RegisterPlayerDataCallback(RawVideoSinkPlayerInterface* dataCallback) override;
  void DeRegisterPlayerDataCallback() override;

  int32_t SetPlayRotation(VideoRotation rotation) override;
  bool SetApplyRotation(bool enable) override;
  bool GetApplyRotation() override;

  // `capture_time` must be specified in NTP time format in milliseconds.
  int32_t IncomingFrame(uint8_t* videoFrame,
                        size_t videoFrameLength,
                        const VideoPlayerCapability& frameInfo,
                        int64_t playTime = 0);

  // Platform dependent
  int32_t StartPlay(const VideoPlayerCapability& capability) override;
  int32_t StopPlay() override;
  bool PlayStarted() override;
  int32_t PlaySettings(VideoPlayerCapability& /*settings*/) override;

 protected:
  VideoPlayerImpl();
  ~VideoPlayerImpl() override;

  // Calls to the public API must happen on a single thread.
  SequenceChecker api_checker_;
  // RaceChecker for members that can be accessed on the API thread while
  // capture is not happening, and on a callback thread otherwise.
  rtc::RaceChecker capture_checker_;
  //TODO:XZG
  // current Device unique name;
  char* _deviceUniqueId RTC_GUARDED_BY(api_checker_);
  Mutex api_lock_;
  // Should be set by platform dependent code in StartCapture.
  VideoPlayerCapability _requestedCapability RTC_GUARDED_BY(api_checker_);

 private:
  void UpdateFrameCount();
  uint32_t CalculateFrameRate(int64_t now_ns);
  int32_t DeliverComingFrame(VideoFrame& comingFrame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(api_lock_);
  void DeliverRawFrame(uint8_t* videoFrame,
                       size_t videoFrameLength,
                       const VideoPlayerCapability& frameInfo,
                       int64_t playTime)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(api_lock_);

  // last time the module process function was called.
  int64_t _lastProcessTimeNanos RTC_GUARDED_BY(capture_checker_);
  // last time the frame rate callback function was called.
  int64_t _lastFrameRateCallbackTimeNanos RTC_GUARDED_BY(capture_checker_);

  rtc::VideoSinkInterface<VideoFrame>* _dataCallBack RTC_GUARDED_BY(api_lock_);
  RawVideoSinkPlayerInterface* _rawDataCallBack RTC_GUARDED_BY(api_lock_);

  int64_t _lastProcessFrameTimeNanos RTC_GUARDED_BY(capture_checker_);
  // timestamp for local captured frames
  int64_t _incomingFrameTimesNanos[kFrameRateCountHistorySize] RTC_GUARDED_BY(capture_checker_);
  // Set if the frame should be rotated by the capture module.
  VideoRotation _rotateFrame RTC_GUARDED_BY(api_lock_);

  // Indicate whether rotation should be applied before delivered externally.
  bool apply_rotation_ RTC_GUARDED_BY(api_lock_);

};

}  // namespace videoplayermodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_PLAYER_IMPL_H_