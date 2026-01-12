
#ifndef EXAMPLES_MYFECEXP_RTCP_FEC_REPORT_BLOCK_H_
#define EXAMPLES_MYFECEXP_RTCP_FEC_REPORT_BLOCK_H_

#include <stddef.h>
#include <stdint.h>

#include <iostream>

namespace webrtc {
namespace rtcp {

class FECReportBlock {
 public:
  // TODO: Change me!!!
  static constexpr size_t kLength = 28;

  FECReportBlock();
  ~FECReportBlock() {}

  bool Parse(const uint8_t* buffer, size_t length);

  void Create(uint8_t* buffer) const;

  void SetMediaSsrc(uint32_t ssrc) {source_ssrc_ = ssrc; }
  void SetFractionLostRecover(uint8_t fraction_lost_recover) {
    fraction_lost_recover_ = fraction_lost_recover;
  }
  void SetFractionEffectiveFECPkt(uint8_t fraction_effective_FEC_pkt) {
    fraction_effective_FEC_pkt_ = fraction_effective_FEC_pkt;
  }
  void SetRecoverPktNum(uint16_t recover_pkt_num) {
    recover_pkt_num_ = recover_pkt_num;
  }
  void SetAverageRecoverTime(int32_t average_recover_time) {
    average_recover_time_ = average_recover_time;
  }
  void SetAverageRetransmitTime(int32_t average_retransmit_time) {
    average_retransmit_time_ = average_retransmit_time;
  }
  void SetTimeAheadForRecover(int32_t time_ahead_for_recover) {
    time_ahead_for_recover_ = time_ahead_for_recover;
  }
  void SetRetransmitPktNum(uint16_t retransmit_pkt_num) {
    retransmit_pkt_num_ = retransmit_pkt_num;
  }
  void SetBothPktNum(uint16_t both_pkt_num) {
    both_pkt_num_ = both_pkt_num;
  }
  void SetBurstLostNum(uint16_t burst_lost_num) {
    burst_lost_num_ = burst_lost_num;
  }

  uint32_t source_ssrc() const { return source_ssrc_; }
  uint8_t fraction_lost_recover() const { return fraction_lost_recover_; }
  uint8_t fraction_effective_FEC_pkt() const { return fraction_effective_FEC_pkt_; }
  uint16_t recover_pkt_num() const { return recover_pkt_num_; }
  int32_t average_recover_time() const { return average_recover_time_; }
  int32_t average_retransmit_time() const { return average_retransmit_time_; }
  int32_t time_ahead_for_recover() const { return time_ahead_for_recover_; }
  uint16_t retransmit_pkt_num() const { return retransmit_pkt_num_; }
  uint16_t both_pkt_num() const { return both_pkt_num_; }
  uint16_t burst_lost_num() const { return burst_lost_num_; }

 private:
  uint32_t source_ssrc_;                // 32 bits
  uint8_t fraction_lost_recover_;       // 8 bits representing a fixed point value 0..1
  uint8_t fraction_effective_FEC_pkt_;  // 8 bits representing a fixed point value 0..1
  uint16_t recover_pkt_num_;            // 16 bits
  int32_t average_recover_time_;        // 32 bits, units of 1/65536 seconds
  int32_t average_retransmit_time_;     // 32 bits, units of 1/65536 seconds
  int32_t time_ahead_for_recover_;      // 32 bits, units of 1/65536 seconds
  uint16_t retransmit_pkt_num_;         // 16 bits
  uint16_t both_pkt_num_;               // 16 bits
  uint16_t burst_lost_num_;             // 16 bits
};

}   // namespace rtcp
}   // namespace webrtc


#endif  // EXAMPLES_MYFECEXP_RTCP_FEC_REPORT_BLOCK_H_