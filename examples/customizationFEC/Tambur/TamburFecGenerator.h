#ifndef EXAMPLES_CUSTOMIZATIONFEC_TAMBUR_TAMBURFECGENERATOR_H_
#define EXAMPLES_CUSTOMIZATIONFEC_TAMBUR_TAMBURFECGENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "modules/include/module_fec_types.h"
#include "examples/customizationFEC/video_fec_generator.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"

// Tambur headers
// #include "examples/customizationFEC/Tambur/src/fec/fec_sender.hh"
// #include "examples/customizationFEC/Tambur/src/fec/frame_generator.hh"
// #include "examples/customizationFEC/Tambur/src/fec/logger.hh"

namespace webrtc {

class TamburFecGenerator : public VideoFecGenerator {
 public:
  TamburFecGenerator(const Environment& env,
                     int red_payload_type,
                     int tambur_fec_payload_type);
  ~TamburFecGenerator() override;

  FecType GetFecType() const override {
    return VideoFecGenerator::FecType::kUlpFec; // Using ULPFEC as base type
  }

  std::optional<uint32_t> FecSsrc() override { return std::nullopt; }

  void SetProtectionParameters(const FecProtectionParams& delta_params,
                               const FecProtectionParams& key_params) override;

  void AddPacketAndGenerateFec(const RtpPacketToSend& packet) override;

  size_t MaxPacketOverhead() const override;

  std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() override;

  DataRate CurrentFecRate() const override;

  std::optional<RtpState> GetRtpState() override { return std::nullopt; }

  const FecProtectionParams& CurrentParams() const;

 private:

  void GenerateFecForCurrentFrame();
  void CreateDataPacketsFromFrame();
  void ResetFrameBuffer();

  // void CreateFecPackets(const std::vector<FECDatagram>& fec_packets);

  const Environment& env_;
  const int red_payload_type_;
  const int tambur_fec_payload_type_;

  // Convert Tambur recovered frame to WebRTC RTP packet
  // std::unique_ptr<RtpPacketToSend> ConvertTamburToRtpPacket(
  //     const std::string& frame_data, uint16_t frame_num);
  
  // Tambur FEC components
  // std::unique_ptr<FrameGenerator> frame_generator_;
  // std::unique_ptr<FECSender> fec_sender_;
  // std::unique_ptr<Logger> logger_;
  
  // Buffer for storing generated FEC packets
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets_;
  
  // Current protection parameters
  FecProtectionParams current_delta_params_;
  FecProtectionParams current_key_params_;
  
  // Statistics
  size_t total_fec_bytes_generated_ = 0;
  Timestamp last_fec_generation_time_ = Timestamp::MinusInfinity();

  // Frame buffering for marker-based frame processing
  std::vector<uint8_t> current_frame_payload_;  // Buffer for current frame's payload
  std::vector<uint16_t> current_frame_seq_nums_; // Sequence numbers of packets in current frame
  uint32_t current_frame_timestamp_ = 0;        // Timestamp of the current frame
  uint16_t current_frame_start_seq_ = 0;        // Starting sequence number of current frame
  bool current_frame_has_marker_ = false;         // Whether we've seen marker packet for current frame
  uint16_t next_fec_seq_num_ = 0;               // Next sequence number for FEC packets
  std::unique_ptr<RtpPacketToSend> base_packet_template_; // Template for creating FEC packets
};

}  // namespace webrtc

#endif  // EXAMPLES_CUSTOMIZATIONFEC_TAMBUR_TAMBURFECGENERATOR_H_