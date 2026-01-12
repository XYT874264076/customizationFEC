
#ifndef EXAMPLES_MYFECEXP_RTCP_FEC_RECEIVE_STATISTICS_IMPL_H_
#define EXAMPLES_MYFECEXP_RTCP_FEC_RECEIVE_STATISTICS_IMPL_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <list>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "examples/MyFECExp/Rtcp/FEC_statistics.h"
#include "examples/MyFECExp/Rtcp/FEC_report_block.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class FECStreamStatisticianImplInterface : public FecStreamStatistician {
 public:
  virtual ~FECStreamStatisticianImplInterface() = default;
  virtual void MaybeAppendReportBlockAndReset(std::vector<rtcp::FECReportBlock>& report_blocks) = 0;
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;
  virtual void SetMaxConsiderLostPacketNum(int max_consider_lost_packet_num) = 0;
  virtual void ReceiveFECPacket(const RtpPacketReceived& packet) = 0;
  virtual void ReceiveVideoPacket(const RtpPacketReceived& packet) = 0;
  virtual void ReceiveRecoverPacket(const RtpPacketReceived& packet) = 0;
  virtual void ReceiveRetransmitPacket(const RtpPacketReceived& packet) = 0;
};

class FECStreamStatisticianImpl : public FECStreamStatisticianImplInterface {
 public:
  FECStreamStatisticianImpl(uint32_t ssrc, Clock* clock);
  ~FECStreamStatisticianImpl() override;

  // TODO: Implements FecStreamStatistician
  

  // Implements StreamStatisticianImplInterface
  void MaybeAppendReportBlockAndReset(std::vector<rtcp::FECReportBlock>& report_blocks) override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void SetMaxConsiderLostPacketNum(int max_consider_lost_packet_num) override;

  // Updates FEC Statistician infos for incoming packets.
  void ReceiveFECPacket(const RtpPacketReceived& packet) override;
  void ReceiveVideoPacket(const RtpPacketReceived& packet) override;
  void ReceiveRecoverPacket(const RtpPacketReceived& packet) override;
  void ReceiveRetransmitPacket(const RtpPacketReceived& packet) override;

 private:
  // Checks if this StreamStatistician received any rtp packets.
  bool ReceivedRtpPacket() const { return last_receive_time_.has_value(); }
  bool InsertConsiderLostPacket(const RtpPacketReceived& packet);

  const uint32_t ssrc_;
  Clock* const clock_;
  // Delta used to map internal timestamps to Unix epoch ones.
  const TimeDelta delta_internal_unix_epoch_;
  // In number of packets or sequence numbers.
  int max_reordering_threshold_;
  int max_consider_lost_packet_num_;

  // To record the Basic Infos
  std::optional<Timestamp> last_receive_time_;
  uint16_t received_seq_first_;
  uint16_t received_seq_max_;
  uint16_t last_report_seq_max_;

  // Infos to calculate FEC Packets effective.
  // TODO: Maybe this is not a perfect method to get the fraction of FEC effective Packet.
  int32_t cumulative_fec_pkt_num_;
  int32_t cumulative_recover_pkt_num_;
  int32_t cumulative_loss_pkt_num_;
  int32_t last_report_cumulative_fec_pkt_num_;
  int32_t last_report_cumulative_recover_pkt_num_;
  int32_t last_report_cumulative_loss_pkt_num_;

  // Infos to calculate Average Recover Time & Average Retransmission Time
  std::list<uint16_t> consider_lost_packet_list_;
  flat_map<uint16_t /*seq_num*/, std::optional<Timestamp>> packet_lost_time_;
  flat_map<uint16_t /*seq_num*/, std::optional<Timestamp>> packet_recover_time_;
  flat_map<uint16_t /*seq_num*/, std::optional<Timestamp>> packet_retransmit_time_;
  int32_t cumulative_retransmit_pkt_num_;
  // int32_t cumulative_recover_pkt_num_ already define!
  int32_t last_report_cumulative_retransmit_pkt_num_;
  // int32_t last_report_cumulative_recover_pkt_num_ already define!
  // TODO: Consider if I should use Double here?
  double cumulative_retransmit_time_;   // unit - seconds
  double cumulative_recover_time_;      // unit - seconds
  double last_report_cumulative_retransmit_time_;
  double last_report_cumulative_recover_time_;

  // Infos to calculate average time ahead.
  int32_t cumulative_both_succ_pkt_num_;
  double cumulative_ahead_time_;        // unit - seconds
  int32_t last_report_cumulative_both_succ_pkt_num_;
  double last_report_cumulative_ahead_time_;

  // Infos to record burst lost.
  uint16_t cumulative_burst_lost_num_;
  uint16_t last_report_cumulative_burst_lost_num_;

  // TODO: Delete this code! This is only for search!
//   std::optional<Timestamp> last_receive_time_;
//   uint32_t last_received_timestamp_;
};

// Thread-safe implementation of FECStreamStatisticianImplInterface.
class FECStreamStatisticianLocked : public FECStreamStatisticianImplInterface {
 public:
  FECStreamStatisticianLocked(uint32_t ssrc, Clock* clock) : impl_(ssrc, clock) {}
  ~FECStreamStatisticianLocked() override = default;

  // TODO: Implements FecStreamStatistician

  
  void MaybeAppendReportBlockAndReset(std::vector<rtcp::FECReportBlock>& report_blocks) override {
    MutexLock lock(&stream_lock_);
    impl_.MaybeAppendReportBlockAndReset(report_blocks);
  }
  void SetMaxReorderingThreshold(int max_reordering_threshold) override {
    MutexLock lock(&stream_lock_);
    return impl_.SetMaxReorderingThreshold(max_reordering_threshold);
  }
  void SetMaxConsiderLostPacketNum(int max_consider_lost_packet_num) override {
    MutexLock lock(&stream_lock_);
    return impl_.SetMaxConsiderLostPacketNum(max_consider_lost_packet_num);
  }
  void ReceiveFECPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&stream_lock_);
    return impl_.ReceiveFECPacket(packet);
  }
  void ReceiveVideoPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&stream_lock_);
    return impl_.ReceiveVideoPacket(packet);
  }
  void ReceiveRecoverPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&stream_lock_);
    return impl_.ReceiveRecoverPacket(packet);
  }
  void ReceiveRetransmitPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&stream_lock_);
    return impl_.ReceiveRetransmitPacket(packet);
  }

 private:
  mutable Mutex stream_lock_;
  FECStreamStatisticianImpl impl_ RTC_GUARDED_BY(&stream_lock_);
};

// Thread-compatible implementation.
class FECReceiveStatisticsImpl : public FecReceiveStatistics {
 public:
  FECReceiveStatisticsImpl(Clock* clock, 
  std::function<std::unique_ptr<FECStreamStatisticianImplInterface>(
        uint32_t ssrc,
        Clock* clock)> stream_statistician_factory);
  ~FECReceiveStatisticsImpl() override = default;

  // Implements FecReceiveStatisticsProvider.
  std::vector<rtcp::FECReportBlock> RtcpReportBlocks(size_t max_blocks) override;

  // Implements FecReceiveStatistics.
  FecStreamStatistician* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(uint32_t ssrc, int max_reordering_threshold) override;
  void SetMaxConsiderLostPktNum(uint32_t ssrc, int max_consider_lost_pkt_num) override;
  void OnFECPacket(const RtpPacketReceived& packet) override;
  void OnVideoPacket(const RtpPacketReceived& packet) override;
  void OnRecoverPacket(const RtpPacketReceived& packet) override;
  void OnRetransmitPacket(const RtpPacketReceived& packet) override;

 private:
  FECStreamStatisticianImplInterface* GetOrCreateStatistician(uint32_t ssrc);
  
  Clock* const clock_;
  std::function<std::unique_ptr<FECStreamStatisticianImplInterface>(uint32_t ssrc, Clock* clock)> stream_statistician_factory_;
  size_t last_returned_ssrc_idx_;
  std::vector<uint32_t> all_ssrcs_;
  flat_map<uint32_t /*ssrc*/, std::unique_ptr<FECStreamStatisticianImplInterface>> statisticians_;
};

// Thread-safe implementation wrapping access to FECReceiveStatisticsImpl with a mutex.
class FECReceiveStatisticsLocked : public FecReceiveStatistics {
 public:
  explicit FECReceiveStatisticsLocked(
    Clock* clock,
    std::function<std::unique_ptr<FECStreamStatisticianImplInterface>(
        uint32_t ssrc,
        Clock* clock)> stream_statitician_factory)
    : impl_(clock, std::move(stream_statitician_factory)) {}
  ~FECReceiveStatisticsLocked() override = default;
  std::vector<rtcp::FECReportBlock> RtcpReportBlocks(size_t max_blocks) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.RtcpReportBlocks(max_blocks);
  }
  FecStreamStatistician* GetStatistician(uint32_t ssrc) const override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.GetStatistician(ssrc);
  }
  void SetMaxReorderingThreshold(uint32_t ssrc, int max_reordering_threshold) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.SetMaxReorderingThreshold(ssrc, max_reordering_threshold);
  }
  void SetMaxConsiderLostPktNum(uint32_t ssrc, int max_consider_lost_pkt_num) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.SetMaxConsiderLostPktNum(ssrc, max_consider_lost_pkt_num);
  }
  void OnFECPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.OnFECPacket(packet);
  }
  void OnVideoPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.OnVideoPacket(packet);
  }
  void OnRecoverPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.OnRecoverPacket(packet);
  }
  void OnRetransmitPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.OnRetransmitPacket(packet);
  }
  
 private:
  mutable Mutex receive_statistics_lock_;
  FECReceiveStatisticsImpl impl_ RTC_GUARDED_BY(&receive_statistics_lock_);
};

}   // namespace webrtc
#endif  // EXAMPLES_MYFECEXP_RTCP_FEC_RECEIVE_STATISTICS_IMPL_H_