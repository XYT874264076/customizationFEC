
#include "examples/MyFECExp/MP4VideoPlayer.h"

#include <algorithm>
#include <iostream>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "examples/MyFECExp/VpmPlayer.h"
#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

MP4VideoPlayer::~MP4VideoPlayer() = default;

void MP4VideoPlayer::OnOutputFormatRequest(int width, int height, const std::optional<int>& max_fps){
  std::optional<std::pair<int, int>> target_aspect_ratio = std::make_pair(width, height);
  std::optional<int> max_pixel_count = width * height;
  video_adapter_.OnOutputFormatRequest(target_aspect_ratio, max_pixel_count, max_fps);
}

void MP4VideoPlayer::OnFrame(const VideoFrame& original_frame) {
  int cropped_width = 0;
  int cropped_height = 0;
  int out_width = 0;
  int out_height = 0;

  VideoFrame frame = MaybePreprocess(original_frame);

  bool enable_adaptation;
  {
    MutexLock lock(&lock_);
    enable_adaptation = enable_adaptation_;
  }
  if (!enable_adaptation) {
    broadcaster_.OnFrame(frame);
    return;
  }

  if (!video_adapter_.AdaptFrameResolution(
          frame.width(), frame.height(), frame.timestamp_us() * 1000,
          &cropped_width, &cropped_height, &out_width, &out_height)) {
    // Drop frame in order to respect frame rate constraint.
    return;
  }

  if (out_height != frame.height() || out_width != frame.width()) {
    // Video adapter has requested a down-scale. Allocate a new buffer and
    // return scaled version.
    // For simplicity, only scale here without cropping.
    rtc::scoped_refptr<I420Buffer> scaled_buffer = I420Buffer::Create(out_width, out_height);
    scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
    VideoFrame::Builder new_frame_builder = VideoFrame::Builder()
            .set_video_frame_buffer(scaled_buffer)
            .set_rotation(kVideoRotation_0)
            .set_timestamp_us(frame.timestamp_us())
            .set_id(frame.id());
    if (frame.has_update_rect()) {
      VideoFrame::UpdateRect new_rect = frame.update_rect().ScaleWithFrame(
          frame.width(), frame.height(), 0, 0, frame.width(), frame.height(),
          out_width, out_height);
      new_frame_builder.set_update_rect(new_rect);
    }
    broadcaster_.OnFrame(new_frame_builder.build());

  } else {
    // No adaptations needed, just return the frame as is.
    broadcaster_.OnFrame(frame);
  }
}

rtc::VideoSinkWants MP4VideoPlayer::GetSinkWants() {
  return broadcaster_.wants();
}

void MP4VideoPlayer::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  broadcaster_.AddOrUpdateSink(sink, wants);
  UpdateVideoAdapter();
}

void MP4VideoPlayer::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  broadcaster_.RemoveSink(sink);
  UpdateVideoAdapter();
}

void MP4VideoPlayer::UpdateVideoAdapter() {
  video_adapter_.OnSinkWants(broadcaster_.wants());
}

VideoFrame MP4VideoPlayer::MaybePreprocess(const VideoFrame& frame) {
  MutexLock lock(&lock_);
  if (preprocessor_ != nullptr) {
    return preprocessor_->Preprocess(frame);
  } else {
    return frame;
  }
}

std::unique_ptr<MP4VideoPlayer> CreateMP4VideoPlayer(
    size_t width,
    size_t height,
    size_t target_fps,
    const char* file_path){
    return absl::WrapUnique<MP4VideoPlayer>(test::VpmPlayer::Create(width, height, target_fps, file_path));
}

} // namespace test
} // namespace webrtc