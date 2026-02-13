#ifndef PARAMS_H_
#define PARAMS_H_

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <list>
#include <cmath>

#include "examples/customizationFEC/LModule/LModule.h"
#include "examples/customizationFEC/IModule/IModule.h"
#include "examples/customizationFEC/MModule/MModule.h"

namespace inputV{

enum ExpType{
  WebRTCSource,
  WebRTCFlexFEC,
  StableRate,
  RSFECBlock,
  RSFECStreamStableRate,
  RSFECStreamSourceRate,
  RLSRSFEC,
  FECClose,
  TamburFEC
};

class Params{
public:
 
 static int doAssign(std::string& value, std::string& name){
    int32_t param_value_int;
    double param_value_double;
    // Assign string value:
    if (name=="playVideo"){
        playVideo=value;
        return 0;
    }
    if (name=="output"){
        output=value;
        if (output.size()>0 && output[output.size()-1]!='/'){
            output=output+"/";
        }
        return 0;
    }
    if (name=="ifTrainI"){
        if (value=="true") {
            ifTrainI = true;
        }
        else {
            ifTrainI = false;
        }
    }
    if (name=="ifSaveI") {
        if (value=="true") {
            ifSaveI = true;
        }
        else {
            ifSaveI = false;
        }
    }
    if (name=="ifTrainM"){
        if (value=="true") {
            ifTrainM = true;
        }
        else {
            ifTrainM = false;
        }
    }
    if (name=="ifSaveM") {
        if (value=="true") {
            ifSaveM = true;
        }
        else {
            ifSaveM = false;
        }
    }
    if (name=="type"){
        if (value=="WebRTCSource"){
            type = ExpType::WebRTCSource;
        }
        else if (value=="WebRTCFlexFEC"){
            type = ExpType::WebRTCFlexFEC;
        }
        else if (value=="StableRate"){
            type = ExpType::StableRate;
        }
        else if (value=="RSFECBlock"){
            type = ExpType::RSFECBlock;
        }
        else if (value=="RSFECStreamStableRate"){
            type = ExpType::RSFECStreamStableRate;
        }
        else if (value=="RSFECStreamSourceRate"){
            type = ExpType::RSFECStreamSourceRate;
        }
        else if (value=="RLSRSFEC"){
            type = ExpType::RLSRSFEC;
        }
        else if (value=="FECClose"){
            type = ExpType::FECClose;
        }
        else if (value=="TamburFEC"){
            type = ExpType::TamburFEC;
        }
        else {
            std::cerr<<"Warning: Unknown ExpType "<< value <<"! Still use WebRTCSource as default!" << std::endl;
        }
        return 0;
    }

    // Asign int value:
    if (name=="interval"){
        param_value_int = std::stoi(value);
        interval_ms = param_value_int;
    }
    if (name=="RLInterval") {
        param_value_int = std::stoi(value);
        RL_interval_ms = param_value_int;
    }
    if (name=="duration"){
        param_value_int = std::stoi(value);
        duration = param_value_int;
    }
    if (name=="FECRate"){
        param_value_double = std::stod(value);
        FecRate = param_value_double;
    }
    if (name=="FECNum") {
        param_value_int = std::stoi(value);
        generate_fec_num = param_value_int;
    }

    if (name=="tamburTau"){
        param_value_int = std::stoi(value);
        tambur_tau = param_value_int;
    }
    if (name=="tamburStripeSize"){
        param_value_int = std::stoi(value);
        tambur_stripe_size = param_value_int;
    }
    if (name=="tamburW"){
        param_value_int = std::stoi(value);
        tambur_w = param_value_int;
    }
    if (name=="tamburMaxDataStripesPerFrame"){
        param_value_int = std::stoi(value);
        tambur_max_data_stripes_per_frame = param_value_int;
    }
    if (name=="tamburMaxFecStripesPerFrame"){
        param_value_int = std::stoi(value);
        tambur_max_fec_stripes_per_frame = param_value_int;
    }
    if (name=="tamburPacketSize"){
        param_value_int = std::stoi(value);
        tambur_packet_size = param_value_int;
    }
    if (name=="tamburMaxQr"){
        param_value_int = std::stoi(value);
        tambur_max_qr = param_value_int;
    }
    if (name=="tamburParityDelay"){
        param_value_int = std::stoi(value);
        tambur_parity_delay = param_value_int;
    }
    if (name=="tamburParityDelay"){
        param_value_int = std::stoi(value);
        tambur_parity_delay = param_value_int;
    }
    if (name=="tamburNumQrsNoReduce"){
        param_value_int = std::stoi(value);
        tambur_num_qrs_no_reduce = param_value_int;
    }

    return 0;
 }

 static int init(int argc, char* argv[]){
    duration = 300;
    interval_ms = 1000;
    type = ExpType::RLSRSFEC;
    playVideo = "/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo1.mp4";
    output = "./tempOut/";
    FecRate = 0.1;
    generate_fec_num = 1;
    RL_interval_ms = 1000;
    ifTrainI = false;
    ifSaveI = false;
    ifTrainM = false;
    ifSaveM = false;

    // Tambur params
    tambur_tau = 3; // default FEC protection window size 
    tambur_stripe_size = 256; // default stripe size in bytes
    tambur_w = 32; // Default Galois field size
    tambur_max_data_stripes_per_frame = 64; // Default Max data stripes per frame
    tambur_max_fec_stripes_per_frame = 32; // Default Max FEC stripes per frame
    tambur_packet_size = 8; // Default Packet size 
    tambur_max_qr = 1; // Default Max QR value
    tambur_parity_delay = 3; // Default Parity delay
    tambur_num_qrs_no_reduce = 2; // Default Num QRs no reduce

    for (int i=1;i<argc;i++){
        std::string arg = argv[i];
        if (arg.substr(0,2)=="--"){
            std::string param_name = arg.substr(2,arg.size());
            std::string param_value = argv[i+1];
            if (doAssign(param_value, param_name)==-1){
                std::cerr<<"Assign parameters fail!"<<std::endl;
                return -1;
            }
            i++;
        }
    }
    
    return 0;
 }

 static std::string playVideo;
 static int32_t duration;
 static int32_t interval_ms;
 static std::string output;
 static ExpType type;
 static double FecRate;
 static int32_t generate_fec_num;
 static int32_t RL_interval_ms;
 static bool ifTrainI;
 static bool ifSaveI;
 static bool ifTrainM;
 static bool ifSaveM;

 // Tambur params
 static uint16_t tambur_tau;
 static int tambur_stripe_size;
 static uint16_t tambur_w;
 static int tambur_max_data_stripes_per_frame; 
 static int tambur_max_fec_stripes_per_frame;
 static int tambur_packet_size;  
 static uint8_t tambur_max_qr;
 static uint16_t tambur_parity_delay;
 static uint8_t tambur_num_qrs_no_reduce;
};
} // namespace inputV

namespace transV{

class Params{
public:
 static uint32_t L;
 static uint32_t I;
 static uint32_t M;

 // Used to calculate current s_pck
 static int32_t last_packets_sent;
 static int32_t last_bytes_sent;


 // To record birtate list (length 30) for L-Module
 static std::list<double> cur_bitrate_list_30;
 // To record RTT list (length 30) for L-Module & M-Module
 static std::list<double> cur_RTT_list_30;
 // To record pkt size list (length 30) for L-Module
 static std::list<double> cur_pkt_size_list_30;

 // To record lost rate list (length 10) for I-Module & M-Module
 static std::list<double> cur_lostrate_list10;
 // To record lost rate list (length 3) for I-Module & M-Module
 static std::list<double> cur_lostrate_list3;
 // To record EFR list (length 5) for I-Module
 static std::list<double> cur_EFR_list5;
 // To record EFR list (length 2) for I-Module
 static std::list<double> cur_EFR_list2;
 // To record LRR list (length 5) for I-Module
 static std::list<double> cur_LRR_list5;
 // To record LRR list (length 2) for I-Module & M-Module
 static std::list<double> cur_LRR_list2;
 // To record last action for I-Module;
 static int delta_I;
 
 // To record burst list (length 3) for M-Module
 static std::list<double> cur_burst_list3;
 // To record burst list (length 10) for M-Module
 static std::list<double> cur_burst_list10;
 // To record lost rate list (length 5) for M-Module
 static std::list<double> cur_lostrate_list5;
 // To record RTT list (length 5) for M-Module
 static std::list<double> cur_RTT_list5;
 // To record jitter list (length 5) for M-Module
 static std::list<double> cur_jitter_list5;
 // To record M list (length 5) for M-Module
 static std::list<double> cur_M_list5;
 // To record last action for M-Module;
 static int last_M;
 // To record delta M for M-Module;
 static double delta_M;

 // We do not do RL when start
 static int32_t do_RL_times;

 // record baseline reward
 static double baseline_Ireward;
 static double baseline_Mreward;

 // note if the python interpreter has been init!
 static int py_init;
};

template<typename T> std::string list_tostring(const std::list<T>& cur_list) {
    std::string ret = "";
    auto it = cur_list.cbegin();
    while (it != cur_list.end()) {
        if (it == cur_list.cbegin()) {
            ret = ret + std::to_string(*it);
        }
        else {
            ret = ret + "_" + std::to_string(*it);
        }
        it++;
    }
    return ret;
}

double list_avg(const std::list<double>& cur_list);
double list_stdDev(const std::list<double>& cur_list);

bool checkLState(const RLSRSFEC::LInput& param);

std::vector<double> IState_to_vector(const RLSRSFEC::IState& state);
double calculate_Ireward(const RLSRSFEC::IReward& reward, double alpha, double beta, double gamma, double lambda1, double lambda2, double lambda3, double lambda4);
bool checkIState(const RLSRSFEC::IState& state);
bool checkIReward(const RLSRSFEC::IReward& reward);

std::vector<double> MState_to_vector(const RLSRSFEC::MState& state);
double calculate_Mreward(const RLSRSFEC::MReward& reward, double lambda1, double lambda2, double lambda3, double lambda4);
bool checkMState(const RLSRSFEC::MState& state);
bool checkMReward(const RLSRSFEC::MReward& reward);

} // namespace transV

#endif  // PARAMS_H_