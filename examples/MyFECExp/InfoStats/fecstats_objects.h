
#ifndef EXAMPLES_MYFECEXP_INFOSTATS_FECSTATS_OBJECTS_H_
#define EXAMPLES_MYFECEXP_INFOSTATS_FECSTATS_OBJECTS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc{

class RTC_EXPORT RTCRemoteFECStats final : public RTCRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCRemoteFECStats(std::string id, Timestamp timestamp);
  ~RTCRemoteFECStats() override;

  std::optional<uint32_t> fraction_lost_recover;
  std::optional<uint32_t> fixed_fraction_lost_recover;
  std::optional<uint32_t> fraction_effective_FEC_pkt;
  std::optional<uint32_t> fixed_fraction_effective_FEC_pkt;
  std::optional<int32_t> fixed_average_recover_time;
  std::optional<int32_t> fixed_average_retransmit_time;
  std::optional<int32_t> fixed_time_ahead_for_recover;
  std::optional<int32_t> average_recover_time;
  std::optional<int32_t> average_retransmit_time;
  std::optional<int32_t> time_ahead_for_recover;
  std::optional<uint32_t> recover_pkt_num_raw;
  std::optional<uint32_t> cumulative_recover_pkt;
  std::optional<uint32_t> retransmit_pkt_num_raw;
  std::optional<uint32_t> cumulative_retransmit_pkt;
  std::optional<uint32_t> both_succ_pkt_num_raw;
  std::optional<uint32_t> cumulative_both_succ_pkt;
  std::optional<uint32_t> burst_lost_num;
  std::optional<uint32_t> cumulative_burst_lost_num;
};

}

#endif  // EXAMPLES_MYFECEXP_INFOSTATS_FECSTATS_OBJECTS_H_