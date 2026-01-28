#include "examples/customizationFEC/Tambur/TamburFecReceiver.h"

#include <memory>
#include <utility>
#include <iostream>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"

// Tambur headers
// #include "examples/customizationFEC/Tambur/tambur/src/fec/fec_multi_receiver.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/code.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/header_code.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/quality_reporter.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/logger.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/streaming_code/streaming_code.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/streaming_code/multi_fec_header_code.hh"

namespace webrtc {

namespace {

// Default parameters for Tambur FEC receiver
constexpr uint8_t kDefaultTau = 3; // Default delay constraint
// constexpr uint8_t kDefaultMemory = 7; // Default memory size (2*tau+1)
// constexpr uint16_t kDefaultStripeSize = 1024; // Default stripe size
// constexpr uint64_t kDefaultIntervalMs = 2000; // Default feedback interval

// // Helper function to extract timestamp from RTP packet
// uint32_t ExtractTimestamp(const RtpPacketReceived& packet) {
//   return packet.Timestamp();
// }

// // Helper function to extract sequence number from RTP packet
// uint16_t ExtractSequenceNumber(const RtpPacketReceived& packet) {
//   return packet.SequenceNumber();
// }

}  // namespace

TamburFecReceiver::TamburFecReceiver(uint32_t ssrc,
                                       int tambur_fec_payload_type,
                                       RecoveredPacketReceiver* callback,
                                       Clock* clock)
    : ssrc_(ssrc),
      tambur_fec_payload_type_(tambur_fec_payload_type),
      clock_(clock) {
  
  // Initialize Tambur components
  // logger_ = std::make_unique<Logger>("", std::vector<MetricLogger*>{});
  
  // Initialize Tambur FEC code
  // Using default parameters for now
  uint8_t tau = kDefaultTau;
  // uint8_t memory = kDefaultMemory;
  
  // Create code instance
  // CodingMatrixInfo coding_matrix_info{uint16_t(memory * 2), uint16_t(memory * 3), 8};
  // code_ = std::make_unique<StreamingCode>(tau, kDefaultStripeSize, nullptr, 8, 2, 1);
  
  // Create header code
  // header_code_ = std::make_unique<MultiFECHeaderCode>(nullptr, tau, 1);
  
  // Create quality reporter
  // quality_reporter_ = std::make_unique<QualityReporter>();
  
  // Create FEC receiver
  // fec_receiver_ = std::make_unique<FECMultiReceiver>(
  //     code_.get(), tau, memory, header_code_.get(), 
  //     quality_reporter_.get(), logger_.get(), kDefaultIntervalMs, false, false);
  
  // Initialize packet counter
  packet_counter_.first_packet_time = Timestamp::MinusInfinity();

  printf("\t\t\t====TamburFecReceiver temp set tau=%d\n", tau);
  
  RTC_LOG(LS_INFO) << "TamburFecReceiver initialized for SSRC=" << ssrc
                   << ", tau=" << static_cast<int>(tau);
}

TamburFecReceiver::~TamburFecReceiver() = default;

bool TamburFecReceiver::AddReceivedRedPacket(const RtpPacketReceived& rtp_packet) {


  if (rtp_packet.Ssrc() != ssrc_) {
    RTC_LOG(LS_WARNING)
        << "Received RED packet with different SSRC than expected; dropping.";
    return false;
  }
  if (rtp_packet.size() > IP_PACKET_SIZE) {
    RTC_LOG(LS_WARNING) << "Received RED packet with length exceeds maximum IP "
                           "packet size; dropping.";
    return false;
  }

  // static constexpr uint8_t kRedHeaderLength = 1;

  if (rtp_packet.payload_size() == 0) {
    RTC_LOG(LS_WARNING) << "Corrupt/truncated FEC packet.";
    return false;
  }

  // Update packet statistics
  if (packet_counter_.first_packet_time.IsMinusInfinity()) {
    packet_counter_.first_packet_time = clock_->CurrentTime();
  }
  
  packet_counter_.num_packets++;
  packet_counter_.num_bytes += rtp_packet.payload_size();
  
  // Check if this is a FEC packet
  if (rtp_packet.PayloadType() == tambur_fec_payload_type_) {
    packet_counter_.num_fec_packets++;
    printf("\t\t====Received Tambur FEC packet\n");
  }
  
  return true;

  // // Convert WebRTC RTP packet to Tambur datagram format
  // std::string tambur_datagram = ConvertRtpToTamburDatagram(rtp_packet);
  
  // if (tambur_datagram.empty()) {
  //   RTC_LOG(LS_WARNING) << "Failed to convert RTP packet to Tambur datagram";
  //   return false;
  // }
  
  // // Process the packet using Tambur FEC receiver
  // try {
  //   bool success = fec_receiver_->receive_pkt(tambur_datagram);
    
  //   if (success) {
  //     RTC_LOG(LS_VERBOSE) << "Successfully processed Tambur FEC packet, seq="
  //                        << rtp_packet.SequenceNumber();
  //   }
    
  //   return success;
    
  // } catch (const std::exception& e) {
  //   RTC_LOG(LS_ERROR) << "Tambur FEC packet processing failed: " << e.what();
  //   return false;
  // }
}

void TamburFecReceiver::ProcessReceivedFec() {
  // Tambur FEC receiver processes packets immediately in receive_pkt()
  // This method is called to trigger any deferred processing
  
  // try {
  //   // Check if we have recovered frames
  //   auto recovered_frames = fec_receiver_->recovered_video_frames();
    
  //   for (const auto& frame_info : recovered_frames) {
  //     uint16_t frame_num = frame_info.first;
  //     uint32_t video_frame_num = frame_info.second;
      
  //     // Get recovered frame data from Tambur
  //     std::string frame_data = fec_receiver_->recovered_frame(frame_num);
      
  //     if (!frame_data.empty()) {
  //       // Split recovered frame into multiple RTP packets
  //       auto recovered_packets = SplitFrameToRtpPackets(frame_data, frame_num, video_frame_num);
        
  //       for (auto& packet : recovered_packets) {
  //         recovered_packets_.push_back(std::move(packet));
  //         packet_counter_.num_recovered_packets++;
  //       }
        
  //       RTC_LOG(LS_INFO) << "Recovered frame " << frame_num 
  //                        << " (video frame " << video_frame_num << ") into "
  //                        << recovered_packets.size() << " RTP packets";
  //     }
  //   }
    
  // } catch (const std::exception& e) {
  //   RTC_LOG(LS_ERROR) << "Tambur FEC processing failed: " << e.what();
  // }
}

std::vector<std::unique_ptr<RtpPacketReceived>> TamburFecReceiver::SplitFrameToRtpPackets(
    const std::string& frame_data, uint16_t frame_num, uint32_t video_frame_num) {
  
  std::vector<std::unique_ptr<RtpPacketReceived>> packets;
  
  if (frame_data.empty()) {
    return packets;
  }
  
  // Calculate optimal packet sizes (similar to what encoder would use)
  size_t frame_size = frame_data.size();
  size_t max_packet_size = 1200; // Typical MTU size minus headers
  size_t min_packet_size = 500;  // Minimum reasonable packet size
  
  // Calculate number of packets needed
  size_t num_packets = std::max<size_t>(1, frame_size / max_packet_size);
  if (frame_size % max_packet_size != 0) {
    num_packets++;
  }
  
  // Ensure we don't have too many small packets
  if (num_packets > 1 && frame_size / num_packets < min_packet_size) {
    num_packets = std::max<size_t>(1, frame_size / min_packet_size);
  }
  
  // Calculate packet sizes
  std::vector<size_t> packet_sizes(num_packets, frame_size / num_packets);
  size_t remaining_bytes = frame_size % num_packets;
  for (size_t i = 0; i < remaining_bytes; i++) {
    packet_sizes[i]++;
  }
  
  // Create RTP packets
  size_t data_offset = 0;
  uint16_t base_seq_num = static_cast<uint16_t>(frame_num * 100); // Simple mapping
  
  for (size_t i = 0; i < num_packets; i++) {
    auto packet = std::make_unique<RtpPacketReceived>();
    
    // Set basic RTP header fields
    packet->SetSsrc(ssrc_);
    packet->SetPayloadType(tambur_fec_payload_type_);
    
    // Set sequence number and timestamp
    packet->SetSequenceNumber(base_seq_num + static_cast<uint16_t>(i));
    packet->SetTimestamp(static_cast<uint32_t>(frame_num * 3000)); // Approximate timestamp
    
    // Set marker bit for last packet
    packet->SetMarker(i == num_packets - 1);
    
    // Set payload data
    packet->Parse(rtc::MakeArrayView(
        reinterpret_cast<const uint8_t*>(frame_data.data() + data_offset), 
        packet_sizes[i]));
    
    packets.push_back(std::move(packet));
    data_offset += packet_sizes[i];
  }
  
  return packets;
}

FecPacketCounter TamburFecReceiver::GetPacketCounter() const {
  return packet_counter_;
}

std::string TamburFecReceiver::ConvertRtpToTamburDatagram(const RtpPacketReceived& rtp_packet) {
  // Convert WebRTC RTP packet to Tambur FEC datagram format
  // This is a simplified conversion - in practice, more complex mapping is needed

  // Extract packet data
  rtc::ArrayView<const uint8_t> payload = rtp_packet.payload();
  uint32_t timestamp = rtp_packet.Timestamp();
  uint16_t sequence_number = rtp_packet.SequenceNumber();
  
  // Create a simple Tambur datagram structure
  // In real implementation, this would need to match Tambur's expected format
  std::string datagram;
  
  // Add header information
  datagram.append(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
  datagram.append(reinterpret_cast<const char*>(&sequence_number), sizeof(sequence_number));
  
  // Add payload
  datagram.append(reinterpret_cast<const char*>(payload.data()), payload.size());
  
  return datagram;
}

}  // namespace webrtc