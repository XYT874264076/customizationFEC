
#ifndef EXAMPLES_CUSTOMIZATION_RTCP_FEC_REPORT_BLOCK_DATA_H_
#define EXAMPLES_CUSTOMIZATION_RTCP_FEC_REPORT_BLOCK_DATA_H_

#include <list>
#include <iostream>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "examples/customizationFEC/Rtcp/FEC_report_block.h"

namespace webrtc {

constexpr int kDefaultTimeWindow = 5;
constexpr TimeDelta kDefaultTimeThread = TimeDelta::Seconds(5);

class FECReportBlockData {
 public:
  FECReportBlockData() = default;
  FECReportBlockData(const FECReportBlockData&) = default;
  FECReportBlockData& operator=(const FECReportBlockData&) = default;

  uint32_t sender_ssrc() const { return sender_ssrc_; }
  uint32_t source_ssrc() const { return source_ssrc_; }

  float fraction_lost_recover() const {
    return static_cast<float>(fraction_lost_recover_raw()) / 256.0f;
  }
  uint8_t fraction_lost_recover_raw() const { return fraction_lost_recover_raw_; }

  float fixed_fraction_lost_recover() const {
    return static_cast<float>(fixed_fraction_lost_recover_raw()) / 256.0f;
  }
  uint8_t fixed_fraction_lost_recover_raw() const { return fixed_fraction_lost_recover_; }

  float fraction_effective_FEC_pkt() const {
    return static_cast<float>(fraction_effective_FEC_pkt_raw()) / 256.0f;
  }
  uint32_t fraction_effective_FEC_pkt_raw() const { return fraction_effective_FEC_pkt_raw_; }

  float fixed_fraction_effective_FEC_pkt() const {
    return static_cast<float>(fixed_fraction_effective_FEC_pkt_raw()) / 256.0f;
  }
  uint32_t fixed_fraction_effective_FEC_pkt_raw() const { return fixed_fraction_effective_FEC_pkt_; }

  uint16_t recover_pkt_num_raw() const { return recover_pkt_num_raw_; }
  uint32_t cumulative_recover_pkt() const { return cumulative_recover_pkt_; }
  uint16_t retransmit_pkt_num_raw() const { return retransmit_pkt_num_raw_; }
  uint32_t cumulative_retransmit_pkt() const { return cumulative_retransmit_pkt_; }
  uint16_t both_succ_pkt_num_raw() const { return both_succ_pkt_num_raw_; }
  uint32_t cumulative_both_succ_pkt() const { return cumulative_both_succ_pkt_; }

  uint16_t burst_lost_num_raw() const { return burst_lost_num_raw_; }
  uint32_t cumulative_burst_lost_num() const { return cumulative_burst_lost_num_; }

  int32_t average_recover_time() const { return average_recover_time_raw_; }
  int32_t fixed_average_recover_time() const { return fixed_average_recover_time_; }

  int32_t average_retransmit_time() const { return averger_retransmit_time_raw_; }
  int32_t fixed_average_retransmit_time() const { return fixed_averger_retransmit_time_; }

  int32_t time_ahead_for_recover() const { return time_ahead_for_recover_raw_; }
  int32_t fixed_time_ahead_for_recover() const { return fixed_time_ahead_for_recover_; }

  Timestamp report_block_timestamp_utc() const { return report_block_timestamp_utc_; }
  Timestamp report_block_timestamp() const { return report_block_timestamp_; }

  void set_sender_ssrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }
  void set_source_ssrc(uint32_t ssrc) { source_ssrc_ = ssrc; }
  void set_fraction_lost_recover(uint8_t fraction) {
    fraction_lost_recover_raw_ = fraction;
    while (fraction_lost_recover_raw_list_.size() >= kDefaultTimeWindow) {
        uint8_t front_fraction = fraction_lost_recover_raw_list_.front();
        sum_fraction_lost_recover_raw_ -= front_fraction;
        fraction_lost_recover_raw_list_.pop_front();
    }
    fraction_lost_recover_raw_list_.push_back(fraction);
    sum_fraction_lost_recover_raw_ += fraction;
    fixed_fraction_lost_recover_ = sum_fraction_lost_recover_raw_ / fraction_lost_recover_raw_list_.size();
  }
  void set_fraction_effective_FEC_pkt(uint8_t fraction) {
    fraction_effective_FEC_pkt_raw_ = fraction;
    while (fraction_effective_FEC_pkt_raw_list_.size() >= kDefaultTimeWindow) {
        uint8_t front_fraction = fraction_effective_FEC_pkt_raw_list_.front();
        sum_fraction_effective_FEC_pkt_raw_ -= front_fraction;
        fraction_effective_FEC_pkt_raw_list_.pop_front();
    }
    fraction_effective_FEC_pkt_raw_list_.push_back(fraction);
    sum_fraction_effective_FEC_pkt_raw_ += fraction;
    fixed_fraction_effective_FEC_pkt_ = sum_fraction_effective_FEC_pkt_raw_ / fraction_effective_FEC_pkt_raw_list_.size();
  }
  void set_recover_pkt_num(uint16_t num) {
    recover_pkt_num_raw_ = num;
    cumulative_recover_pkt_ += num;
  }
  void set_retransmit_pkt_num(uint16_t num) {
    retransmit_pkt_num_raw_ = num;
    cumulative_retransmit_pkt_ += num;
  }
  void set_both_succ_pkt_num(uint16_t num) {
    both_succ_pkt_num_raw_ = num;
    cumulative_both_succ_pkt_ += num;
  }
  void set_burst_lost_num(uint16_t num) {
    burst_lost_num_raw_ = num;
    while (burst_lost_num_list_.size() >= kDefaultTimeWindow) {
      burst_lost_num_list_.pop_front();
    }
    burst_lost_num_list_.push_back(num);
    cumulative_burst_lost_num_ += num;
  }
  void set_average_recover_time(int32_t recvtime, uint16_t num) {
    average_recover_time_raw_ = recvtime;

    if (num == 0) return;
    
    while (average_recover_time_raw_list_.size() >= kDefaultTimeWindow) {
        int32_t front_time = average_recover_time_raw_list_.front();
        sum_average_recover_time_raw_ -= front_time;
        average_recover_time_raw_list_.pop_front();
    }
    average_recover_time_raw_list_.push_back(recvtime);
    sum_average_recover_time_raw_ += recvtime;
    fixed_average_recover_time_ = sum_average_recover_time_raw_ / average_recover_time_raw_list_.size();
  }
  void set_average_retransmit_time(int32_t recvtime, uint16_t num) {
    averger_retransmit_time_raw_ = recvtime;
    if (num == 0) return;
    while (averger_retransmit_time_raw_list_.size() >= kDefaultTimeWindow) {
        int32_t front_time = averger_retransmit_time_raw_list_.front();
        sum_averger_retransmit_time_raw_ -= front_time;
        averger_retransmit_time_raw_list_.pop_front();
    }
    averger_retransmit_time_raw_list_.push_back(recvtime);
    sum_averger_retransmit_time_raw_ += recvtime;
    fixed_averger_retransmit_time_ = sum_averger_retransmit_time_raw_ / averger_retransmit_time_raw_list_.size();
  }
  void set_time_ahead_for_recover(int32_t recvtime, uint16_t num) {

    time_ahead_for_recover_raw_ = recvtime;
    if (num == 0) return;
    while (time_ahead_for_recover_raw_list_.size() >= kDefaultTimeWindow) {
        int32_t front_time = time_ahead_for_recover_raw_list_.front();
        sum_time_ahead_for_recover_raw_ -= front_time;
        time_ahead_for_recover_raw_list_.pop_front();
    }
    time_ahead_for_recover_raw_list_.push_back(recvtime);
    sum_time_ahead_for_recover_raw_ += recvtime;
    int list_size = time_ahead_for_recover_raw_list_.size();
    fixed_time_ahead_for_recover_ = sum_time_ahead_for_recover_raw_ / list_size;
  }
  void set_report_block_timestamp(Timestamp report_block_timestamp_utc, Timestamp report_block_timestamp) {
    if (report_block_timestamp_ != Timestamp::Zero()) {
        if (report_block_timestamp - report_block_timestamp_ > kDefaultTimeThread) {
            ResetFECReportBlockData();
        }
    }
    report_block_timestamp_utc_ = report_block_timestamp_utc;
    report_block_timestamp_ = report_block_timestamp;
  }
  void ResetFECReportBlockData() {
    recover_pkt_num_raw_ = 0;
    cumulative_recover_pkt_ = 0;
    retransmit_pkt_num_raw_ = 0;
    cumulative_retransmit_pkt_ = 0;
    both_succ_pkt_num_raw_ = 0;
    cumulative_both_succ_pkt_ = 0;

    fraction_lost_recover_raw_ = 0;
    fraction_effective_FEC_pkt_raw_ = 0;

    burst_lost_num_raw_ = 0;
    cumulative_burst_lost_num_ = 0;
  
    average_recover_time_raw_ = 0;
    averger_retransmit_time_raw_ = 0;
    time_ahead_for_recover_raw_ = 0;

    fixed_average_recover_time_ = 0;
    fixed_averger_retransmit_time_ = 0;
    fixed_time_ahead_for_recover_ = 0;
    fixed_fraction_lost_recover_ = 0;
    fixed_fraction_effective_FEC_pkt_ = 0;

    sum_average_recover_time_raw_ = 0;
    sum_averger_retransmit_time_raw_ = 0;
    sum_time_ahead_for_recover_raw_ = 0;
    sum_fraction_lost_recover_raw_ = 0;
    sum_fraction_effective_FEC_pkt_raw_ = 0;
    average_recover_time_raw_list_.clear();
    averger_retransmit_time_raw_list_.clear();
    time_ahead_for_recover_raw_list_.clear();
    fraction_lost_recover_raw_list_.clear();
    fraction_effective_FEC_pkt_raw_list_.clear();
  }
  void SetFECReportBlock(uint32_t sender_ssrc,
                         const rtcp::FECReportBlock& report_block,
                         Timestamp report_block_timestamp_utc,
                         Timestamp report_block_timestamp);

 private:
  uint32_t sender_ssrc_ = 0;
  uint32_t source_ssrc_ = 0;

  uint16_t recover_pkt_num_raw_ = 0;
  uint32_t cumulative_recover_pkt_ = 0;
  uint16_t retransmit_pkt_num_raw_ = 0;
  uint32_t cumulative_retransmit_pkt_ = 0;
  uint16_t both_succ_pkt_num_raw_ = 0;
  uint32_t cumulative_both_succ_pkt_ = 0;

  uint8_t fraction_lost_recover_raw_ = 0;
  uint8_t fraction_effective_FEC_pkt_raw_ = 0;

  uint16_t burst_lost_num_raw_ = 0;
  uint32_t cumulative_burst_lost_num_ = 0;
  
  // Maybe is zero if there no recover/retransmit packets are received in this time interval.
  int32_t average_recover_time_raw_ = 0;
  int32_t averger_retransmit_time_raw_ = 0;
  int32_t time_ahead_for_recover_raw_ = 0;

  // The fix parameters calculated by raw data for a time window, skipped if data is zero.
  int32_t fixed_average_recover_time_ = 0;
  int32_t fixed_averger_retransmit_time_ = 0;
  int32_t fixed_time_ahead_for_recover_ = 0;
  uint8_t fixed_fraction_lost_recover_ = 0;
  uint8_t fixed_fraction_effective_FEC_pkt_ = 0;

  // Data to calculate fix parameters
  int64_t sum_average_recover_time_raw_ = 0;
  int64_t sum_averger_retransmit_time_raw_ = 0;
  int64_t sum_time_ahead_for_recover_raw_ = 0;
  uint16_t sum_fraction_lost_recover_raw_ = 0;
  uint16_t sum_fraction_effective_FEC_pkt_raw_ = 0;
  std::list<int32_t> average_recover_time_raw_list_;
  std::list<int32_t> averger_retransmit_time_raw_list_;
  std::list<int32_t> time_ahead_for_recover_raw_list_;
  std::list<uint8_t> fraction_lost_recover_raw_list_;
  std::list<uint8_t> fraction_effective_FEC_pkt_raw_list_;
  std::list<uint16_t> burst_lost_num_list_;

  Timestamp report_block_timestamp_utc_ = Timestamp::Zero();
  Timestamp report_block_timestamp_ = Timestamp::Zero();
};

class FECReportBlockDataObserver {
 public:
  virtual ~FECReportBlockDataObserver() = default;

  virtual void OnFECReportBlockDataUpdate(FECReportBlockData fec_report_block_data) = 0;
};

}   // namespace webrtc

#endif  // EXAMPLES_CUSTOMIZATION_RTCP_FEC_REPORT_BLOCK_DATA_H_