#ifndef EXAMPLES_MYFECEXP_RSFEC_MMODULE_MMODULE_H_
#define EXAMPLES_MYFECEXP_RSFEC_MMODULE_MMODULE_H_

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <list>

namespace RLSRSFEC{
    struct MState{

        double EMAburst_3;
        double EMAburst_3_10;
        double EMALR_3_5_10;
        double StdDevRTT_5;
        double RTT_EMARTT_30;
        double VarJitter_5;
        double I_inv_EMALR_10;
        double EMAM_5;

    };
    struct MReward{

        double LRR;
        double EMALRR_5;
        double M;
        double EMAburst_3;
        double delta_M;

    };

} // namespace RLSRSFEC


#endif // EXAMPLES_MYFECEXP_RSFEC_MMODULE_MMODULE_H_