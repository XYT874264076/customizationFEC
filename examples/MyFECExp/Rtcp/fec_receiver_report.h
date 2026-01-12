
#ifndef EXAMPLES_MYFECEXP_RTCP_FEC_RECEIVER_REPORT_H_
#define EXAMPLES_MYFECEXP_RTCP_FEC_RECEIVER_REPORT_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "examples/MyFECExp/Rtcp/FEC_report_block.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

class FecReceiverReport : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 208;
  static constexpr size_t kMaxNumberOfReportBlocks = 0x1f;

  FecReceiverReport();
  FecReceiverReport(const FecReceiverReport&);
  ~FecReceiverReport() override;

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  bool AddReportBlock(const FECReportBlock& block);
  bool SetReportBlocks(std::vector<FECReportBlock> blocks);

  const std::vector<FECReportBlock>& report_blocks() const {
    return report_blocks_;
  }

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
    size_t* index,
    size_t max_length,
    PacketReadyCallback callback) const override;

 private:
  static constexpr size_t kRrBaseLength = 4;
  std::vector<FECReportBlock> report_blocks_;
};

}   // namespace rtcp
}   // namespace webrtc
#endif  //  EXAMPLES_MYFECEXP_RTCP_FEC_RECEIVER_REPORT_H_