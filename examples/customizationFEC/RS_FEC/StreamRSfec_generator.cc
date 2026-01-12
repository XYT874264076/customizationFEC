#include "examples/customizationFEC/RS_FEC/StreamRSfec_generator.h"

#include <string.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <cstdio>
#include <iostream>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "examples/customizationFEC/RS_FEC/RS_forward_error_correction.h"
#include "examples/customizationFEC/RS_FEC/RS_forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/mod_ops.h"
#include "rtc_base/synchronization/mutex.h"
#include "examples/customizationFEC/Params.h"

namespace webrtc {

namespace {

constexpr size_t kRedForFecHeaderLength = 1;

constexpr int kMaxFECSeqNumDiff = 32;

// This controls the maximum amount of excess overhead (actual - target)
// allowed in order to trigger EncodeFec(), before `params_.max_fec_frames`
// is reached. Overhead here is defined as relative to number of media packets.
constexpr int kMaxExcessOverhead = 50;  // Q8.

// This is the minimum number of media packets required (above some protection
// level) in order to trigger EncodeFec(), before `params_.max_fec_frames` is
// reached.
constexpr size_t kMinMediaPackets = 4;

// Threshold on the received FEC protection level, above which we enforce at
// least `kMinMediaPackets` packets for the FEC code. Below this
// threshold `kMinMediaPackets` is set to default value of 1.
//
// The range is between 0 and 255, where 255 corresponds to 100% overhead
// (relative to the number of protected media packets).
constexpr uint8_t kHighProtectionThreshold = 80;

// This threshold is used to adapt the `kMinMediaPackets` threshold, based
// on the average number of packets per frame seen so far. When there are few
// packets per frame (as given by this threshold), at least
// `kMinMediaPackets` + 1 packets are sent to the FEC code.
constexpr float kMinMediaPacketsAdaptationThreshold = 2.0f;

// At construction time, we don't know the SSRC that is used for the generated
// FEC packets, but we still need to give it to the ForwardErrorCorrection ctor
// to be used in the decoding.
// TODO(brandtr): Get rid of this awkwardness by splitting
// ForwardErrorCorrection in two objects -- one encoder and one decoder.
constexpr uint32_t kUnknownSsrc = 0;

} // namespace 

//
// Constructors and destructors
//

StreamRSfecGenerator::Params::Params() = default;
StreamRSfecGenerator::Params::Params(FecProtectionParams delta_params,
                               FecProtectionParams keyframe_params)
    : delta_params(delta_params), keyframe_params(keyframe_params) {}

StreamRSfecGenerator::StreamRSfecGenerator(const Environment& env,
                                            int red_payload_type,
                                            int RSfec_payload_type)
    : env_(env),
      red_payload_type_(red_payload_type),
      RSfec_payload_type_(RSfec_payload_type),
      fec_(RSForwardErrorCorrection::CreateRSFec(kUnknownSsrc, nullptr)),
      num_protected_frames_(0),
      min_num_media_packets_(1),
      media_contains_keyframe_(0),
      fec_bitrate_(/*max_window_size=*/TimeDelta::Seconds(1)) {}

// Used by FlexFecSender, payload types are unused.
StreamRSfecGenerator::StreamRSfecGenerator(const Environment& env,
                                            std::unique_ptr<RSForwardErrorCorrection> fec)
    : env_(env),
      red_payload_type_(0),
      RSfec_payload_type_(0),
      fec_(std::move(fec)),
      num_protected_frames_(0),
      min_num_media_packets_(1),
      num_media_packets_insert_(0),
      left_rate_(0),
      media_contains_keyframe_(0),
      fec_bitrate_(/*max_window_size=*/TimeDelta::Seconds(1)) {}

StreamRSfecGenerator::~StreamRSfecGenerator() = default;

void StreamRSfecGenerator::SetProtectionParameters(
    const FecProtectionParams& delta_params,
    const FecProtectionParams& key_params) {
  RTC_DCHECK_GE(delta_params.fec_rate, 0);
  RTC_DCHECK_LE(delta_params.fec_rate, 255);
  RTC_DCHECK_GE(key_params.fec_rate, 0);
  RTC_DCHECK_LE(key_params.fec_rate, 255);
  // Store the new params and apply them for the next set of FEC packets being
  // produced.
  MutexLock lock(&mutex_);
  pending_params_.emplace(delta_params, key_params);
}

void StreamRSfecGenerator::AddPacketAndGenerateFec(const RtpPacketToSend& packet) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  RTC_DCHECK(generated_fec_packets_.empty());

  if (inputV::Params::type == inputV::ExpType::RSFECStreamStableRate) {

    while (media_packets_.size() > 0 && media_packets_.size() >= transV::Params::L) {
      media_packets_.pop_front();
    }
    const bool complete_frame = packet.Marker();
    if (media_packets_.size() < transV::Params::L) {
        // Our packet masks can only protect up to `kUlpfecMaxMediaPackets` packets.
        auto fec_packet = std::make_unique<RSForwardErrorCorrection::Packet>();
        fec_packet->data = packet.Buffer();
        uint16_t insert_seq_num = RSForwardErrorCorrection::ParseSequenceNumber(fec_packet->data.data()); 
        media_packets_.push_back(std::move(fec_packet));
        num_media_packets_insert_++;

        auto media_pkt_it = media_packets_.cbegin();
        while (media_pkt_it != media_packets_.end()) {
            uint16_t front_seq_num = RSForwardErrorCorrection::ParseSequenceNumber((*media_pkt_it)->data.data());
            uint16_t seq_num_diff = MinDiff(insert_seq_num, front_seq_num);
            if (seq_num_diff>=kMaxFECSeqNumDiff) {
                media_pkt_it = media_packets_.erase(media_pkt_it);
            }
            else {
                break;
            }
        }

        // Keep a copy of the last RTP packet, so we can copy the RTP header
        // from it when creating newly generated ULPFEC+RED packets.
        RTC_DCHECK_GE(packet.headers_size(), kRtpHeaderSize);
        last_media_packet_ = packet;
    }

    // std::cout<<"Here is RSFECStreamStableRate"<<std::endl;

    if (num_media_packets_insert_ >= static_cast<int>(media_packets_.size())) {
        std::cout<<"Generate RSFECStreamStableRate: "<<left_rate_<<" "<<inputV::Params::FecRate<<" "<<num_media_packets_insert_<<" "<<media_packets_.size()<<std::endl;

        // std::cout<<"==Run fec_->EncodeRSFec() to Generate FEC Packets"<<std::endl;

        int ret = fec_->EncodeRSFec(media_packets_, 0, &generated_fec_packets_);
        if (generated_fec_packets_.empty() || ret == -1) {
            
            // std::cout<<"Generate FEC Error!"<<std::endl;

            ResetState();

            return;
        }
        left_rate_ = 0;
        num_media_packets_insert_ = 0;
    }

    else if (num_media_packets_insert_ > 0 && left_rate_ + inputV::Params::FecRate*num_media_packets_insert_ >= inputV::Params::generate_fec_num && complete_frame) {
        
        std::cout<<"Generate RSFECStreamStableRate: "<<left_rate_<<" "<<inputV::Params::FecRate<<" "<<num_media_packets_insert_<<" "<<media_packets_.size()<<std::endl;

        // std::cout<<"==Run fec_->EncodeRSFec() to Generate FEC Packets"<<std::endl;

        int ret = fec_->EncodeRSFec(media_packets_, 0, &generated_fec_packets_);
        if (generated_fec_packets_.empty() || ret == -1) {
            
            // std::cout<<"Generate FEC Error!"<<std::endl;

            ResetState();

            return;
        }
        left_rate_ = left_rate_ + inputV::Params::FecRate*num_media_packets_insert_ - inputV::Params::generate_fec_num;
        if (left_rate_>1) left_rate_ = 1;
        num_media_packets_insert_ = 0;

        // std::cout<<"==After generate: generated_fec_packets_.size() = "<<generated_fec_packets_.size()<<std::endl;

        // if (generated_fec_packets_.size() > 0) {
        //     std::cout<<"====== We Generate "<<generated_fec_packets_.size()<<" packets!"<<std::endl;
        //     std::cout<<"To protect packets:"<<std::endl;
        //     std::cout<<"\t";
        //     auto media_pkt_it = media_packets_.cbegin();
        //     while (media_pkt_it != media_packets_.end()) {
        //         std::cout<<RSForwardErrorCorrection::ParseSequenceNumber((*media_pkt_it)->data.data())<<" ";
        //         media_pkt_it++;
        //     }
        //     std::cout<<std::endl;

        //     int ii=0;
        //     auto fec_pkt_it = generated_fec_packets_.cbegin();
        //     while (fec_pkt_it != generated_fec_packets_.end()) {
        //         std::cout<<"The first 30 bytes of fec pkt "<<ii<<" is:"<<std::endl;
        //         for (int jj=0;jj<30;jj++) {
        //             std::cout<<(int)(*fec_pkt_it)->data[jj]<<" ";
        //         }
        //         std::cout<<std::endl;
        //         ii++;
        //         fec_pkt_it++;
        //     }
        // }
    }


  }
  else if (inputV::Params::type == inputV::ExpType::RLSRSFEC) {
    // std::cout << "Now we run RLSRSFEC!" << std::endl;

    // const bool complete_frame = packet.Marker();

    while (media_packets_.size() > 0 && media_packets_.size() >= transV::Params::L) {
      media_packets_.pop_front();
    }

    if (media_packets_.size() < transV::Params::L) {
      // Our packet masks can only protect up to `kUlpfecMaxMediaPackets` packets.
      auto fec_packet = std::make_unique<RSForwardErrorCorrection::Packet>();
      fec_packet->data = packet.Buffer();
      uint16_t insert_seq_num = RSForwardErrorCorrection::ParseSequenceNumber(fec_packet->data.data()); 
      media_packets_.push_back(std::move(fec_packet));
      num_media_packets_insert_++;

      // To ensure the different of seq_num is smaller than mask length.
      auto media_pkt_it = media_packets_.cbegin();
      while (media_pkt_it != media_packets_.end()) {
          uint16_t front_seq_num = RSForwardErrorCorrection::ParseSequenceNumber((*media_pkt_it)->data.data());
          uint16_t seq_num_diff = MinDiff(insert_seq_num, front_seq_num);
          if (seq_num_diff>=kMaxFECSeqNumDiff) {
              media_pkt_it = media_packets_.erase(media_pkt_it);
          }
          else {
              break;
          }
      }

      // Keep a copy of the last RTP packet, so we can copy the RTP header
      // from it when creating newly generated ULPFEC+RED packets.
      RTC_DCHECK_GE(packet.headers_size(), kRtpHeaderSize);
      last_media_packet_ = packet;
    }

    if (media_packets_.size() > 0 && num_media_packets_insert_ >= static_cast<int>(transV::Params::I)) {
      // std::cout<<"==Run fec_->EncodeRSFec() to Generate FEC Packets"<<std::endl;

      int ret = fec_->EncodeRSFec(media_packets_, 0, &generated_fec_packets_);
      if (generated_fec_packets_.empty() || ret == -1) {
          
          // std::cout<<"Generate FEC Error!"<<std::endl;

          ResetState();

          return;
      }

      // num_media_packets_insert_ = num_media_packets_insert_ - static_cast<int>(transV::Params::I);
      num_media_packets_insert_ = 0;

      // std::cout<<"==After generate: generated_fec_packets_.size() = "<<generated_fec_packets_.size()<<std::endl;

      // if (generated_fec_packets_.size() > 0) {
      //     std::cout<<"====== We Generate "<<generated_fec_packets_.size()<<" packets!"<<std::endl;
      //     std::cout<<"To protect packets:"<<std::endl;
      //     std::cout<<"\t";
      //     auto media_pkt_it = media_packets_.cbegin();
      //     while (media_pkt_it != media_packets_.end()) {
      //         std::cout<<RSForwardErrorCorrection::ParseSequenceNumber((*media_pkt_it)->data.data())<<" ";
      //         media_pkt_it++;
      //     }
      //     std::cout<<std::endl;

      //     int ii=0;
      //     auto fec_pkt_it = generated_fec_packets_.cbegin();
      //     while (fec_pkt_it != generated_fec_packets_.end()) {
      //         std::cout<<"The first 30 bytes of fec pkt "<<ii<<" is:"<<std::endl;
      //         for (int jj=0;jj<30;jj++) {
      //             std::cout<<(int)(*fec_pkt_it)->data[jj]<<" ";
      //         }
      //         std::cout<<std::endl;
      //         ii++;
      //         fec_pkt_it++;
      //     }
      // }
    }

  }
  else if (inputV::Params::type == inputV::ExpType::RSFECStreamSourceRate) {
    {
        MutexLock lock(&mutex_);
        if (pending_params_) {
            current_params_ = *pending_params_;
            pending_params_.reset();

            if (CurrentParams().fec_rate > kHighProtectionThreshold) {
                min_num_media_packets_ = kMinMediaPackets;
            } else {
                min_num_media_packets_ = 1;
            }
        }
    }

    // if (packet.is_key_frame()) {
    //     media_contains_keyframe_ += 1;
    // }
    const bool complete_frame = packet.Marker();
    if (media_packets_.size() < kRSfecMaxMediaPackets) {
        // Our packet masks can only protect up to `kUlpfecMaxMediaPackets` packets.
        auto fec_packet = std::make_unique<RSForwardErrorCorrection::Packet>();
        fec_packet->data = packet.Buffer();
        uint16_t insert_seq_num = RSForwardErrorCorrection::ParseSequenceNumber(fec_packet->data.data());
        media_packets_.push_back(std::move(fec_packet));
        if (packet.is_key_frame()){
            media_packets_key_frame_note_.push_back(1);
        }
        else {
            media_packets_key_frame_note_.push_back(0);
        }
         
        auto media_pkt_it = media_packets_.cbegin();
        auto media_key_frame_note_it = media_packets_key_frame_note_.cbegin();
        while (media_pkt_it != media_packets_.end()) {
            uint16_t front_seq_num = RSForwardErrorCorrection::ParseSequenceNumber((*media_pkt_it)->data.data());
            uint16_t seq_num_diff = MinDiff(insert_seq_num, front_seq_num);
            if (seq_num_diff>=32) {
                // media_contains_keyframe_ -= *media_key_frame_note_it;
                media_key_frame_note_it = media_packets_key_frame_note_.erase(media_key_frame_note_it);
                media_pkt_it = media_packets_.erase(media_pkt_it);
            }
            else {
                break;
            }
        }

        // Keep a copy of the last RTP packet, so we can copy the RTP header
        // from it when creating newly generated ULPFEC+RED packets.
        RTC_DCHECK_GE(packet.headers_size(), kRtpHeaderSize);
        last_media_packet_ = packet;
    }

    if (complete_frame) {
        ++num_protected_frames_;
    }

    auto params = CurrentParams();

    // Produce FEC over at most `params_.max_fec_frames` frames, or as soon as:
    // (1) the excess overhead (actual overhead - requested/target overhead) is
    // less than `kMaxExcessOverhead`, and
    // (2) at least `min_num_media_packets_` media packets is reached.
    if (complete_frame && (num_protected_frames_ >= params.max_fec_frames ||
        (ExcessOverheadBelowMax() && MinimumMediaPacketsReached()))) {
        // We are not using Unequal Protection feature of the parity erasure code.

        // std::cout<<"==Run fec_->EncodeRSFec() to Generate FEC Packets"<<std::endl;

        int ret = fec_->EncodeRSFec(media_packets_, params.fec_rate, &generated_fec_packets_);
        if (generated_fec_packets_.empty() || ret == -1) {
            ResetState();
        }

        // std::cout<<"==After generate: generated_fec_packets_.size() = "<<generated_fec_packets_.size()<<std::endl;

        // if (generated_fec_packets_.size() > 0) {
        //     std::cout<<"====== We Generate "<<generated_fec_packets_.size()<<" packets!"<<std::endl;
        //     std::cout<<"To protect packets:"<<std::endl;
        //     std::cout<<"\t";
        //     auto media_pkt_it = media_packets_.cbegin();
        //     while (media_pkt_it != media_packets_.end()) {
        //         std::cout<<RSForwardErrorCorrection::ParseSequenceNumber((*media_pkt_it)->data.data())<<" ";
        //         media_pkt_it++;
        //     }
        //     std::cout<<std::endl;

        //     int ii=0;
        //     auto fec_pkt_it = generated_fec_packets_.cbegin();
        //     while (fec_pkt_it != generated_fec_packets_.end()) {
        //         std::cout<<"The first 30 bytes of fec pkt "<<ii<<" is:"<<std::endl;
        //         for (int jj=0;jj<30;jj++) {
        //         std::cout<<(int)(*fec_pkt_it)->data[jj]<<" ";
        //         }
        //         std::cout<<std::endl;
        //         ii++;
        //         fec_pkt_it++;
        //     }
        // }
    }
  }
  
}

bool StreamRSfecGenerator::ExcessOverheadBelowMax() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  return ((Overhead() - CurrentParams().fec_rate) < kMaxExcessOverhead);
}

bool StreamRSfecGenerator::MinimumMediaPacketsReached() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  float average_num_packets_per_frame =
      static_cast<float>(media_packets_.size()) / num_protected_frames_;
  int num_media_packets = static_cast<int>(media_packets_.size());
  if (average_num_packets_per_frame < kMinMediaPacketsAdaptationThreshold) {
    return num_media_packets >= min_num_media_packets_;
  } else {
    // For larger rates (more packets/frame), increase the threshold.
    // TODO(brandtr): Investigate what impact this adaptation has.
    return num_media_packets >= min_num_media_packets_ + 1;
  }
}

const FecProtectionParams& StreamRSfecGenerator::CurrentParams() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return media_contains_keyframe_ ? current_params_.keyframe_params
                                  : current_params_.delta_params;
}

size_t StreamRSfecGenerator::MaxPacketOverhead() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return fec_->MaxPacketOverhead();
}

std::vector<std::unique_ptr<RtpPacketToSend>> StreamRSfecGenerator::GetFecPackets() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  if (generated_fec_packets_.empty()) {
    return std::vector<std::unique_ptr<RtpPacketToSend>>();
  }

  // Wrap FEC packet (including FEC headers) in a RED packet. Since the
  // FEC packets in `generated_fec_packets_` don't have RTP headers, we
  // reuse the header from the last media packet.
  RTC_CHECK(last_media_packet_.has_value());
  last_media_packet_->SetPayloadSize(0);

  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets;
  fec_packets.reserve(generated_fec_packets_.size());

  size_t total_fec_size_bytes = 0;
  for (const auto* fec_packet : generated_fec_packets_) {
    std::unique_ptr<RtpPacketToSend> red_packet =
        std::make_unique<RtpPacketToSend>(*last_media_packet_);
    red_packet->SetPayloadType(red_payload_type_);
    red_packet->SetMarker(false);
    uint8_t* payload_buffer = red_packet->SetPayloadSize(
        kRedForFecHeaderLength + fec_packet->data.size());
    // Primary RED header with F bit unset.
    // See https://tools.ietf.org/html/rfc2198#section-3
    payload_buffer[0] = RSfec_payload_type_;  // RED header.
    memcpy(&payload_buffer[1], fec_packet->data.data(),
           fec_packet->data.size());
    total_fec_size_bytes += red_packet->size();
    red_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
    red_packet->set_allow_retransmission(false);
    red_packet->set_is_red(true);
    red_packet->set_fec_protect_packet(false);
    fec_packets.push_back(std::move(red_packet));
  }

  MutexLock lock(&mutex_);
  fec_bitrate_.Update(total_fec_size_bytes, env_.clock().CurrentTime());

  generated_fec_packets_.clear();

  return fec_packets;
}

DataRate StreamRSfecGenerator::CurrentFecRate() const {
  MutexLock lock(&mutex_);
  return fec_bitrate_.Rate(env_.clock().CurrentTime())
      .value_or(DataRate::Zero());
}

int StreamRSfecGenerator::Overhead() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  RTC_DCHECK(!media_packets_.empty());
  int num_fec_packets =
      fec_->NumFecPackets(media_packets_.size(), CurrentParams().fec_rate);

  // Return the overhead in Q8.
  return (num_fec_packets << 8) / media_packets_.size();
}

void StreamRSfecGenerator::ResetState() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  media_packets_.clear();
  media_packets_key_frame_note_.clear();
  last_media_packet_.reset();
  generated_fec_packets_.clear();
  num_protected_frames_ = 0;
  num_media_packets_insert_ = 0;
  media_contains_keyframe_ = 0;
}

}    // namespace webrtc