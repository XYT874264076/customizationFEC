#ifndef EXAMPLES_MYFECEXP_RSFEC_LMODULE_LMODULE_H_
#define EXAMPLES_MYFECEXP_RSFEC_LMODULE_LMODULE_H_

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <list>

namespace RLSRSFEC{
    struct LInput{
        double target_bitrate;
        double RTT;
        double s_pck;
        uint32_t fps;
    };

    uint32_t getL(LInput params);
} // namespace RLSRSFEC


#endif  // EXAMPLES_MYFECEXP_RSFEC_LMODULE_LMODULE_H_