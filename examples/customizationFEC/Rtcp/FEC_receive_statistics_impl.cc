
#include "examples/customizationFEC/Rtcp/FEC_receive_statistics_impl.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>
#include <iostream>

#include "api/units/time_delta.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "examples/customizationFEC/Rtcp/FEC_report_block.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc{
namespace {
constexpr TimeDelta kStatisticsTimeout = TimeDelta::Seconds(8);
constexpr int kDefaultMaxConsiderLostPktNum = 256;

TimeDelta UnixEpochDelta(Clock& clock) {
    Timestamp now = clock.CurrentTime();
    NtpTime ntp_now = clock.ConvertTimestampToNtpTime(now);
    return TimeDelta::Millis(ntp_now.ToMs() - now.ms() - rtc::kNtpJan1970Millisecs);
}

}   // namespace

FecStreamStatistician::~FecStreamStatistician() {}

FECStreamStatisticianImpl::FECStreamStatisticianImpl(uint32_t ssrc, Clock* clock) 
    : ssrc_(ssrc),
      clock_(clock),
      delta_internal_unix_epoch_(UnixEpochDelta(*clock_)),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold),
      max_consider_lost_packet_num_(kDefaultMaxConsiderLostPktNum),
      received_seq_first_(-1),
      received_seq_max_(-1),
      last_report_seq_max_(-1),
      cumulative_fec_pkt_num_(0),
      cumulative_recover_pkt_num_(0),
      cumulative_loss_pkt_num_(0),
      last_report_cumulative_fec_pkt_num_(0),
      last_report_cumulative_recover_pkt_num_(0),
      last_report_cumulative_loss_pkt_num_(0),
      cumulative_retransmit_pkt_num_(0),
      last_report_cumulative_retransmit_pkt_num_(0),
      cumulative_retransmit_time_(0),
      cumulative_recover_time_(0),
      last_report_cumulative_retransmit_time_(0),
      last_report_cumulative_recover_time_(0),
      cumulative_both_succ_pkt_num_(0),
      cumulative_ahead_time_(0),
      last_report_cumulative_both_succ_pkt_num_(0),
      last_report_cumulative_ahead_time_(0),
      cumulative_burst_lost_num_(0),
      last_report_cumulative_burst_lost_num_(0) {}

FECStreamStatisticianImpl::~FECStreamStatisticianImpl() = default;


// TODO: Implementation for Message from StreamStatistician!


// Implements StreamStatisticianImplInterface
void FECStreamStatisticianImpl::MaybeAppendReportBlockAndReset(std::vector<rtcp::FECReportBlock>& report_blocks) {

//   std::cout<<"Run MaybeAppendReportBlockAndReset"<<std::endl;

  if (!ReceivedRtpPacket()) {
    return;
  }
  Timestamp now = clock_->CurrentTime();
  if (now - *last_receive_time_ >= kStatisticsTimeout) {
    // Not active.
    return;
  }

  report_blocks.emplace_back();
  rtcp::FECReportBlock& stats = report_blocks.back();
  stats.SetMediaSsrc(ssrc_);

  // Calculate fraction lost recover.
  int32_t recv_fec_pkt_num = cumulative_fec_pkt_num_ - last_report_cumulative_fec_pkt_num_;
  int32_t recov_pkt_num = cumulative_recover_pkt_num_ - last_report_cumulative_recover_pkt_num_;
  if (recov_pkt_num > recv_fec_pkt_num) {
    stats.SetFractionEffectiveFECPkt(255);
  }
  else if (recv_fec_pkt_num == 0) {
    stats.SetFractionEffectiveFECPkt(255);
  }
  else {
    stats.SetFractionEffectiveFECPkt(255 * recov_pkt_num / recv_fec_pkt_num);
  }

  // Calculate fraction FEC Packet effective.
  int32_t lost_since_last = cumulative_loss_pkt_num_ - last_report_cumulative_loss_pkt_num_;
  if (recov_pkt_num > lost_since_last) {
    stats.SetFractionLostRecover(255);
  }
  else if (lost_since_last == 0) {
    stats.SetFractionLostRecover(255);
  }
  else {
    stats.SetFractionLostRecover(255 * recov_pkt_num / lost_since_last);
  }

  // Set RecoverPktNum.
  stats.SetRecoverPktNum((uint16_t)recov_pkt_num);

  // Calculate average recover time.
  double recover_time_sum = cumulative_recover_time_ - last_report_cumulative_recover_time_;
  double averager_recover_time = 0;
  if (recov_pkt_num != 0) {
    averager_recover_time = recover_time_sum / recov_pkt_num;
  }
  double converted_double_value = averager_recover_time * 65536;
  stats.SetAverageRecoverTime(static_cast<int32_t>(std::round(converted_double_value)));

  // Calculate average retransmit time.
  double retransmit_time_sum = cumulative_retransmit_time_ - last_report_cumulative_retransmit_time_;
  int32_t retrans_pkt_num = cumulative_retransmit_pkt_num_ - last_report_cumulative_retransmit_pkt_num_;
  double average_retransmit_time = 0;
  if (retrans_pkt_num != 0) {
    average_retransmit_time = retransmit_time_sum / retrans_pkt_num;
  }
  converted_double_value = average_retransmit_time * 65536;
  stats.SetAverageRetransmitTime(static_cast<int32_t>(std::round(converted_double_value)));
  
  // Calculate time head for recover.
  double time_ahead_sum = cumulative_ahead_time_ - last_report_cumulative_ahead_time_;
  int32_t both_succ_num = cumulative_both_succ_pkt_num_ - last_report_cumulative_both_succ_pkt_num_;
  double average_time_ahead = 0;
  if (both_succ_num != 0) {
    average_time_ahead = time_ahead_sum / both_succ_num;
  } 

  converted_double_value = average_time_ahead * 65536;
  
  stats.SetTimeAheadForRecover(static_cast<int32_t>(std::round(converted_double_value)));

  // Set Retransmit Packet Number & Both Packet Number
  stats.SetRetransmitPktNum(retrans_pkt_num);
  stats.SetBothPktNum(both_succ_num);

  // Set burst packets lost.
  uint16_t burst_pkt_lost = cumulative_burst_lost_num_ - last_report_cumulative_burst_lost_num_;
  stats.SetBurstLostNum(burst_pkt_lost);

  // Update last_report_*
  last_report_seq_max_ = received_seq_max_;

  last_report_cumulative_fec_pkt_num_ = cumulative_fec_pkt_num_;
  last_report_cumulative_recover_pkt_num_ = cumulative_recover_pkt_num_;
  last_report_cumulative_loss_pkt_num_ = cumulative_loss_pkt_num_;

  last_report_cumulative_retransmit_pkt_num_ = cumulative_retransmit_pkt_num_;
  last_report_cumulative_retransmit_time_ = cumulative_retransmit_time_;
  last_report_cumulative_recover_time_ = cumulative_recover_time_;


  last_report_cumulative_both_succ_pkt_num_ = cumulative_both_succ_pkt_num_;
  last_report_cumulative_ahead_time_ = cumulative_ahead_time_;

  last_report_cumulative_burst_lost_num_ = cumulative_burst_lost_num_;
}

void FECStreamStatisticianImpl::SetMaxReorderingThreshold(int max_reordering_threshold) {
    max_reordering_threshold_ = max_reordering_threshold;
}

void FECStreamStatisticianImpl::SetMaxConsiderLostPacketNum(int max_consider_lost_packet_num) {
    max_consider_lost_packet_num_ = max_consider_lost_packet_num;
}

// Updates FEC Statistician infos for incoming packets.
void FECStreamStatisticianImpl::ReceiveFECPacket(const RtpPacketReceived& packet) {
    RTC_DCHECK_EQ(ssrc_, packet.Ssrc());

    // std::cout<<"Run ReceiveFECPacket"<<std::endl;

    // Update statistic infos
    uint16_t seq_num = packet.SequenceNumber();
    if (!ReceivedRtpPacket()) {
        received_seq_first_ = seq_num;
        received_seq_max_ = seq_num - 1;
        last_report_seq_max_ = seq_num - 1;
    }
    cumulative_fec_pkt_num_ += 1;

    // Update arriving time
    Timestamp now = clock_->CurrentTime();
    last_receive_time_ = now;

    // Judge if there are new lost packet
    uint16_t seq_num_diff = MinDiff(received_seq_max_, seq_num);
    if (IsNewerSequenceNumber(seq_num, received_seq_max_) && seq_num_diff!=1) {
        InsertConsiderLostPacket(packet);
    }

    // Update received_seq_max_
    if (IsNewerSequenceNumber(seq_num, received_seq_max_)) {
        received_seq_max_ = seq_num;
    }
}

void FECStreamStatisticianImpl::ReceiveVideoPacket(const RtpPacketReceived& packet) {
    RTC_DCHECK_EQ(ssrc_, packet.Ssrc());

    // std::cout<<"Run ReceiveVideoPacket"<<std::endl;

    // Update statistic infos
    uint16_t seq_num = packet.SequenceNumber();
    if (!ReceivedRtpPacket()) {
        received_seq_first_ = seq_num;
        received_seq_max_ = seq_num - 1;
        last_report_seq_max_ = seq_num - 1;
    }

    // Update arriving time
    Timestamp now = clock_->CurrentTime();
    last_receive_time_ = now;

    // Judge if there are new lost packet
    uint16_t seq_num_diff = MinDiff(received_seq_max_, seq_num);
    if (IsNewerSequenceNumber(seq_num, received_seq_max_) && seq_num_diff!=1) {
        InsertConsiderLostPacket(packet);
    }

    // Update received_seq_max_
    if (IsNewerSequenceNumber(seq_num, received_seq_max_)) {
        received_seq_max_ = seq_num;
    }
}

void FECStreamStatisticianImpl::ReceiveRecoverPacket(const RtpPacketReceived& packet) {
    RTC_DCHECK_EQ(ssrc_, packet.Ssrc());
    
    // std::cout<<"Run ReceiveRecoverPacket"<<std::endl;
    
    if (!ReceivedRtpPacket()) {
        // Recover Packet can't be the first packet for a flow!
        return;
    }

    uint16_t seq_num = packet.SequenceNumber();
    if (!packet_lost_time_.contains(seq_num) || !packet_recover_time_.contains(seq_num) || !packet_retransmit_time_.contains(seq_num)) {
        // Too old recover packet is not meaningful!
        return;
    }
    if (packet_lost_time_[seq_num] == std::nullopt) {
        // Maybe wrong, fail to record lost time!
        return;
    }
    
    // Update arriving time & statistic infos
    Timestamp now = clock_->CurrentTime();
    last_receive_time_ = now;
    if (packet_recover_time_[seq_num] == std::nullopt) {
        // We only consider the first recover packet for this lost one!
        cumulative_recover_pkt_num_ += 1;
        packet_recover_time_[seq_num] = now;
        TimeDelta TDelta = now - *packet_lost_time_[seq_num];
        double TDeltaDouble = static_cast<double>(TDelta.ns()) / 1e9;
        cumulative_recover_time_ += TDeltaDouble;

        if (packet_retransmit_time_[seq_num] != std::nullopt) {
            // This packet has receive a retransmit packet!
            TimeDelta AheadTime = *packet_retransmit_time_[seq_num] - now;
            double AheadTimeDouble = static_cast<double>(AheadTime.ns()) / 1e9;
            cumulative_ahead_time_ += AheadTimeDouble;
            cumulative_both_succ_pkt_num_ += 1;
        }
    }

    // std::cout<<"\tCurrent cumulative_recover_pkt_num_:"<<cumulative_recover_pkt_num_<<std::endl;
}

void FECStreamStatisticianImpl::ReceiveRetransmitPacket(const RtpPacketReceived& packet) {
    RTC_DCHECK_EQ(ssrc_, packet.Ssrc());
    
    // std::cout<<"Run ReceiveRetransmitPacket"<<std::endl;

    if (!ReceivedRtpPacket()) {
        // Retransmit Packet can't be the first packet for a flow!
        return;
    }
    uint16_t seq_num = packet.SequenceNumber();
    if (!packet_lost_time_.contains(seq_num) || !packet_recover_time_.contains(seq_num) || !packet_retransmit_time_.contains(seq_num)) {
        // Too old retransmit packet is not meaningful!
        return;
    }
    if (packet_lost_time_[seq_num] == std::nullopt) {
        // Maybe wrong, fail to record lost time!
        return;
    }

    // Update arriving time & statistic infos
    Timestamp now = clock_->CurrentTime();
    last_receive_time_ = now;
    if (packet_retransmit_time_[seq_num] == std::nullopt) {
        // We only consider the first retransmit packet for this lost one!
        cumulative_retransmit_pkt_num_ += 1;
        packet_retransmit_time_[seq_num] = now;
        TimeDelta TDelta = now - *packet_lost_time_[seq_num];
        double TDeltaDouble = static_cast<double>(TDelta.ns()) / 1e9;
        cumulative_retransmit_time_ += TDeltaDouble;

        if (packet_recover_time_[seq_num] != std::nullopt) {
            // This packet has receive a recover packet!
            TimeDelta AheadTime = now - *packet_recover_time_[seq_num];
            double AheadTimeDouble = static_cast<double>(AheadTime.ns()) / 1e9;
            cumulative_ahead_time_ += AheadTimeDouble;
            cumulative_both_succ_pkt_num_ += 1;
        }
    }
}

// Implementation private function.
bool FECStreamStatisticianImpl::InsertConsiderLostPacket(const RtpPacketReceived& packet) {
    uint16_t maybe_lost_seq_num = received_seq_max_ + 1;
    uint16_t seq_num = packet.SequenceNumber();
    Timestamp now = clock_->CurrentTime();
    uint16_t cnt = 0;

    while (IsNewerSequenceNumber(seq_num, maybe_lost_seq_num)) {
        while (consider_lost_packet_list_.size() >= (size_t)max_consider_lost_packet_num_) {
            uint16_t pop_seq_num = *consider_lost_packet_list_.cbegin();
            packet_lost_time_.erase(pop_seq_num);
            packet_recover_time_.erase(pop_seq_num);
            packet_retransmit_time_.erase(pop_seq_num);
            consider_lost_packet_list_.pop_front();           
        }

        consider_lost_packet_list_.push_back(maybe_lost_seq_num);
        packet_lost_time_[maybe_lost_seq_num] = now;
        packet_recover_time_[maybe_lost_seq_num] = std::nullopt;
        packet_retransmit_time_[maybe_lost_seq_num] = std::nullopt;

        cumulative_loss_pkt_num_ += 1;
        maybe_lost_seq_num += 1; 
        cnt += 1;       
    }
    if (cnt > 1) cumulative_burst_lost_num_ += 1;

    return true;
}

// Implementation for ReceiveStatistics.
std::unique_ptr<FecReceiveStatistics> FecReceiveStatistics::Create(Clock* clock) {
    return std::make_unique<FECReceiveStatisticsLocked>(
        clock, [](uint32_t ssrc, Clock* clock) {
            return std::make_unique<FECStreamStatisticianLocked>(ssrc, clock);
        }
    );
}

std::unique_ptr<FecReceiveStatistics> FecReceiveStatistics::CreateThreadCompatible(Clock* clock) {
    return std::make_unique<FECReceiveStatisticsImpl>(
        clock, [](uint32_t ssrc, Clock* clock) {
            return std::make_unique<FECStreamStatisticianImpl>(ssrc,clock);
        }
    );
}

// Implementation for ReceiveStatisticsImpl
FECReceiveStatisticsImpl::FECReceiveStatisticsImpl(
    Clock* clock,
    std::function<std::unique_ptr<FECStreamStatisticianImplInterface>(
        uint32_t ssrc,
        Clock* clock)> stream_statistician_factory) 
    : clock_(clock),
      stream_statistician_factory_(stream_statistician_factory),
      last_returned_ssrc_idx_(0) {}

std::vector<rtcp::FECReportBlock> FECReceiveStatisticsImpl::RtcpReportBlocks(size_t max_blocks) {
    
    // std::cout<<"Run FECReceiveStatisticsImpl::RtcpReportBlocks!"<<std::endl;
    
    std::vector<rtcp::FECReportBlock> result;
    result.reserve(std::min(max_blocks, all_ssrcs_.size()));

    size_t ssrc_idx = 0;
    for (size_t i = 0; i < all_ssrcs_.size() && result.size() < max_blocks; ++i) {
        ssrc_idx = (last_returned_ssrc_idx_ + i + 1) % all_ssrcs_.size();
        const uint32_t media_ssrc = all_ssrcs_[ssrc_idx];
        auto statistician_it = statisticians_.find(media_ssrc);
        RTC_DCHECK(statistician_it != statisticians_.end());
        statistician_it->second->MaybeAppendReportBlockAndReset(result);
    }

    last_returned_ssrc_idx_ = ssrc_idx;
    return result;
}

FecStreamStatistician* FECReceiveStatisticsImpl::GetStatistician(uint32_t ssrc) const {
    const auto& it = statisticians_.find(ssrc);
    if (it == statisticians_.end())
        return nullptr;
    return it->second.get();
}

void FECReceiveStatisticsImpl::SetMaxReorderingThreshold(uint32_t ssrc, int max_reordering_threshold) {
    GetOrCreateStatistician(ssrc)->SetMaxReorderingThreshold(max_reordering_threshold);
}

void FECReceiveStatisticsImpl::SetMaxConsiderLostPktNum(uint32_t ssrc, int max_consider_lost_pkt_num) {
    GetOrCreateStatistician(ssrc)->SetMaxConsiderLostPacketNum(max_consider_lost_pkt_num);
}

void FECReceiveStatisticsImpl::OnFECPacket(const RtpPacketReceived& packet) {
    GetOrCreateStatistician(packet.Ssrc())->ReceiveFECPacket(packet);
}

void FECReceiveStatisticsImpl::OnVideoPacket(const RtpPacketReceived& packet) {
    GetOrCreateStatistician(packet.Ssrc())->ReceiveVideoPacket(packet);
}

void FECReceiveStatisticsImpl::OnRecoverPacket(const RtpPacketReceived& packet) {
    GetOrCreateStatistician(packet.Ssrc())->ReceiveRecoverPacket(packet);
}

void FECReceiveStatisticsImpl::OnRetransmitPacket(const RtpPacketReceived& packet) {
    GetOrCreateStatistician(packet.Ssrc())->ReceiveRetransmitPacket(packet);
}

FECStreamStatisticianImplInterface* FECReceiveStatisticsImpl::GetOrCreateStatistician(uint32_t ssrc) {
    std::unique_ptr<FECStreamStatisticianImplInterface>& impl = statisticians_[ssrc];
    if (impl == nullptr) {
        impl = stream_statistician_factory_(ssrc, clock_);
        all_ssrcs_.push_back(ssrc);
    }

    return impl.get();
}

}   // namespace webrtc