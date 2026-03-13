/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/customizationFEC/rtp_sender_video.h"

#include <stdlib.h>
#include <string.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/field_trials_view.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_layers_allocation.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "common_video/corruption_detection_converters.h"
#include "common_video/corruption_detection_message.h"
#include "common_video/frame_instrumentation_data.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/absolute_capture_time_sender.h"
#include "modules/rtp_rtcp/source/corruption_detection_extension.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"
// #include "examples/MyFECExp/video_fec_generator.h"
#include "examples/customizationFEC/video_fec_generator.h"
#include "examples/customizationFEC/Params.h"
#include "examples/customizationFEC/Tambur/protocol.hh"
#include "examples/customizationFEC/Tambur/src/fec/fec_datagram.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/coding_matrix_info.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/block_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/multi_frame_fec_helpers.hh"
#include "examples/customizationFEC/Tambur/src/fec/feedback_datagram.hh"
#include "examples/customizationFEC/Tambur/src/fec/fec_datagram.hh"
#include "examples/customizationFEC/Tambur/src/fec/code.hh"
#include "examples/customizationFEC/Tambur/src/fec/metric_logger.hh"
#include "examples/customizationFEC/Tambur/src/fec/header_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/packetization.hh"
#include "examples/customizationFEC/Tambur/src/fec/streaming_code/streaming_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/streaming_code/streaming_code_packetization.hh"
#include "examples/customizationFEC/Tambur/src/fec/streaming_code/multi_fec_header_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/logging/timing_logger.hh"
#include "examples/customizationFEC/Tambur/src/fec/frame_generator.hh"
#include "examples/customizationFEC/Tambur/src/fec/fec_sender.hh"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

namespace {
constexpr size_t kRedForFecHeaderLength = 1;
constexpr TimeDelta kMaxUnretransmittableFrameInterval =
    TimeDelta::Millis(33 * 4);

void BuildRedPayload(const RtpPacketToSend& media_packet,
                     RtpPacketToSend* red_packet) {
  uint8_t* red_payload = red_packet->AllocatePayload(
      kRedForFecHeaderLength + media_packet.payload_size());
  RTC_DCHECK(red_payload);
  red_payload[0] = media_packet.PayloadType();

  auto media_payload = media_packet.payload();
  memcpy(&red_payload[kRedForFecHeaderLength], media_payload.data(),
         media_payload.size());
}

bool MinimizeDescriptor(RTPVideoHeader* video_header) {
  if (auto* vp8 =
          absl::get_if<RTPVideoHeaderVP8>(&video_header->video_type_header)) {
    // Set minimum fields the RtpPacketizer is using to create vp8 packets.
    // nonReference is the only field that doesn't require extra space.
    bool non_reference = vp8->nonReference;
    vp8->InitRTPVideoHeaderVP8();
    vp8->nonReference = non_reference;
    return true;
  }
  return false;
}

bool IsBaseLayer(const RTPVideoHeader& video_header) {
  // For AV1 & H.265 we fetch temporal index from the generic descriptor.
  if (video_header.generic) {
    const auto& generic = video_header.generic.value();
    return (generic.temporal_index == 0 ||
            generic.temporal_index == kNoTemporalIdx);
  }
  switch (video_header.codec) {
    case kVideoCodecVP8: {
      const auto& vp8 =
          absl::get<RTPVideoHeaderVP8>(video_header.video_type_header);
      return (vp8.temporalIdx == 0 || vp8.temporalIdx == kNoTemporalIdx);
    }
    case kVideoCodecVP9: {
      const auto& vp9 =
          absl::get<RTPVideoHeaderVP9>(video_header.video_type_header);
      return (vp9.temporal_idx == 0 || vp9.temporal_idx == kNoTemporalIdx);
    }
    case kVideoCodecH264:
      // TODO(kron): Implement logic for H264 once WebRTC supports temporal
      // layers for H264.
      break;
    // These codecs do not have codec-specifics, from which we can fetch
    // temporal index.
    case kVideoCodecH265:
    case kVideoCodecAV1:
    case kVideoCodecGeneric:
      break;
  }
  return true;
}

std::optional<VideoPlayoutDelay> LoadVideoPlayoutDelayOverride(
    const FieldTrialsView* key_value_config) {
  RTC_DCHECK(key_value_config);
  FieldTrialOptional<int> playout_delay_min_ms("min_ms", std::nullopt);
  FieldTrialOptional<int> playout_delay_max_ms("max_ms", std::nullopt);
  ParseFieldTrial({&playout_delay_max_ms, &playout_delay_min_ms},
                  key_value_config->Lookup("WebRTC-ForceSendPlayoutDelay"));
  return playout_delay_max_ms && playout_delay_min_ms
             ? std::make_optional<VideoPlayoutDelay>(
                   TimeDelta::Millis(*playout_delay_min_ms),
                   TimeDelta::Millis(*playout_delay_max_ms))
             : std::nullopt;
}

// Some packets can be skipped and the stream can still be decoded. Those
// packets are less likely to be retransmitted if they are lost.
bool PacketWillLikelyBeRequestedForRestransmissionIfLost(
    const RTPVideoHeader& video_header) {
  return IsBaseLayer(video_header) &&
         !(video_header.generic.has_value()
               ? absl::c_linear_search(
                     video_header.generic->decode_target_indications,
                     DecodeTargetIndication::kDiscardable)
               : false);
}

}  // namespace

RTPSenderVideo::RTPSenderVideo(const Config& config)
    : rtp_sender_(config.rtp_sender),
      clock_(config.clock),
      retransmission_settings_(
          config.enable_retransmit_all_layers
              ? kRetransmitAllLayers
              : (kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers)),
      last_rotation_(kVideoRotation_0),
      transmit_color_space_next_frame_(false),
      send_allocation_(SendVideoLayersAllocation::kDontSend),
      playout_delay_pending_(false),
      forced_playout_delay_(LoadVideoPlayoutDelayOverride(config.field_trials)),
      red_payload_type_(config.red_payload_type),
      fec_type_(config.fec_type),
      fec_overhead_bytes_(config.fec_overhead_bytes),
      post_encode_overhead_bitrate_(/*max_window_size=*/TimeDelta::Seconds(1)),
      frame_encryptor_(config.frame_encryptor),
      require_frame_encryption_(config.require_frame_encryption),
      generic_descriptor_auth_experiment_(!absl::StartsWith(
          config.field_trials->Lookup("WebRTC-GenericDescriptorAuth"),
          "Disabled")),
      absolute_capture_time_sender_(config.clock),
      frame_transformer_delegate_(
          config.frame_transformer
              ? rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
                    this,
                    config.frame_transformer,
                    rtp_sender_->SSRC(),
                    config.task_queue_factory)
              : nullptr),
      enable_av1_even_split_(
          config.field_trials->IsEnabled("WebRTC-Video-AV1EvenPayloadSizes")) {
  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Init();
}

RTPSenderVideo::~RTPSenderVideo() {
  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Reset();
}

void RTPSenderVideo::LogAndSendToNetwork(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets,
    size_t encoder_output_size) {
  {
    MutexLock lock(&stats_mutex_);

    size_t packetized_payload_size = 0;
    for (const auto& packet : packets) {
      if (*packet->packet_type() == RtpPacketMediaType::kVideo) {
        packetized_payload_size += packet->payload_size();
      }
    }
    // AV1 and H264 packetizers may produce less packetized bytes than
    // unpacketized.
    if (packetized_payload_size >= encoder_output_size) {
      post_encode_overhead_bitrate_.Update(
          packetized_payload_size - encoder_output_size, clock_->CurrentTime());
    }
  }

  rtp_sender_->EnqueuePackets(std::move(packets));
}

size_t RTPSenderVideo::FecPacketOverhead() const {
  size_t overhead = fec_overhead_bytes_;
  if (red_enabled()) {
    // The RED overhead is due to a small header.
    overhead += kRedForFecHeaderLength;

    if (fec_type_ == VideoFecGenerator::FecType::kUlpFec) {
      // For ULPFEC, the overhead is the FEC headers plus RED for FEC header
      // (see above) plus anything in RTP header beyond the 12 bytes base header
      // (CSRC list, extensions...)
      // This reason for the header extensions to be included here is that
      // from an FEC viewpoint, they are part of the payload to be protected.
      // (The base RTP header is already protected by the FEC header.)
      overhead +=
          rtp_sender_->FecOrPaddingPacketMaxRtpHeaderLength() - kRtpHeaderSize;
    }
  }
  return overhead;
}

void RTPSenderVideo::SetRetransmissionSetting(int32_t retransmission_settings) {
  RTC_DCHECK_RUNS_SERIALIZED(&send_checker_);
  retransmission_settings_ = retransmission_settings;
}

void RTPSenderVideo::SetVideoStructure(
    const FrameDependencyStructure* video_structure) {
  if (frame_transformer_delegate_) {
    frame_transformer_delegate_->SetVideoStructureUnderLock(video_structure);
    return;
  }
  SetVideoStructureInternal(video_structure);
}

void RTPSenderVideo::SetVideoStructureAfterTransformation(
    const FrameDependencyStructure* video_structure) {
  SetVideoStructureInternal(video_structure);
}

void RTPSenderVideo::SetVideoStructureInternal(
    const FrameDependencyStructure* video_structure) {
  RTC_DCHECK_RUNS_SERIALIZED(&send_checker_);
  if (video_structure == nullptr) {
    video_structure_ = nullptr;
    return;
  }
  // Simple sanity checks video structure is set up.
  RTC_DCHECK_GT(video_structure->num_decode_targets, 0);
  RTC_DCHECK_GT(video_structure->templates.size(), 0);

  int structure_id = 0;
  if (video_structure_) {
    if (*video_structure_ == *video_structure) {
      // Same structure (just a new key frame), no update required.
      return;
    }
    // When setting different video structure make sure structure_id is updated
    // so that templates from different structures do not collide.
    static constexpr int kMaxTemplates = 64;
    structure_id =
        (video_structure_->structure_id + video_structure_->templates.size()) %
        kMaxTemplates;
  }

  video_structure_ =
      std::make_unique<FrameDependencyStructure>(*video_structure);
  video_structure_->structure_id = structure_id;
}

void RTPSenderVideo::SetVideoLayersAllocation(
    VideoLayersAllocation allocation) {
  if (frame_transformer_delegate_) {
    frame_transformer_delegate_->SetVideoLayersAllocationUnderLock(
        std::move(allocation));
    return;
  }
  SetVideoLayersAllocationInternal(std::move(allocation));
}

void RTPSenderVideo::SetVideoLayersAllocationAfterTransformation(
    VideoLayersAllocation allocation) {
  SetVideoLayersAllocationInternal(std::move(allocation));
}

void RTPSenderVideo::SetVideoLayersAllocationInternal(
    VideoLayersAllocation allocation) {
  RTC_DCHECK_RUNS_SERIALIZED(&send_checker_);
  if (!allocation_ || allocation.active_spatial_layers.size() !=
                          allocation_->active_spatial_layers.size()) {
    send_allocation_ = SendVideoLayersAllocation::kSendWithResolution;
  } else if (send_allocation_ == SendVideoLayersAllocation::kDontSend) {
    send_allocation_ = SendVideoLayersAllocation::kSendWithoutResolution;
  }
  if (send_allocation_ == SendVideoLayersAllocation::kSendWithoutResolution) {
    // Check if frame rate changed more than 5fps since the last time the
    // extension was sent with frame rate and resolution.
    for (size_t i = 0; i < allocation.active_spatial_layers.size(); ++i) {
      if (abs(static_cast<int>(
                  allocation.active_spatial_layers[i].frame_rate_fps) -
              static_cast<int>(
                  last_full_sent_allocation_->active_spatial_layers[i]
                      .frame_rate_fps)) > 5) {
        send_allocation_ = SendVideoLayersAllocation::kSendWithResolution;
        break;
      }
    }
  }
  allocation_ = std::move(allocation);
}

void RTPSenderVideo::AddRtpHeaderExtensions(const RTPVideoHeader& video_header,
                                            bool first_packet,
                                            bool last_packet,
                                            RtpPacketToSend* packet) const { 
  // Send color space when changed or if the frame is a key frame. Keep
  // sending color space information until the first base layer frame to
  // guarantee that the information is retrieved by the receiver.
  bool set_color_space =
      video_header.color_space != last_color_space_ ||
      video_header.frame_type == VideoFrameType::kVideoFrameKey ||
      transmit_color_space_next_frame_;
  // Color space requires two-byte header extensions if HDR metadata is
  // included. Therefore, it's best to add this extension first so that the
  // other extensions in the same packet are written as two-byte headers at
  // once.
  if (last_packet && set_color_space && video_header.color_space)
    packet->SetExtension<ColorSpaceExtension>(video_header.color_space.value());

  // According to
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
  // ts_126114v120700p.pdf Section 7.4.5:
  // The MTSI client shall add the payload bytes as defined in this clause
  // onto the last RTP packet in each group of packets which make up a key
  // frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
  // (HEVC)). The MTSI client may also add the payload bytes onto the last RTP
  // packet in each group of packets which make up another type of frame
  // (e.g. a P-Frame) only if the current value is different from the previous
  // value sent.
  // Set rotation when key frame or when changed (to follow standard).
  // Or when different from 0 (to follow current receiver implementation).
  bool set_video_rotation =
      video_header.frame_type == VideoFrameType::kVideoFrameKey ||
      video_header.rotation != last_rotation_ ||
      video_header.rotation != kVideoRotation_0;
  if (last_packet && set_video_rotation)
    packet->SetExtension<VideoOrientation>(video_header.rotation);

  // Report content type only for key frames.
  if (last_packet &&
      video_header.frame_type == VideoFrameType::kVideoFrameKey &&
      video_header.content_type != VideoContentType::UNSPECIFIED)
    packet->SetExtension<VideoContentTypeExtension>(video_header.content_type);

  if (last_packet &&
      video_header.video_timing.flags != VideoSendTiming::kInvalid)
    packet->SetExtension<VideoTimingExtension>(video_header.video_timing);

  // If transmitted, add to all packets; ack logic depends on this.
  if (playout_delay_pending_ && current_playout_delay_.has_value()) {
    packet->SetExtension<PlayoutDelayLimits>(*current_playout_delay_);
  }

  if (first_packet && video_header.absolute_capture_time.has_value()) {
    packet->SetExtension<AbsoluteCaptureTimeExtension>(
        *video_header.absolute_capture_time);
  }

  if (video_header.generic) {
    bool extension_is_set = false;
    if (packet->IsRegistered<RtpDependencyDescriptorExtension>() &&
        video_structure_ != nullptr) {
      DependencyDescriptor descriptor;
      descriptor.first_packet_in_frame = first_packet;
      descriptor.last_packet_in_frame = last_packet;
      descriptor.frame_number = video_header.generic->frame_id & 0xFFFF;
      descriptor.frame_dependencies.spatial_id =
          video_header.generic->spatial_index;
      descriptor.frame_dependencies.temporal_id =
          video_header.generic->temporal_index;
      for (int64_t dep : video_header.generic->dependencies) {
        descriptor.frame_dependencies.frame_diffs.push_back(
            video_header.generic->frame_id - dep);
      }
      descriptor.frame_dependencies.chain_diffs =
          video_header.generic->chain_diffs;
      descriptor.frame_dependencies.decode_target_indications =
          video_header.generic->decode_target_indications;
      RTC_DCHECK_EQ(
          descriptor.frame_dependencies.decode_target_indications.size(),
          video_structure_->num_decode_targets);

      if (first_packet) {
        descriptor.active_decode_targets_bitmask =
            active_decode_targets_tracker_.ActiveDecodeTargetsBitmask();
      }
      // VP9 mark all layer frames of the first picture as kVideoFrameKey,
      // Structure should be attached to the descriptor to lowest spatial layer
      // when inter layer dependency is used, i.e. L structures; or to all
      // layers when inter layer dependency is not used, i.e. S structures.
      // Distinguish these two cases by checking if there are any dependencies.
      if (video_header.frame_type == VideoFrameType::kVideoFrameKey &&
          video_header.generic->dependencies.empty() && first_packet) {
        // To avoid extra structure copy, temporary share ownership of the
        // video_structure with the dependency descriptor.
        descriptor.attached_structure =
            absl::WrapUnique(video_structure_.get());
      }
      extension_is_set = packet->SetExtension<RtpDependencyDescriptorExtension>(
          *video_structure_,
          active_decode_targets_tracker_.ActiveChainsBitmask(), descriptor);

      // Remove the temporary shared ownership.
      descriptor.attached_structure.release();
    }

    // Do not use generic frame descriptor when dependency descriptor is stored.
    if (packet->IsRegistered<RtpGenericFrameDescriptorExtension00>() &&
        !extension_is_set) {
      RtpGenericFrameDescriptor generic_descriptor;
      generic_descriptor.SetFirstPacketInSubFrame(first_packet);
      generic_descriptor.SetLastPacketInSubFrame(last_packet);

      if (first_packet) {
        generic_descriptor.SetFrameId(
            static_cast<uint16_t>(video_header.generic->frame_id));
        for (int64_t dep : video_header.generic->dependencies) {
          generic_descriptor.AddFrameDependencyDiff(
              video_header.generic->frame_id - dep);
        }

        uint8_t spatial_bitmask = 1 << video_header.generic->spatial_index;
        generic_descriptor.SetSpatialLayersBitmask(spatial_bitmask);

        generic_descriptor.SetTemporalLayer(
            video_header.generic->temporal_index);

        if (video_header.frame_type == VideoFrameType::kVideoFrameKey) {
          generic_descriptor.SetResolution(video_header.width,
                                           video_header.height);
        }
      }

      packet->SetExtension<RtpGenericFrameDescriptorExtension00>(
          generic_descriptor);
    }
  }

  if (packet->IsRegistered<RtpVideoLayersAllocationExtension>() &&
      first_packet &&
      send_allocation_ != SendVideoLayersAllocation::kDontSend &&
      (video_header.frame_type == VideoFrameType::kVideoFrameKey ||
       PacketWillLikelyBeRequestedForRestransmissionIfLost(video_header))) {
    VideoLayersAllocation allocation = allocation_.value();
    allocation.resolution_and_frame_rate_is_valid =
        send_allocation_ == SendVideoLayersAllocation::kSendWithResolution;
    packet->SetExtension<RtpVideoLayersAllocationExtension>(allocation);
  }

  if (first_packet && video_header.video_frame_tracking_id) {
    packet->SetExtension<VideoFrameTrackingIdExtension>(
        *video_header.video_frame_tracking_id);
  }

  if (last_packet && video_header.frame_instrumentation_data) {
    std::optional<CorruptionDetectionMessage> message;
    if (const auto* data = absl::get_if<FrameInstrumentationData>(
            &(*video_header.frame_instrumentation_data))) {
      message =
          ConvertFrameInstrumentationDataToCorruptionDetectionMessage(*data);
    } else if (const auto* sync_data =
                   absl::get_if<FrameInstrumentationSyncData>(
                       &(*video_header.frame_instrumentation_data))) {
      message = ConvertFrameInstrumentationSyncDataToCorruptionDetectionMessage(
          *sync_data);
    } else {
      RTC_DCHECK_NOTREACHED();
    }

    if (message.has_value()) {
      packet->SetExtension<CorruptionDetectionExtension>(*message);
    } else {
      RTC_LOG(LS_WARNING) << "Failed to convert frame instrumentation data to "
                             "corruption detection message.";
    }
  }
}

bool RTPSenderVideo::SendVideo(int payload_type,
                               std::optional<VideoCodecType> codec_type,
                               uint32_t rtp_timestamp,
                               Timestamp capture_time,
                               rtc::ArrayView<const uint8_t> payload,
                               size_t encoder_output_size,
                               RTPVideoHeader video_header,
                               TimeDelta expected_retransmission_time,
                               std::vector<uint32_t> csrcs) {
  RTC_CHECK_RUNS_SERIALIZED(&send_checker_);

  // Check if we should use Tambur FEC
  if (inputV::Params::type == inputV::ExpType::TamburFEC) {
    return SendVideoWithTamburFEC(payload_type, codec_type, rtp_timestamp,
                                 capture_time, payload, encoder_output_size,
                                 video_header, expected_retransmission_time, csrcs);
  }

  // Original WebRTC logic for other FEC types
  if (video_header.frame_type == VideoFrameType::kEmptyFrame)
    return true;

  if (payload.empty())
    return false;

  if (!rtp_sender_->SendingMedia()) {
    return false;
  }

  int32_t retransmission_settings = retransmission_settings_;
  if (codec_type == VideoCodecType::kVideoCodecH264) {
    // Backward compatibility for older receivers without temporal layer logic.
    retransmission_settings = kRetransmitBaseLayer | kRetransmitHigherLayers;
  }
  const uint8_t temporal_id = GetTemporalId(video_header);
  // TODO(bugs.webrtc.org/10714): retransmission_settings_ should generally be
  // replaced by expected_retransmission_time.IsFinite().
  const bool allow_retransmission =
      expected_retransmission_time.IsFinite() &&
      AllowRetransmission(temporal_id, retransmission_settings,
                          expected_retransmission_time);

  MaybeUpdateCurrentPlayoutDelay(video_header);
  if (video_header.frame_type == VideoFrameType::kVideoFrameKey) {
    if (current_playout_delay_.has_value()) {
      // Force playout delay on key-frames, if set.
      playout_delay_pending_ = true;
    }
    if (allocation_) {
      // Send the bitrate allocation on every key frame.
      send_allocation_ = SendVideoLayersAllocation::kSendWithResolution;
    }
  }

  if (video_structure_ != nullptr && video_header.generic) {
    active_decode_targets_tracker_.OnFrame(
        video_structure_->decode_target_protected_by_chain,
        video_header.generic->active_decode_targets,
        video_header.frame_type == VideoFrameType::kVideoFrameKey,
        video_header.generic->frame_id, video_header.generic->chain_diffs);
  }

  // No FEC protection for upper temporal layers, if used.
  const bool use_fec = fec_type_.has_value() &&
                       (temporal_id == 0 || temporal_id == kNoTemporalIdx);

  // Maximum size of packet including rtp headers.
  // Extra space left in case packet will be resent using fec or rtx.
  int packet_capacity = rtp_sender_->MaxRtpPacketSize();
  if (use_fec) {
    packet_capacity -= FecPacketOverhead();
  }
  if (allow_retransmission) {
    packet_capacity -= rtp_sender_->RtxPacketOverhead();
  }

  std::unique_ptr<RtpPacketToSend> single_packet =
      rtp_sender_->AllocatePacket(csrcs);
  RTC_DCHECK_LE(packet_capacity, single_packet->capacity());
  single_packet->SetPayloadType(payload_type);
  single_packet->SetTimestamp(rtp_timestamp);
  if (capture_time.IsFinite())
    single_packet->set_capture_time(capture_time);

  // Construct the absolute capture time extension if not provided.
  if (!video_header.absolute_capture_time.has_value() &&
      capture_time.IsFinite()) {
    video_header.absolute_capture_time.emplace();
    video_header.absolute_capture_time->absolute_capture_timestamp =
        Int64MsToUQ32x32(
            clock_->ConvertTimestampToNtpTime(capture_time).ToMs());
    video_header.absolute_capture_time->estimated_capture_clock_offset = 0;
  }

  // Let `absolute_capture_time_sender_` decide if the extension should be sent.
  if (video_header.absolute_capture_time.has_value()) {
    video_header.absolute_capture_time =
        absolute_capture_time_sender_.OnSendPacket(
            AbsoluteCaptureTimeSender::GetSource(single_packet->Ssrc(), csrcs),
            single_packet->Timestamp(), kVideoPayloadTypeFrequency,
            NtpTime(
                video_header.absolute_capture_time->absolute_capture_timestamp),
            video_header.absolute_capture_time->estimated_capture_clock_offset);
  }

  auto first_packet = std::make_unique<RtpPacketToSend>(*single_packet);
  auto middle_packet = std::make_unique<RtpPacketToSend>(*single_packet);
  auto last_packet = std::make_unique<RtpPacketToSend>(*single_packet);
  // Simplest way to estimate how much extensions would occupy is to set them.
  AddRtpHeaderExtensions(video_header,
                         /*first_packet=*/true, /*last_packet=*/true,
                         single_packet.get());
  if (video_structure_ != nullptr &&
      single_packet->IsRegistered<RtpDependencyDescriptorExtension>() &&
      !single_packet->HasExtension<RtpDependencyDescriptorExtension>()) {
    RTC_DCHECK_EQ(video_header.frame_type, VideoFrameType::kVideoFrameKey);
    // Disable attaching dependency descriptor to delta packets (including
    // non-first packet of a key frame) when it wasn't attached to a key frame,
    // as dependency descriptor can't be usable in such case.
    // This can also happen when the descriptor is larger than 15 bytes and
    // two-byte header extensions are not negotiated using extmap-allow-mixed.
    RTC_LOG(LS_WARNING) << "Disable dependency descriptor because failed to "
                           "attach it to a key frame.";
    video_structure_ = nullptr;
  }

  AddRtpHeaderExtensions(video_header,
                         /*first_packet=*/true, /*last_packet=*/false,
                         first_packet.get());
  AddRtpHeaderExtensions(video_header,
                         /*first_packet=*/false, /*last_packet=*/false,
                         middle_packet.get());
  AddRtpHeaderExtensions(video_header,
                         /*first_packet=*/false, /*last_packet=*/true,
                         last_packet.get());

  RTC_DCHECK_GT(packet_capacity, single_packet->headers_size());
  RTC_DCHECK_GT(packet_capacity, first_packet->headers_size());
  RTC_DCHECK_GT(packet_capacity, middle_packet->headers_size());
  RTC_DCHECK_GT(packet_capacity, last_packet->headers_size());
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = packet_capacity - middle_packet->headers_size();

  RTC_DCHECK_GE(single_packet->headers_size(), middle_packet->headers_size());
  limits.single_packet_reduction_len =
      single_packet->headers_size() - middle_packet->headers_size();

  RTC_DCHECK_GE(first_packet->headers_size(), middle_packet->headers_size());
  limits.first_packet_reduction_len =
      first_packet->headers_size() - middle_packet->headers_size();

  RTC_DCHECK_GE(last_packet->headers_size(), middle_packet->headers_size());
  limits.last_packet_reduction_len =
      last_packet->headers_size() - middle_packet->headers_size();

  bool has_generic_descriptor =
      first_packet->HasExtension<RtpGenericFrameDescriptorExtension00>() ||
      first_packet->HasExtension<RtpDependencyDescriptorExtension>();

  // Minimization of the vp8 descriptor may erase temporal_id, so use
  // `temporal_id` rather than reference `video_header` beyond this point.
  if (has_generic_descriptor) {
    MinimizeDescriptor(&video_header);
  }

  rtc::Buffer encrypted_video_payload;
  if (frame_encryptor_ != nullptr) {
    const size_t max_ciphertext_size =
        frame_encryptor_->GetMaxCiphertextByteSize(cricket::MEDIA_TYPE_VIDEO,
                                                   payload.size());
    encrypted_video_payload.SetSize(max_ciphertext_size);

    size_t bytes_written = 0;

    // Enable header authentication if the field trial isn't disabled.
    std::vector<uint8_t> additional_data;
    if (generic_descriptor_auth_experiment_) {
      additional_data = RtpDescriptorAuthentication(video_header);
    }

    if (frame_encryptor_->Encrypt(
            cricket::MEDIA_TYPE_VIDEO, first_packet->Ssrc(), additional_data,
            payload, encrypted_video_payload, &bytes_written) != 0) {
      return false;
    }

    encrypted_video_payload.SetSize(bytes_written);
    payload = encrypted_video_payload;
  } else if (require_frame_encryption_) {
    RTC_LOG(LS_WARNING)
        << "No FrameEncryptor is attached to this video sending stream but "
           "one is required since require_frame_encryptor is set";
  }

  std::unique_ptr<RtpPacketizer> packetizer = RtpPacketizer::Create(
      codec_type, payload, limits, video_header, enable_av1_even_split_);

  const size_t num_packets = packetizer->NumPackets();

  if (num_packets == 0)
    return false;

  bool first_frame = first_frame_sent_();
  std::vector<std::unique_ptr<RtpPacketToSend>> rtp_packets;
  for (size_t i = 0; i < num_packets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet;
    int expected_payload_capacity;
    // Choose right packet template:
    if (num_packets == 1) {
      packet = std::move(single_packet);
      expected_payload_capacity =
          limits.max_payload_len - limits.single_packet_reduction_len;
    } else if (i == 0) {
      packet = std::move(first_packet);
      expected_payload_capacity =
          limits.max_payload_len - limits.first_packet_reduction_len;
    } else if (i == num_packets - 1) {
      packet = std::move(last_packet);
      expected_payload_capacity =
          limits.max_payload_len - limits.last_packet_reduction_len;
    } else {
      packet = std::make_unique<RtpPacketToSend>(*middle_packet);
      expected_payload_capacity = limits.max_payload_len;
    }

    packet->set_first_packet_of_frame(i == 0);

    if (!packetizer->NextPacket(packet.get()))
      return false;
    RTC_DCHECK_LE(packet->payload_size(), expected_payload_capacity);

    packet->set_allow_retransmission(allow_retransmission);
    packet->set_is_key_frame(video_header.frame_type ==
                             VideoFrameType::kVideoFrameKey);

    // Put packetization finish timestamp into extension.
    if (packet->HasExtension<VideoTimingExtension>()) {
      packet->set_packetization_finish_time(clock_->CurrentTime());
    }

    packet->set_fec_protect_packet(use_fec);

    if (red_enabled()) {
      // TODO(sprang): Consider packetizing directly into packets with the RED
      // header already in place, to avoid this copy.
      std::unique_ptr<RtpPacketToSend> red_packet(new RtpPacketToSend(*packet));
      BuildRedPayload(*packet, red_packet.get());
      red_packet->SetPayloadType(*red_payload_type_);
      red_packet->set_is_red(true);

      // Append `red_packet` instead of `packet` to output.
      red_packet->set_packet_type(RtpPacketMediaType::kVideo);
      red_packet->set_allow_retransmission(packet->allow_retransmission());
      rtp_packets.emplace_back(std::move(red_packet));
    } else {
      packet->set_packet_type(RtpPacketMediaType::kVideo);
      rtp_packets.emplace_back(std::move(packet));
    }

    if (first_frame) {
      if (i == 0) {
        RTC_LOG(LS_INFO)
            << "Sent first RTP packet of the first video frame (pre-pacer)";
      }
      if (i == num_packets - 1) {
        RTC_LOG(LS_INFO)
            << "Sent last RTP packet of the first video frame (pre-pacer)";
      }
    }
  }

  LogAndSendToNetwork(std::move(rtp_packets), encoder_output_size);

  // Update details about the last sent frame.
  last_rotation_ = video_header.rotation;

  if (video_header.color_space != last_color_space_) {
    last_color_space_ = video_header.color_space;
    transmit_color_space_next_frame_ = !IsBaseLayer(video_header);
  } else {
    transmit_color_space_next_frame_ =
        transmit_color_space_next_frame_ ? !IsBaseLayer(video_header) : false;
  }

  if (video_header.frame_type == VideoFrameType::kVideoFrameKey ||
      PacketWillLikelyBeRequestedForRestransmissionIfLost(video_header)) {
    // This frame will likely be delivered, no need to populate playout
    // delay extensions until it changes again.
    playout_delay_pending_ = false;
    if (send_allocation_ == SendVideoLayersAllocation::kSendWithResolution) {
      last_full_sent_allocation_ = allocation_;
    }
    send_allocation_ = SendVideoLayersAllocation::kDontSend;
  }

  return true;
}

bool RTPSenderVideo::SendEncodedImage(int payload_type,
                                      std::optional<VideoCodecType> codec_type,
                                      uint32_t rtp_timestamp,
                                      const EncodedImage& encoded_image,
                                      RTPVideoHeader video_header,
                                      TimeDelta expected_retransmission_time) {
  if (frame_transformer_delegate_) {
    // The frame will be sent async once transformed.
    return frame_transformer_delegate_->TransformFrame(
        payload_type, codec_type, rtp_timestamp, encoded_image, video_header,
        expected_retransmission_time);
  }
  return SendVideo(payload_type, codec_type, rtp_timestamp,
                   encoded_image.CaptureTime(), encoded_image,
                   encoded_image.size(), video_header,
                   expected_retransmission_time, /*csrcs=*/{});
}

DataRate RTPSenderVideo::PostEncodeOverhead() const {
  MutexLock lock(&stats_mutex_);
  return post_encode_overhead_bitrate_.Rate(clock_->CurrentTime())
      .value_or(DataRate::Zero());
}

bool RTPSenderVideo::AllowRetransmission(
    uint8_t temporal_id,
    int32_t retransmission_settings,
    TimeDelta expected_retransmission_time) {
  if (retransmission_settings == kRetransmitOff)
    return false;

  MutexLock lock(&stats_mutex_);
  // Media packet storage.
  if ((retransmission_settings & kConditionallyRetransmitHigherLayers) &&
      UpdateConditionalRetransmit(temporal_id, expected_retransmission_time)) {
    retransmission_settings |= kRetransmitHigherLayers;
  }

  if (temporal_id == kNoTemporalIdx)
    return true;

  if ((retransmission_settings & kRetransmitBaseLayer) && temporal_id == 0)
    return true;

  if ((retransmission_settings & kRetransmitHigherLayers) && temporal_id > 0)
    return true;

  return false;
}

uint8_t RTPSenderVideo::GetTemporalId(const RTPVideoHeader& header) {
  struct TemporalIdGetter {
    uint8_t operator()(const RTPVideoHeaderVP8& vp8) { return vp8.temporalIdx; }
    uint8_t operator()(const RTPVideoHeaderVP9& vp9) {
      return vp9.temporal_idx;
    }
    uint8_t operator()(const RTPVideoHeaderH264&) { return kNoTemporalIdx; }
    uint8_t operator()(const RTPVideoHeaderLegacyGeneric&) {
      return kNoTemporalIdx;
    }
    uint8_t operator()(const absl::monostate&) { return kNoTemporalIdx; }
  };
  return absl::visit(TemporalIdGetter(), header.video_type_header);
}

bool RTPSenderVideo::UpdateConditionalRetransmit(
    uint8_t temporal_id,
    TimeDelta expected_retransmission_time) {
  Timestamp now = clock_->CurrentTime();
  // Update stats for any temporal layer.
  TemporalLayerStats* current_layer_stats =
      &frame_stats_by_temporal_layer_[temporal_id];
  current_layer_stats->frame_rate.Update(now);
  TimeDelta tl_frame_interval = now - current_layer_stats->last_frame_time;
  current_layer_stats->last_frame_time = now;

  // Conditional retransmit only applies to upper layers.
  if (temporal_id != kNoTemporalIdx && temporal_id > 0) {
    if (tl_frame_interval >= kMaxUnretransmittableFrameInterval) {
      // Too long since a retransmittable frame in this layer, enable NACK
      // protection.
      return true;
    } else {
      // Estimate when the next frame of any lower layer will be sent.
      Timestamp expected_next_frame_time = Timestamp::PlusInfinity();
      for (int i = temporal_id - 1; i >= 0; --i) {
        TemporalLayerStats* stats = &frame_stats_by_temporal_layer_[i];
        std::optional<Frequency> rate = stats->frame_rate.Rate(now);
        if (rate > Frequency::Zero()) {
          Timestamp tl_next = stats->last_frame_time + 1 / *rate;
          if (tl_next - now > -expected_retransmission_time &&
              tl_next < expected_next_frame_time) {
            expected_next_frame_time = tl_next;
          }
        }
      }

      if (expected_next_frame_time - now > expected_retransmission_time) {
        // The next frame in a lower layer is expected at a later time (or
        // unable to tell due to lack of data) than a retransmission is
        // estimated to be able to arrive, so allow this packet to be nacked.
        return true;
      }
    }
  }

  return false;
}

void RTPSenderVideo::MaybeUpdateCurrentPlayoutDelay(
    const RTPVideoHeader& header) {
  std::optional<VideoPlayoutDelay> requested_delay =
      forced_playout_delay_.has_value() ? forced_playout_delay_
                                        : header.playout_delay;

  if (!requested_delay.has_value()) {
    return;
  }

  current_playout_delay_ = requested_delay;
  playout_delay_pending_ = true;
}

bool RTPSenderVideo::SendVideoWithTamburFEC(
    int payload_type,
    std::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    Timestamp capture_time,
    rtc::ArrayView<const uint8_t> payload,
    size_t encoder_output_size,
    RTPVideoHeader video_header,
    TimeDelta expected_retransmission_time,
    std::vector<uint32_t> csrcs) {

  // Initialize Tambur FEC components
  if (!tambur_encoder_) {
    
    // Get configuration parameters from WebRTC config
    uint16_t tau = inputV::Params::tambur_tau; // default FEC protection window size 
    int stripe_size = inputV::Params::tambur_stripe_size; // default stripe size in bytes
    uint16_t w = inputV::Params::tambur_w; // Default Galois field size
    int max_data_stripes_per_frame = inputV::Params::tambur_max_data_stripes_per_frame; // Default Max data stripes per frame
    int max_fec_stripes_per_frame = inputV::Params::tambur_max_fec_stripes_per_frame; // Default Max FEC stripes per frame
    int packet_size = inputV::Params::tambur_packet_size; // Default Packet size 
    uint8_t max_qr = inputV::Params::tambur_max_qr; // Default Max QR value
    uint16_t parity_delay = inputV::Params::tambur_parity_delay; // Default Parity delay
    
    // Calculate number of frames for delay
    uint16_t num_frames = num_frames_for_delay(tau);
    uint16_t n_cols = num_frames;
    uint16_t n_rows = 256;
    while (n_rows * n_cols > 255 or (n_rows % num_frames)) {
      n_rows--;
    }

    std::string log_folder = "logs/";
    // std::vector<MetricLogger *> metricLoggersFrameGenerator;
    // TimingLogger timingLoggerFrameGenerator(log_folder + "frame_generator/");
    // metricLoggersFrameGenerator.push_back(&timingLoggerFrameGenerator);
    // Logger loggerFrameGenerator(log_folder + "frame_generator/", metricLoggersFrameGenerator);
    std::vector<MetricLogger *> metricLoggersFECSender;
    auto timingLoggerFECSender = std::make_unique<TimingLogger>(log_folder + "sender/");
    metricLoggersFECSender.push_back(timingLoggerFECSender.get());
    auto loggerFECSender = std::make_unique<Logger>(log_folder + "sender/", metricLoggersFECSender);

    // 1. Create packetization first
    printf("\t\t\t== Sender == 1. Create packetization first\n");
    auto tambur_packetization = std::make_unique<StreamingCodePacketization>(
        w, stripe_size, std::map<int, std::pair<int, int>>{{0, {0, 1}}, {max_qr, {1, 2}}, {2 * max_qr, {1, 4}}}, 
        uint16_t(1500), parity_delay);
    
    // 2. Create header coding matrix info
    printf("\t\t\t== Sender == 2. Create header coding matrix info\n");
    CodingMatrixInfo coding_matrix_info_header{n_rows, n_cols, 8};
    
    // 3. Create FEC coding matrix info
    printf("\t\t\t== Sender == 3. Create FEC coding matrix info\n");
    CodingMatrixInfo coding_matrix_info_fec{
        uint16_t(num_frames_for_delay(tau) * max_fec_stripes_per_frame),
        uint16_t(num_frames_for_delay(tau) * max_data_stripes_per_frame), 
        w};
    
    // 4. Create header block code
    printf("\t\t\t== Sender == 4. Create header block code\n");
    auto block_code_header = std::make_unique<BlockCode>(
        coding_matrix_info_header, tau, std::pair<uint16_t, uint16_t>{1, 0}, 8, false);
    
    // 5. Create FEC block code
    printf("\t\t\t== Sender == 5. Create FEC block code\n");
    auto block_code_fec = std::make_unique<BlockCode>(
        coding_matrix_info_fec, tau, std::pair<uint16_t, uint16_t>{1, 1}, uint16_t(packet_size), false, loggerFECSender.get());
    
    // 6. Create header code
    printf("\t\t\t== Sender == 6. Create header code\n");
    auto tambur_header_code = std::make_unique<MultiFECHeaderCode>(
        block_code_header.get(), tau, coding_matrix_info_header.n_rows / num_frames_for_delay(tau));
    
    // 7. Create streaming code
    printf("\t\t\t== Sender == 7. Create streaming code\n");
    auto tambur_code = std::make_unique<StreamingCode>(
        tau, stripe_size, block_code_fec.get(), w, max_data_stripes_per_frame, max_fec_stripes_per_frame);
    
    // 8. Create frame generator - 修复构造函数参数顺序
    printf("\t\t\t== Sender == 8. Create frame generator\n");
    auto tambur_frame_generator = std::make_unique<FrameGenerator>(
        tambur_code.get(), tau, tambur_packetization.get(), 
        tambur_header_code.get(), loggerFECSender.get(), 0);
    
    // 9. Create FECSender wrapper - 修复构造函数参数
    printf("\t\t\t== Sender == 9. Create FECSender wrapper\n");
    tambur_fec_sender_ = std::make_unique<FECSender>(
        *tambur_frame_generator, uint8_t(tau), loggerFECSender.get(), uint64_t(33), stripe_size * max_data_stripes_per_frame, true);
    
    // 10. Create Tambur Encoder
    printf("\t\t\t== Sender == 10. Create Tambur Encoder\n");
    tambur_encoder_ = std::make_unique<TamburEncoder>(tambur_fec_sender_.get());

    // Store Tambur FEC components
    tambur_packetization_ = std::move(tambur_packetization);
    block_code_header_ = std::move(block_code_header);
    block_code_fec_ = std::move(block_code_fec);
    tambur_header_code_ = std::move(tambur_header_code);
    tambur_code_ = std::move(tambur_code);
    tambur_frame_generator_ = std::move(tambur_frame_generator);
    loggerFECSender_ = std::move(loggerFECSender);
    timingLoggerFECSender_ = std::move(timingLoggerFECSender);
  }

  // Convert WebRTC payload to format expected by Tambur FEC
  std::vector<uint8_t> frame_data(payload.begin(), payload.end());
  size_t frame_size = frame_data.size();
  uint64_t max_frame_size_fec = tambur_fec_sender_->max_frame_size();
  uint16_t rel_num_frames = uint16_t((frame_size / max_frame_size_fec) +
                                ((frame_size % max_frame_size_fec) > 0));
  while ((frame_size / rel_num_frames +
          ((frame_size % rel_num_frames) > 0) +
          FECDatagram::VIDEO_FRAME_INFO_SIZE) > tambur_fec_sender_->max_frame_size()) {
    rel_num_frames++;
  }

  FrameType cur_frame_type = FrameType::UNKNOWN;
  if (video_header.frame_type == VideoFrameType::kVideoFrameKey) {
    cur_frame_type = FrameType::KEY;
  } else {
    cur_frame_type = FrameType::NONKEY;
  }
  
  // Generate Tambur FEC packets using FECSender
  std::vector<Datagram> tambur_packets = 
      tambur_encoder_->encode(frame_data, frame_size, cur_frame_type, rel_num_frames);

  if (tambur_packets.empty()) {
    RTC_LOG(LS_ERROR) << "Tambur FEC failed to generate packets";
    return false;
  }

  // Convert Tambur packets to WebRTC RTP packets
  std::vector<std::unique_ptr<RtpPacketToSend>> rtp_packets = 
      ConvertTamburPacketsToRtp(tambur_packets, rtp_timestamp, payload_type, 
                               video_header, csrcs);

  if (rtp_packets.empty()) {
    RTC_LOG(LS_ERROR) << "Failed to convert Tambur packets to RTP packets";
    return false;
  }

  // Send the packets
  LogAndSendToNetwork(std::move(rtp_packets), encoder_output_size);

  // Update frame tracking state
  last_rotation_ = video_header.rotation;
  if (video_header.color_space != last_color_space_) {
    last_color_space_ = video_header.color_space;
    transmit_color_space_next_frame_ = !IsBaseLayer(video_header);
  } else {
    transmit_color_space_next_frame_ =
        transmit_color_space_next_frame_ ? !IsBaseLayer(video_header) : false;
  }

  return true;
}

std::vector<std::unique_ptr<RtpPacketToSend>> 
RTPSenderVideo::ConvertTamburPacketsToRtp(
    const std::vector<Datagram>& tambur_packets,
    uint32_t rtp_timestamp,
    int payload_type,
    const RTPVideoHeader& video_header,
    const std::vector<uint32_t>& csrcs) const {
  
  std::vector<std::unique_ptr<RtpPacketToSend>> rtp_packets;
  rtp_packets.reserve(tambur_packets.size());

  for (const auto& tambur_packet : tambur_packets) {
    std::unique_ptr<RtpPacketToSend> rtp_packet = 
        rtp_sender_->AllocatePacket(csrcs);
    
    rtp_packet->SetPayloadType(payload_type);
    rtp_packet->SetTimestamp(rtp_timestamp);

    // RTP sequence number should be assigned by WebRTC RTP sender/sequencer
    // (see ModuleRtpRtcpImpl2::AssignSequenceNumber). Do not set it here.

    const bool is_first_packet = (tambur_packet.frag_id == 0);
    const bool is_last_packet = (tambur_packet.frag_id + 1 == tambur_packet.frag_cnt);

    rtp_packet->set_first_packet_of_frame(is_first_packet);

    // Set marker bit for last packet in frame.
    rtp_packet->SetMarker(is_last_packet);

    // IMPORTANT: Header extensions must be added before payload is allocated.
    // Otherwise RtpPacket::AllocateRawExtension() will fail with:
    // "Can't add new extension id X after payload was set."
    AddRtpHeaderExtensions(video_header, is_first_packet, is_last_packet,
                           rtp_packet.get());

    // Put packetization finish timestamp into extension.
    if (rtp_packet->HasExtension<VideoTimingExtension>()) {
      rtp_packet->set_packetization_finish_time(clock_->CurrentTime());
    }

    // // printf 输出 tambur_packet 的前 40 个字节，以16进制输出，例如 AA BB CC 这样，输出到一行即可: 
    // printf("\t Before ser \t : ");
    // for (size_t j = 0; j < 40 && j < tambur_packet.payload.size(); j++) {
    //   printf("%02X ", static_cast<unsigned char>(tambur_packet.payload[j]));
    // }
    // printf("    size: %lu", tambur_packet.payload.size());
    // printf("\n");
    
    // Copy serialized Tambur datagram into RTP payload.
    const std::string serialized_packet = tambur_packet.serialize_to_string();

    // // printf 输出 serialized_packet 的前 40 个字节，以16进制输出，例如 AA BB CC 这样，输出到一行即可: 
    // printf("\t After ser \t : ");
    // for (size_t j = 0; j < 40 && j < serialized_packet.size(); j++) {
    //   printf("%02X ", static_cast<unsigned char>(serialized_packet[j]));
    // }
    // printf("    size: %lu", serialized_packet.size());
    // printf("\n");

    uint8_t* dst = rtp_packet->AllocatePayload(serialized_packet.size());
    RTC_DCHECK(dst);
    memcpy(dst, serialized_packet.data(), serialized_packet.size());

    // Set packet type and retransmission permissions.
    rtp_packet->set_packet_type(RtpPacketMediaType::kVideo);
    rtp_packet->set_allow_retransmission(true);
    rtp_packet->set_is_key_frame(video_header.frame_type == VideoFrameType::kVideoFrameKey);
    
    rtp_packets.push_back(std::move(rtp_packet));
  }

  return rtp_packets;
}

}  // namespace webrtc
