#ifndef EXAMPLES_MYFECEXP_RSFEC_IMODULE_IMODULE_H_
#define EXAMPLES_MYFECEXP_RSFEC_IMODULE_IMODULE_H_

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <list>

namespace RLSRSFEC{
    // struct IState{
    //     double inv_avg_lostRate;
    //     std::list<double> lostRate_list;
    //     std::list<double> RTT_list;
    //     double target_bitrate;
    // };
    struct IState{
        double EMALR_10_3;
        double EMAEFR_5_2;
        double EMALRR_5_2;
        double I_inv_EMALR_10;
        double EFR_LRR_diff;
        double M_5;
    };
    // struct IReward{
    //     double effective_fec_ratio;
    //     double lost_recovery_ratio;
    // };
    struct IReward{
        double EFR;
        double LRR;
        double EMAEFR_5;
        double EMALRR_5;
        double delta_I;
        double M;
        double EMALR_10_3;
    };

} // namespace RLSRSFEC


#endif // EXAMPLES_MYFECEXP_RSFEC_IMODULE_IMODULE_H_