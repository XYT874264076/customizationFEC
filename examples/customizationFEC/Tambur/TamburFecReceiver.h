#ifndef EXAMPLES_CUSTOMIZATIONFEC_TAMBUR_TAMBURFECRECEIVER_H_
#define EXAMPLES_CUSTOMIZATIONFEC_TAMBUR_TAMBURFECRECEIVER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "examples/customizationFEC/recovered_packet_receiver.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "examples/customizationFEC/UlpFEC/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/system/no_unique_address.h"
#include "system_wrappers/include/clock.h"
#include "examples/customizationFEC/fec_receiver.h"

// Tambur headers
// #include "examples/customizationFEC/Tambur/tambur/src/fec/fec_multi_receiver.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/code.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/header_code.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/quality_reporter.hh"
// #include "examples/customizationFEC/Tambur/tambur/src/fec/logger.hh"

namespace webrtc {

class TamburFecReceiver : public fecReceiver {
 public:
  TamburFecReceiver(uint32_t ssrc,
                     int tambur_fec_payload_type,
                     RecoveredPacketReceiver* callback,
                     Clock* clock);
  ~TamburFecReceiver() override;

  int fec_payload_type() const override { return tambur_fec_payload_type_; }

  bool AddReceivedRedPacket(const RtpPacketReceived& rtp_packet) override;

  void ProcessReceivedFec() override;

  FecPacketCounter GetPacketCounter() const override;

 private:
  // Convert WebRTC RTP packet to Tambur FEC datagram format
  std::string ConvertRtpToTamburDatagram(const RtpPacketReceived& rtp_packet);

  // Split recovered frame into multiple RTP packets
  std::vector<std::unique_ptr<RtpPacketReceived>> SplitFrameToRtpPackets(
      const std::string& frame_data, uint16_t frame_num, uint32_t video_frame_num);

  const uint32_t ssrc_;
  const int tambur_fec_payload_type_;
  Clock* const clock_;

  // Tambur FEC components
//   std::unique_ptr<Code> code_;
//   std::unique_ptr<HeaderCode> header_code_;
//   std::unique_ptr<QualityReporter> quality_reporter_;
//   std::unique_ptr<Logger> logger_;
//   std::unique_ptr<FECMultiReceiver> fec_receiver_;

  // Statistics
  FecPacketCounter packet_counter_;
  Timestamp first_packet_time_ = Timestamp::MinusInfinity();
  
  // Buffer for recovered packets
  std::vector<std::unique_ptr<RtpPacketReceived>> recovered_packets_;
};

}  // namespace webrtc

#endif  // EXAMPLES_CUSTOMIZATIONFEC_TAMBUR_TAMBURFECRECEIVER_H_