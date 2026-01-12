
#ifndef EXAMPLES_CUSTOMIZATION_RTCP_FEC_STATISTICS_H_
#define EXAMPLES_CUSTOMIZATION_RTCP_FEC_STATISTICS_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "call/rtp_packet_sink_interface.h"
#include "examples/customizationFEC/Rtcp/FEC_report_block.h"

namespace webrtc {

class Clock;

class FecReceiveStatisticsProvider {
    public:
     virtual ~FecReceiveStatisticsProvider() = default;
     // Collects receive statistic in a form of rtcp report blocks.
     // Returns at most `max_blocks` report blocks.
     virtual std::vector<rtcp::FECReportBlock> RtcpReportBlocks(size_t max_blocks) = 0;
};

class FecStreamStatistician {
 public:
  virtual ~FecStreamStatistician();
  
// Here maybe will return some FEC metrics for corresponding ssrc.
}; 

class FecReceiveStatistics : public FecReceiveStatisticsProvider {
 public:
  ~FecReceiveStatistics() override = default;

  // Returns a thread-safe instance of ReceiveStatistics.
  static std::unique_ptr<FecReceiveStatistics> Create(Clock* clock);
  // Returns a thread-compatible instance of ReceiveStatistics.
  static std::unique_ptr<FecReceiveStatistics> CreateThreadCompatible(Clock* clock);

  // Returns a pointer to the statistician of an ssrc.
  virtual FecStreamStatistician* GetStatistician(uint32_t ssrc) const = 0;

  // Sets the max reordering threshold in number of packets.
  virtual void SetMaxReorderingThreshold(uint32_t ssrc, int max_reordering_threshold) = 0;
  virtual void SetMaxConsiderLostPktNum(uint32_t ssrc, int max_consider_lost_pkt_num) = 0;

  // Receive various packets and update statistics.
  virtual void OnFECPacket(const RtpPacketReceived& packet) = 0;
  virtual void OnVideoPacket(const RtpPacketReceived& packet) = 0;
  virtual void OnRecoverPacket(const RtpPacketReceived& packet) = 0;
  virtual void OnRetransmitPacket(const RtpPacketReceived& packet) = 0;
};

}   // namespace webrtc


#endif  //EXAMPLES_CUSTOMIZATION_RTCP_FEC_STATISTICS_H_