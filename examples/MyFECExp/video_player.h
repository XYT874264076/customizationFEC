
#ifndef MODULE_VIDEO_PLAYER_H_
#define MODULE_VIDEO_PLAYER_H_

#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "examples/MyFECExp/raw_video_sink_interface.h"
#include "examples/MyFECExp/video_player_defines.h"

#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc{

class VideoPlayerModule : public RefCountInterface {
 public:
  //   Register capture data callback
  virtual void RegisterPlayerDataCallback(rtc::VideoSinkInterface<VideoFrame>* dataCallback) = 0;
  virtual void RegisterPlayerDataCallback(RawVideoSinkPlayerInterface* dataCallback) = 0;

  //  Remove capture data callback
  virtual void DeRegisterPlayerDataCallback() = 0;

  // Start capture device
  virtual int32_t StartPlay(const VideoPlayerCapability& capability) = 0;

  virtual int32_t StopPlay() = 0;

  // Returns true if the capture device is running
  virtual bool PlayStarted() = 0;

  // Gets the current configuration.
  virtual int32_t PlaySettings(VideoPlayerCapability& settings) = 0;

  // Set the rotation of the captured frames.
  // If the rotation is set to the same as returned by
  // DeviceInfo::GetOrientation the captured frames are
  // displayed correctly if rendered.
  virtual int32_t SetPlayRotation(VideoRotation rotation) = 0;

  // Tells the capture module whether to apply the pending rotation. By default,
  // the rotation is applied and the generated frame is up right. When set to
  // false, generated frames will carry the rotation information from
  // SetCaptureRotation. Return value indicates whether this operation succeeds.
  virtual bool SetApplyRotation(bool enable) = 0;

  // Return whether the rotation is applied or left pending.
  virtual bool GetApplyRotation() = 0;

 protected:
  ~VideoPlayerModule() override {}
};

class RTC_EXPORT VideoPlayerFactory {
 public:
  static rtc::scoped_refptr<VideoPlayerModule> Create(const char* file_path);
  
 private:
  ~VideoPlayerFactory();
};

}

#endif  // MODULE_VIDEO_PLAYER_H_