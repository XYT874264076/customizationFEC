
#include "Params.h"

namespace inputV {
    std::string Params::playVideo;
    int32_t Params::duration;
    int32_t Params::interval_ms;
    int32_t Params::RL_interval_ms;
    std::string Params::output;
    ExpType Params::type;
    double Params::FecRate;
    int32_t Params::generate_fec_num;
    bool Params::ifTrainI;
    bool Params::ifSaveI;
    bool Params::ifTrainM;
    bool Params::ifSaveM;

    // Tambur parameters
    uint16_t Params::tambur_tau;
    int Params::tambur_stripe_size;
    uint16_t Params::tambur_w;
    int Params::tambur_max_data_stripes_per_frame;
    int Params::tambur_max_fec_stripes_per_frame;
    int Params::tambur_packet_size;
    uint8_t Params::tambur_max_qr;
    uint16_t Params::tambur_parity_delay;
    uint8_t Params::tambur_num_qrs_no_reduce;
} // namespace inputV

namespace transV {
    uint32_t Params::L;
    uint32_t Params::I;
    uint32_t Params::M;

    // Used to calculate current s_pck
    int32_t Params::last_packets_sent = 0;
    int32_t Params::last_bytes_sent = 0;

    // To record birtate list (length 30) for L-Module
    std::list<double> Params::cur_bitrate_list_30;
    // To record RTT list (length 30) for L-Module
    std::list<double> Params::cur_RTT_list_30;
    // To record pkt size list (length 30) for L-Module
    std::list<double> Params::cur_pkt_size_list_30;
    // To record lost rate list (length 10) for I-Module
    std::list<double> Params::cur_lostrate_list10;
    // To record lost rate list (length 3) for I-Module
    std::list<double> Params::cur_lostrate_list3;
     // To record EFR list (length 5) for I-Module
    std::list<double> Params::cur_EFR_list5;
    // To record EFR list (length 2) for I-Module
    std::list<double> Params::cur_EFR_list2;
    // To record LRR list (length 5) for I-Module
    std::list<double> Params::cur_LRR_list5;
    // To record LRR list (length 2) for I-Module
    std::list<double> Params::cur_LRR_list2;
    // To record last action for I-Module;
    int Params::delta_I = 0;

    // To record burst list (length 3) for M-Module
    std::list<double> Params::cur_burst_list3;
    // To record burst list (length 10) for M-Module
    std::list<double> Params::cur_burst_list10;
    // To record lost rate list (length 5) for M-Module
    std::list<double> Params::cur_lostrate_list5;
    // To record RTT list (length 5) for M-Module
    std::list<double> Params::cur_RTT_list5;
    // To record jitter list (length 5) for M-Module
    std::list<double> Params::cur_jitter_list5;
    // To record M list (length 5) for M-Module
    std::list<double> Params::cur_M_list5;
    // To record last action for M-Module;
    int Params::last_M = 1;
    // To record delta M for M-Module;
    double Params::delta_M = 0;

    // We do not do RL when start
    int32_t Params::do_RL_times = 0;

    // record baseline reward
    double Params::baseline_Ireward = 0.0L;
    double Params::baseline_Mreward = 0.0L;

    // note if the python interpreter has been init!
    int Params::py_init = 0;

    uint8_t Params::tambur_qr_state = 1;

    double list_avg(const std::list<double>& cur_list) {
        double ret = 0;
        auto it = cur_list.cbegin();
        while (it != cur_list.end()) {
            ret += (*it);
            it++;
        }
        if (cur_list.size() > 0) {
            return ret / cur_list.size();
        }
        else {
            return ret;
        }
    }

    double list_stdDev(const std::list<double>& cur_list) {
        double ret = 0, sum = 0, sqSum = 0;
        if (cur_list.size() == 0) return ret;
        auto it = cur_list.cbegin();
        while (it != cur_list.end()) {
            sum += (*it);
            it++;
        }
        double avg = sum / cur_list.size();
        it = cur_list.cbegin();
        while (it != cur_list.end()) {
            sqSum += ((*it)-avg)*((*it)-avg);
            it++;
        }
        ret = sqSum / cur_list.size();
        return std::sqrt(ret);
    }

    bool checkLState(const RLSRSFEC::LInput& param) {

        if (std::isnan(param.target_bitrate)) return false;
        if (std::isnan(param.RTT)) return false;
        if (std::isnan(param.s_pck)) return false;

        return true;
    }

    std::vector<double> IState_to_vector(const RLSRSFEC::IState& state) {
        std::vector<double> result;
        
        size_t total_elements = 6;
        result.reserve(total_elements);

        result.push_back(state.EMALR_10_3);
        result.push_back(state.EMAEFR_5_2);
        result.push_back(state.EMALRR_5_2);
        result.push_back(state.I_inv_EMALR_10);
        result.push_back(state.EFR_LRR_diff);
        result.push_back(state.M_5);

        return result;
    }

    double calculate_Ireward(const RLSRSFEC::IReward& reward, double alpha, double beta, double gamma, double lambda1, double lambda2, double lambda3, double lambda4) {
        double result = 0;
        double short_term_reward = alpha*reward.EFR + beta*reward.LRR;
        double long_term_reward = alpha*reward.EMAEFR_5 + beta*reward.EMALRR_5;
        // double short_term_reward = reward.EFR*std::sqrt(reward.LRR);
        // double long_term_reward = reward.EMAEFR_5*std::sqrt(reward.EMALRR_5);
        result += lambda1*(gamma*short_term_reward + (1-gamma)*long_term_reward);
        result -= lambda2*abs(reward.delta_I);
        result -= lambda3*reward.delta_I*(reward.M-1);
        result += lambda4*reward.delta_I*reward.EMALR_10_3;
        return result;
    }

    bool checkIState(const RLSRSFEC::IState& state) {
        
        if (std::isnan(state.EMALR_10_3)) return false;
        if (std::isnan(state.EMAEFR_5_2)) return false;
        if (std::isnan(state.EMALRR_5_2)) return false;
        if (std::isnan(state.I_inv_EMALR_10)) return false;
        if (std::isnan(state.EFR_LRR_diff)) return false;

        return true;
    }

    bool checkIReward(const RLSRSFEC::IReward& reward) {

        if (std::isnan(reward.EFR)) return false;
        if (std::isnan(reward.LRR)) return false;
        if (std::isnan(reward.EMAEFR_5)) return false;
        if (std::isnan(reward.EMALRR_5)) return false;
        if (std::isnan(reward.EMALR_10_3)) return false;
        return true;
    }

    std::vector<double> MState_to_vector(const RLSRSFEC::MState& state) {
        std::vector<double> result;
        
        size_t total_elements = 8;
        result.reserve(total_elements);

        result.push_back(state.EMAburst_3);
        result.push_back(state.EMAburst_3_10);
        result.push_back(state.EMALR_3_5_10);
        result.push_back(state.StdDevRTT_5);
        result.push_back(state.RTT_EMARTT_30);
        result.push_back(state.VarJitter_5);
        result.push_back(state.I_inv_EMALR_10);
        result.push_back(state.EMAM_5);
        
        return result;
    }

    double calculate_Mreward(const RLSRSFEC::MReward& reward, double lambda1, double lambda2, double lambda3, double lambda4) {
        double result = 0;
        double LRR_Inc = (reward.LRR - reward.EMALRR_5) / (1.0L - reward.EMALRR_5 + 0.0001);
        result += lambda1*LRR_Inc;
        result -= lambda2*reward.M*reward.M / 16.0L;
        // result += lambda3/(1+reward.EMAburst_3);
        // result += lambda3*reward.LRR;
        result -= lambda4*reward.delta_M;
        return result;
    }

    bool checkMState(const RLSRSFEC::MState& state) {
        if (std::isnan(state.EMAburst_3)) return false;
        if (std::isnan(state.EMAburst_3_10)) return false;
        if (std::isnan(state.EMALR_3_5_10)) return false;
        if (std::isnan(state.StdDevRTT_5)) return false;
        if (std::isnan(state.RTT_EMARTT_30)) return false;
        if (std::isnan(state.VarJitter_5)) return false;
        if (std::isnan(state.I_inv_EMALR_10)) return false;
        return true;
    }

    bool checkMReward(const RLSRSFEC::MReward& reward) {
        if (std::isnan(reward.LRR)) return false;
        if (std::isnan(reward.EMALRR_5)) return false;
        if (std::isnan(reward.EMAburst_3)) return false;
        return true;
    }

} // namespace transV