
#include "examples/MyFECExp/LModule/LModule.h"

#include <cmath>

namespace RLSRSFEC{

    uint32_t getL(LInput params) {
        double target_bytes_rate = params.target_bitrate/8;
        double L1_double = (target_bytes_rate*params.RTT) / params.s_pck;
        uint32_t L1 = static_cast<uint32_t>(std::floor(L1_double));
        double L2_double = static_cast<double>(params.fps) * params.RTT;
        uint32_t L2 = static_cast<uint32_t>(std::floor(L2_double));
        uint32_t L = std::max(L1,L2);
        // for FEC packet, the maximum number of protected packets is 32
        L = std::min(L, 32U); 
        L = std::max(L, 3U);
        return L;
    }

} // namespace RLSRSFEC