
#include "examples/customizationFEC/Rtcp/FEC_report_block.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rtcp {

// Define by XZG
//
// RTCP FEC report block
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  0 |                 SSRC_1 (SSRC of first source)                 |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 | frac. Recover | frac. effctive|   Number of packets recover   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |                    Time Ahead for recover                     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |                     Average Recover Time                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |                   Average Retransmit Time                     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 20 | Number of packets retransmit  |   Number of packets both re.  |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 24 |   Number of burst lost pkts.  |            Padding            |
// 28 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


FECReportBlock::FECReportBlock()
    : source_ssrc_(0),
      fraction_lost_recover_(0),
      fraction_effective_FEC_pkt_(0),
      recover_pkt_num_(0),
      average_recover_time_(0),
      average_retransmit_time_(0),
      time_ahead_for_recover_(0),
      retransmit_pkt_num_(0),
      both_pkt_num_(0),
      burst_lost_num_(0) {}

bool FECReportBlock::Parse(const uint8_t* buffer, size_t length) {
    RTC_DCHECK(buffer != nullptr);
    if (length < FECReportBlock::kLength) {
        RTC_LOG(LS_ERROR) << "FEC Report Block should be 28 bytes long";
        return false;
    }

    source_ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[0]);
    fraction_lost_recover_ = buffer[4];
    fraction_effective_FEC_pkt_ = buffer[5];
    recover_pkt_num_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[6]);
    average_recover_time_ = ByteReader<int32_t>::ReadBigEndian(&buffer[8]);
    average_retransmit_time_ = ByteReader<int32_t>::ReadBigEndian(&buffer[12]);
    time_ahead_for_recover_ = ByteReader<int32_t>::ReadBigEndian(&buffer[16]);
    retransmit_pkt_num_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[20]);
    both_pkt_num_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[22]);
    burst_lost_num_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[24]);
    return true;
}

void FECReportBlock::Create(uint8_t* buffer) const {
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[0], source_ssrc());
    ByteWriter<uint8_t>::WriteBigEndian(&buffer[4], fraction_lost_recover());
    ByteWriter<uint8_t>::WriteBigEndian(&buffer[5], fraction_effective_FEC_pkt());
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[6], recover_pkt_num());
    ByteWriter<int32_t>::WriteBigEndian(&buffer[8], average_recover_time());
    ByteWriter<int32_t>::WriteBigEndian(&buffer[12], average_retransmit_time());
    ByteWriter<int32_t>::WriteBigEndian(&buffer[16], time_ahead_for_recover());
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[20], retransmit_pkt_num());
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[22], both_pkt_num());
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[24], burst_lost_num());
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[26], 0);
}

}   // namespace rtcp
}   // namespace webrtc
