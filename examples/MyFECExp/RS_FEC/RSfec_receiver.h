#ifndef EXAMPLES_MYFECEXP_RSFEC_RS_FEC_RSFEC_RECEIVER_H_
#define EXAMPLES_MYFECEXP_RSFEC_RS_FEC_RSFEC_RECEIVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "examples/MyFECExp/recovered_packet_receiver.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "examples/MyFECExp/RS_FEC/RS_forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/system/no_unique_address.h"
#include "system_wrappers/include/clock.h"
#include "examples/MyFECExp/fec_receiver.h"

namespace webrtc {

class RSfecReceiver : public fecReceiver {
  public:
    RSfecReceiver(uint32_t ssrc, int rsfec_payload_type, RecoveredPacketReceiver* callback, Clock* clock);
    ~RSfecReceiver();

    int fec_payload_type() const override {return rsfec_payload_type_;}
    bool AddReceivedRedPacket(const RtpPacketReceived& rtp_packet) override;
    void ProcessReceivedFec() override;
    FecPacketCounter GetPacketCounter() const override;

  private:
    const uint32_t ssrc_;
    const int rsfec_payload_type_;
    Clock* const clock_;

    RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
    RecoveredPacketReceiver* const recovered_packet_callback_;
    const std::unique_ptr<RSForwardErrorCorrection> fec_;
    std::vector<std::unique_ptr<RSForwardErrorCorrection::ReceivedPacket>>
        received_packets_ RTC_GUARDED_BY(&sequence_checker_);
    RSForwardErrorCorrection::RecoveredPacketList recovered_packets_
        RTC_GUARDED_BY(&sequence_checker_);
    FecPacketCounter packet_counter_ RTC_GUARDED_BY(&sequence_checker_);
};

}  // webrtc


#endif // EXAMPLES_MYFECEXP_RSFEC_RS_FEC_RSFEC_RECEIVER_H_