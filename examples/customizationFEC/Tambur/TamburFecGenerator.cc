#include "examples/customizationFEC/Tambur/TamburFecGenerator.h"

#include <string.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <cstdio>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "examples/customizationFEC/UlpFEC/forward_error_correction.h"
#include "examples/customizationFEC/UlpFEC/forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"

// Tambur headers
// #include "examples/customizationFEC/Tambur/src/fec/fec_sender.hh"
// #include "examples/customizationFEC/Tambur/src/fec/frame_generator.hh"
// #include "examples/customizationFEC/Tambur/src/fec/logger.hh"
// #include "examples/customizationFEC/Tambur/src/fec/streaming_code/streaming_code.hh"
// #include "examples/customizationFEC/Tambur/src/fec/reed_solomon/reed_solomon_multi_frame_packetization.hh"

namespace webrtc {

namespace {

// Default parameters for Tambur FEC
constexpr uint8_t kDefaultTau = 3; // Default delay constraint
// constexpr uint16_t kDefaultStripeSize = 1024; // Default stripe size
// constexpr uint16_t kDefaultMaxFrameSize = 65535; // Default max frame size
// constexpr uint64_t kDefaultIntervalMs = 30; // Default frame interval

// uint16_t ConvertMaxFrameSize(const FecProtectionParams& params) {
//   // Convert max frame size based on FEC parameters
//   return static_cast<uint16_t>(params.max_frame_size > 0 ? 
//                                params.max_frame_size : kDefaultMaxFrameSize);
// }

}  // namespace

TamburFecGenerator::TamburFecGenerator(const Environment& env,
                                       int red_payload_type,
                                       int tambur_fec_payload_type)
    : env_(env),
      red_payload_type_(red_payload_type),
      tambur_fec_payload_type_(tambur_fec_payload_type) {
  
  // Initialize Tambur components
  // logger_ = std::make_unique<Logger>("", std::vector<MetricLogger*>{});
  
  // Initialize Tambur FEC components based on configuration
  // For now, using default parameters - these will be updated by SetProtectionParameters
  uint8_t tau = kDefaultTau;

  printf("\t\t\t====TamburFecGenerator temp set tau to %d\n", tau);
  
  // Create frame generator with default parameters
  // frame_generator_ = std::make_unique<FrameGenerator>(
  //     nullptr, tau, nullptr, nullptr, logger_.get(), 0);
  
  // Create FEC sender
  // fec_sender_ = std::make_unique<FECSender>(
  //     *frame_generator_, tau, logger_.get(), kDefaultIntervalMs, 
  //     kDefaultMaxFrameSize, false);
  
  // RTC_LOG(LS_INFO) << "TamburFecGenerator initialized with tau=" << static_cast<int>(tau);
}

TamburFecGenerator::~TamburFecGenerator() = default;

void TamburFecGenerator::SetProtectionParameters(
    const FecProtectionParams& delta_params,
    const FecProtectionParams& key_params) {
  
  current_delta_params_ = delta_params;
  current_key_params_ = key_params;
  
  // Update Tambur parameters based on WebRTC FEC parameters
  // Use keyframe parameters for stronger protection
  // uint16_t max_frame_size = ConvertMaxFrameSize(key_params);
  
  // Recreate FEC components with new parameters
  // frame_generator_ = std::make_unique<FrameGenerator>(
  //     nullptr, tau, nullptr, nullptr, logger_.get(), 0);
  
  // fec_sender_ = std::make_unique<FECSender>(
  //     *frame_generator_, tau, logger_.get(), kDefaultIntervalMs, 
  //     max_frame_size, false);
  
  // RTC_LOG(LS_INFO) << "TamburFecGenerator parameters updated: tau=" 
  //                  << static_cast<int>(tau) << ", max_frame_size=" << max_frame_size;
}

void TamburFecGenerator::AddPacketAndGenerateFec(const RtpPacketToSend& packet) {
  // Store base packet template for creating FEC packets
  if (!base_packet_template_) {
    base_packet_template_ = std::make_unique<RtpPacketToSend>(packet);
    current_frame_timestamp_ = packet.Timestamp();
    current_frame_start_seq_ = packet.SequenceNumber();
    next_fec_seq_num_ = packet.SequenceNumber();
  }
  
  // Extract video data from RTP packet
  rtc::ArrayView<const uint8_t> payload = packet.payload();
  
  // Add packet payload to current frame buffer
  current_frame_payload_.insert(current_frame_payload_.end(), 
                                payload.begin(), payload.end());
  current_frame_seq_nums_.push_back(packet.SequenceNumber());
  
  // Check if this is the last packet of the frame (marker bit set)
  if (packet.Marker()) {
    current_frame_has_marker_ = true;

    printf("\t\t\t==== We gat a whole frame! payload_size = %zu  pkt_size = %zu\n", current_frame_payload_.size(), current_frame_seq_nums_.size());   
    
    // Generate FEC for the complete frame
    // GenerateFecForCurrentFrame();
    
    // Reset frame buffer for next frame
    ResetFrameBuffer();
  }
}

void TamburFecGenerator::GenerateFecForCurrentFrame() {
  // if (current_frame_payload_.empty()) {
  //   RTC_LOG(LS_WARNING) << "Empty frame buffer, skipping FEC generation";
  //   return;
  // }
  
  // uint16_t frame_size = static_cast<uint16_t>(current_frame_payload_.size());
  
  // // Generate FEC packets using Tambur
  // try {
  //   auto fec_packets = fec_sender_->next_frame(frame_size, true);
    
  //   // Create data packets from the frame (split back into RTP packets)
  //   CreateDataPacketsFromFrame();
    
  //   // Create FEC packets
  //   CreateFecPackets(fec_packets);
    
  //   total_fec_bytes_generated_ += frame_size * fec_packets.size();
  //   last_fec_generation_time_ = env_.clock().CurrentTime();
    
  //   RTC_LOG(LS_INFO) << "Generated " << fec_packets.size() 
  //                    << " FEC packets for frame of size " << frame_size
  //                    << " bytes";
    
  // } catch (const std::exception& e) {
  //   RTC_LOG(LS_ERROR) << "Tambur FEC generation failed: " << e.what();
  // }
}

void TamburFecGenerator::CreateDataPacketsFromFrame() {
  // if (current_frame_payload_.empty() || current_frame_seq_nums_.empty()) {
  //   return;
  // }
  
  // // Calculate average packet size for this frame
  // size_t total_payload_size = current_frame_payload_.size();
  // size_t num_packets = current_frame_seq_nums_.size();
  // size_t avg_packet_size = total_payload_size / num_packets;
  
  // // Split frame payload back into individual RTP packets
  // size_t payload_offset = 0;
  // for (size_t i = 0; i < num_packets; i++) {
  //   size_t packet_size = (i == num_packets - 1) ? 
  //                        (total_payload_size - payload_offset) : avg_packet_size;
    
  //   if (payload_offset + packet_size > total_payload_size) {
  //     packet_size = total_payload_size - payload_offset;
  //   }
    
  //   auto rtp_packet = std::make_unique<RtpPacketToSend>(base_packet_template_.get());
    
  //   // Set sequence number and timestamp
  //   rtp_packet->SetSequenceNumber(current_frame_seq_nums_[i]);
  //   rtp_packet->SetTimestamp(current_frame_timestamp_);
    
  //   // Set marker bit for last packet
  //   rtp_packet->SetMarker(i == num_packets - 1);
    
  //   // Set payload
  //   rtp_packet->SetPayload(rtc::MakeArrayView(
  //       current_frame_payload_.data() + payload_offset, packet_size));
    
  //   // Add to FEC packets vector (these are the recovered data packets)
  //   fec_packets_.push_back(std::move(rtp_packet));
    
  //   payload_offset += packet_size;
  // }
}

// void TamburFecGenerator::CreateFecPackets(const std::vector<FECDatagram>& fec_packets) {
  // for (const auto& tambur_packet : fec_packets) {
  //   auto rtp_packet = std::make_unique<RtpPacketToSend>(base_packet_template_.get());
    
  //   // Set FEC payload type
  //   rtp_packet->SetPayloadType(tambur_fec_payload_type_);
    
  //   // Convert Tambur packet data to RTP payload
  //   std::string tambur_data = tambur_packet.serialize_to_string();
  //   rtp_packet->SetPayload(rtc::MakeArrayView(
  //       reinterpret_cast<const uint8_t*>(tambur_data.data()), 
  //       tambur_data.size()));
    
  //   // Update sequence number and timestamp
  //   rtp_packet->SetSequenceNumber(next_fec_seq_num_++);
  //   rtp_packet->SetTimestamp(current_frame_timestamp_);
    
  //   // FEC packets don't have marker bit set
  //   rtp_packet->SetMarker(false);
    
  //   fec_packets_.push_back(std::move(rtp_packet));
  // }
// }

void TamburFecGenerator::ResetFrameBuffer() {
  current_frame_payload_.clear();
  current_frame_seq_nums_.clear();
  current_frame_has_marker_ = false;
  base_packet_template_.reset();
}

size_t TamburFecGenerator::MaxPacketOverhead() const {
  // Estimate maximum overhead for Tambur FEC
  // This includes FEC headers and potential encoding overhead
  return 100; // Conservative estimate
}

std::vector<std::unique_ptr<RtpPacketToSend>> TamburFecGenerator::GetFecPackets() {
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  packets.swap(fec_packets_);
  return packets;
}

DataRate TamburFecGenerator::CurrentFecRate() const {
  if (last_fec_generation_time_.IsMinusInfinity()) {
    return DataRate::Zero();
  }
  
  TimeDelta time_since_last_fec = env_.clock().CurrentTime() - last_fec_generation_time_;
  if (time_since_last_fec.IsZero()) {
    return DataRate::Zero();
  }
  
  // Calculate FEC rate based on generated bytes and time
  return DataRate::BytesPerSec(total_fec_bytes_generated_ / 
                              time_since_last_fec.seconds<double>());
}

// std::unique_ptr<RtpPacketToSend> TamburFecReceiver::ConvertTamburToRtpPacket(
//     const std::string& frame_data, uint16_t frame_num) {
  
//   try {
//     auto packet = std::make_unique<RtpPacketToSend>();
    
//     // Set basic RTP header fields
//     packet->SetSsrc(ssrc_);
//     packet->SetPayloadType(tambur_fec_payload_type_);
    
//     // Set sequence number and timestamp based on frame number
//     // This is a simplified mapping - real implementation would need proper mapping
//     packet->SetSequenceNumber(static_cast<uint16_t>(frame_num));
//     packet->SetTimestamp(static_cast<uint32_t>(frame_num * 3000)); // Approximate timestamp
    
//     // Set payload data
//     packet->SetPayload(rtc::MakeArrayView(
//         reinterpret_cast<const uint8_t*>(frame_data.data()), 
//         frame_data.size()));
    
//     return packet;
    
//   } catch (const std::exception& e) {
//     RTC_LOG(LS_ERROR) << "Failed to convert Tambur frame to RTP packet: " << e.what();
//     return nullptr;
//   }
// }

}  // namespace webrtc