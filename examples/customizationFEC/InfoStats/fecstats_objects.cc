
#include "examples/customizationFEC/InfoStats/fecstats_objects.h"

#include <utility>

#include "api/stats/attribute.h"
#include "api/stats/rtc_stats.h"
#include "rtc_base/checks.h"

namespace webrtc {

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCRemoteFECStats, RTCRtpStreamStats,
        "remote-fec-receiver",
    AttributeInit("fractionLostRecover", &fraction_lost_recover),
    AttributeInit("fixedFractionLostRecover", &fixed_fraction_lost_recover),
    AttributeInit("fractionEffectiveFECPkt", &fraction_effective_FEC_pkt),
    AttributeInit("fixedFractionEffectiveFECPkt", &fixed_fraction_effective_FEC_pkt),
    AttributeInit("fixedAverageRecoverTime", &fixed_average_recover_time),
    AttributeInit("fixedAverageRetransmitTime", &fixed_average_retransmit_time),
    AttributeInit("fixedTimeAheadForRecover", &fixed_time_ahead_for_recover),
    AttributeInit("AverageRecoverTime", &average_recover_time),
    AttributeInit("AverageRetransmitTime", &average_retransmit_time),
    AttributeInit("TimeAheadForRecover", &time_ahead_for_recover),
    AttributeInit("recoverPktNumRaw", &recover_pkt_num_raw),
    AttributeInit("cumulativeRecoverPkt", &cumulative_recover_pkt),
    AttributeInit("retransmitPktNumRaw", &retransmit_pkt_num_raw),
    AttributeInit("cumulativeRetransmitPkt", &cumulative_retransmit_pkt),
    AttributeInit("bothSuccPktNumRaw", &both_succ_pkt_num_raw),
    AttributeInit("cumulativeBothSuccPkt", &cumulative_both_succ_pkt),
    AttributeInit("burstLostNum", &burst_lost_num),
    AttributeInit("cumulativeBurstLostNum", &cumulative_burst_lost_num))
// clang-format on

RTCRemoteFECStats::RTCRemoteFECStats(
    std::string id,
    Timestamp timestamp)
    : RTCRtpStreamStats(std::move(id), timestamp) {}

RTCRemoteFECStats::~RTCRemoteFECStats() {}

}   // namespace webrtc