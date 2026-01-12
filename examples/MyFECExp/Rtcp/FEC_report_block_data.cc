
#include "examples/MyFECExp/Rtcp/FEC_report_block_data.h" 

#include "rtc_base/checks.h"

namespace webrtc {

void FECReportBlockData::SetFECReportBlock(uint32_t sender_ssrc,
                                           const rtcp::FECReportBlock& report_block,
                                           Timestamp report_block_timestamp_utc,
                                           Timestamp report_block_timestamp) {
    sender_ssrc_ = sender_ssrc;
    source_ssrc_ = report_block.source_ssrc();

    set_report_block_timestamp(report_block_timestamp_utc, report_block_timestamp);
    set_fraction_lost_recover(report_block.fraction_lost_recover());
    set_fraction_effective_FEC_pkt(report_block.fraction_effective_FEC_pkt());
    set_recover_pkt_num(report_block.recover_pkt_num());
    set_average_recover_time(report_block.average_recover_time(), report_block.recover_pkt_num());
    set_average_retransmit_time(report_block.average_retransmit_time(), report_block.retransmit_pkt_num());
    set_time_ahead_for_recover(report_block.time_ahead_for_recover(), report_block.both_pkt_num());
    set_retransmit_pkt_num(report_block.retransmit_pkt_num());
    set_both_succ_pkt_num(report_block.both_pkt_num());
    set_burst_lost_num(report_block.burst_lost_num());
}

}   // namespace webrtc