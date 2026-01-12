
#include "examples/customizationFEC/conductor.h"

#include <stddef.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <fstream>

#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/enable_media.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/create_frame_generator.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "examples/customizationFEC/defaults.h"
#include "examples/customizationFEC/main_wnd.h"
#include "examples/customizationFEC/peer_connection_client.h"
#include "json/reader.h"
#include "json/value.h"
#include "json/writer.h"
#include "examples/customizationFEC/video_player.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "system_wrappers/include/clock.h"
#include "examples/customizationFEC/frame_generator_player.h"
#include "examples/customizationFEC/MP4VideoPlayer.h"
#include "examples/customizationFEC/Params.h"
#include "examples/customizationFEC/LModule/LModule.h" 
#include "examples/customizationFEC/MModule/MModule.h"
#include "examples/customizationFEC/IModule/IModule.h"
#include "examples/customizationFEC/IModule/IModulePyWrapper.h"
#include "examples/customizationFEC/MModule/MModulePyWrapper.h"


namespace {
using webrtc::test::MP4VideoPlayer;

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
 public:
  static rtc::scoped_refptr<DummySetSessionDescriptionObserver> Create() {
    return rtc::make_ref_counted<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() { RTC_LOG(LS_INFO) << __FUNCTION__; }
  virtual void OnFailure(webrtc::RTCError error) {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": " << error.message();
  }
};

class RTCStatsObserver : virtual public webrtc::RTCStatsCollectorCallback {
  public:
   virtual void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report){
     std::string jsonStr = report->ToJson();
     Json::Value root;
     Json::Reader reader;
 
     if (!reader.parse(jsonStr, root)){
       std::cerr << "Failed to parse JSON: " << reader.getFormatedErrorMessages();
       return;
     }
 
    //  std::cout<<jsonStr<<std::endl;
 
     std::ofstream inboundFile(inputV::Params::output+"inbound.csv",std::ios::app);
     std::ofstream outboundFile(inputV::Params::output+"outbound.csv",std::ios::app);
     std::ofstream candidateFile(inputV::Params::output+"candidate_pair.csv",std::ios::app);
     std::ofstream remoteinbound(inputV::Params::output+"remote_inbound.csv",std::ios::app);
     std::ofstream remoteoutbound(inputV::Params::output+"remote_outbound.csv",std::ios::app);
     std::ofstream remotefecFile(inputV::Params::output+"remote_fec.csv",std::ios::app);
     //Write file!
 
     for (const auto& item : root) {
         std::string type = item["type"].asString();
         if (type == "inbound-rtp") {
             inboundFile << item["timestamp"].asUInt64() << ","
                         << item["jitter"].asDouble() << ","
                         << item["packetsLost"].asInt() << ","
                         << item["packetsReceived"].asUInt() << ","
                         << item["bytesReceived"].asUInt() << ","
                         << item["retransmittedPacketsReceived"].asUInt() << ","
                         << item["retransmittedBytesReceived"].asUInt() << ","
                         << item["jitterBufferDelay"].asDouble() << ","
                         << item["jitterBufferEmittedCount"].asUInt() << ","
                         << item["jitterBufferTargetDelay"].asDouble() << ","
                         << item["jitterBufferMinimumDelay"].asDouble() << ","
                         << item["framesReceived"].asUInt() << ","
                         << item["frameWidth"].asUInt() << ","
                         << item["frameHeight"].asUInt() << ","
                         << item["framesPerSecond"].asUInt() << ","
                         << item["framesDecoded"].asUInt() << ","
                         << item["keyFramesDecoded"].asUInt() << ","
                         << item["framesDropped"].asUInt() << ","
                         << item["totalDecodeTime"].asDouble() << ","
                         << item["totalInterFrameDelay"].asDouble() << ","
                         << item["totalSquaredInterFrameDelay"].asDouble() << ","
                         << item["freezeCount"].asUInt() << ","
                         << item["totalFreezesDuration"].asDouble() << ","
                         << item["pauseCount"].asUInt() << ","
                         << item["totalPausesDuration"].asDouble() << ","
                         << item["totalAssemblyTime"].asDouble() << ","
                         << item["framesAssembledFromMultiplePackets"].asUInt() << ","
                         << item["totalProcessingDelay"].asDouble() << ","
                         << item["firCount"].asUInt() << ","
                         << item["pliCount"].asUInt() << ","
                         << item["nackCount"].asUInt() << ","
                         << item["qpSum"].asDouble() << ","
                         << item["minPlayoutDelay"].asDouble()<< "\n";
         } else if (type == "outbound-rtp") {
             outboundFile << item["timestamp"].asUInt64() << ","
                          << item["packetsSent"].asUInt() << ","
                          << item["bytesSent"].asUInt() << ","
                          << item["retransmittedPacketsSent"].asUInt() << ","
                          << item["retransmittedBytesSent"].asUInt() << ","
                          << item["targetBitrate"].asUInt() << ","
                          << item["framesEncoded"].asUInt() << ","
                          << item["keyFramesEncoded"].asUInt() << ","
                          << item["totalEncodeTime"].asDouble() << ","
                          << item["totalEncodedBytesTarget"].asUInt() << ","
                          << item["frameWidth"].asUInt() << ","
                          << item["frameHeight"].asUInt() << ","
                          << item["framesPerSecond"].asUInt() << ","
                          << item["framesSent"].asUInt() << ","
                          << item["totalPacketSendDelay"].asDouble() << ","
                          << item["nackCount"].asUInt() << ","
                          << item["qualityLimitationReason"] << ","
                          << item["qualityLimitationResolutionChanges"] << ","
                          << item["firCount"] << ","
                          << item["pliCount"] << "\n";
         } else if (type == "candidate-pair" && item["packetsReceived"].asUInt64()>0) {
             candidateFile << item["timestamp"].asUInt64() << ","
                           << item["totalRoundTripTime"].asDouble() << ","
                           << item["currentRoundTripTime"].asDouble() << ","
                           << item["availableOutgoingBitrate"].asDouble() << ","
                           << item["packetsSent"].asUInt() << ","
                           << item["packetsReceived"] << "," 
                           << item["bytesSent"] << ","
                           << item["bytesReceived"] << "\n";
         } else if (type == "remote-inbound-rtp") {
             remoteinbound << item["timestamp"].asUInt64() << ","
                           << item["jitter"].asDouble() << ","
                           << item["packetsLost"].asUInt64() << ","
                           << item["fractionLost"].asDouble() << ","
                           << item["roundTripTime"].asDouble() << ","
                           << item["totalRoundTripTime"].asDouble() << ","
                           << item["roundTripTimeMeasurements"].asUInt64() << "\n";
         } else if (type == "remote-outbound-rtp") {
             remoteoutbound << item["timestamp"].asUInt64() << ","
                            << item["packetsSent"].asUInt64() << ","
                            << item["bytesSent"].asUInt64() << ","
                            << item["reportsSent"].asUInt64() << ","
                            << item["totalRoundTripTime"].asDouble() << ","
                            << item["roundTripTimeMeasurements"].asUInt64() << "\n";
         } else if (type == "remote-fec-receiver") {
             remotefecFile << item["timestamp"].asUInt64() << ","
                           << item["fractionLostRecover"].asUInt64() << ","
                           << item["fixedFractionLostRecover"].asUInt64() << ","
                           << item["fractionEffectiveFECPkt"].asUInt64() << ","
                           << item["fixedFractionEffectiveFECPkt"].asUInt64() << ","
                           << item["fixedAverageRecoverTime"].asInt64() << ","
                           << item["fixedAverageRetransmitTime"].asInt64() << ","
                           << item["fixedTimeAheadForRecover"].asInt64() << ","
                           << item["AverageRecoverTime"].asInt64() << ","
                           << item["AverageRetransmitTime"].asInt64() << ","
                           << item["TimeAheadForRecover"].asInt64() << ","
                           << item["recoverPktNumRaw"].asUInt64() << ","
                           << item["cumulativeRecoverPkt"].asUInt64() << ","
                           << item["retransmitPktNumRaw"].asUInt64() << ","
                           << item["cumulativeRetransmitPkt"].asUInt64() << ","
                           << item["bothSuccPktNumRaw"].asUInt64() << ","
                           << item["cumulativeBothSuccPkt"].asUInt64() << ","
                           << item["burstLostNum"].asUInt64() << ","
                           << item["cumulativeBurstLostNum"].asUInt64() << ","
                           << item["fixedFractionEffectiveFECPkt"].asDouble() / 255.0L <<","
                           << item["fixedFractionLostRecover"].asDouble() / 255.0L << "\n";
         }
     }
 
     inboundFile.close();
     outboundFile.close();
   }
 };

class FECStatsObserver : virtual public webrtc::RTCStatsCollectorCallback {
  public:
   virtual void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report){
     std::string jsonStr = report->ToJson();
     Json::Value root;
     Json::Reader reader;

    //  std::cout<<"Run FECStatsObserver!!"<<std::endl;
 
     if (!reader.parse(jsonStr, root)){
       std::cerr << "Failed to parse JSON: " << reader.getFormatedErrorMessages();
       return;
     }

     //Record file timestamp!
     uint64_t rec_timestamp;
    
     // L-Module parameters
     RLSRSFEC::LInput L_params;
     
     // I-Module parameters
     RLSRSFEC::IState I_states;
     RLSRSFEC::IReward I_rewards;
     I_states.M_5 = static_cast<double>(transV::Params::M) / 5;
     I_rewards.delta_I = static_cast<double>(transV::Params::delta_I);
     I_rewards.M = static_cast<double>(transV::Params::M);
     int default_I;

     // M-Module parameters
     RLSRSFEC::MState M_states;
     RLSRSFEC::MReward M_rewards;
     M_states.EMAM_5 = transV::list_avg(transV::Params::cur_M_list5);
     M_rewards.M = transV::Params::last_M;
     M_rewards.delta_M = transV::Params::delta_M;
 
     // Do parameters assign!
     for (const auto& item : root) {
         std::string type = item["type"].asString();
         if (type == "inbound-rtp") {
            
         } else if (type == "outbound-rtp") {

            // Record file timestamp
            rec_timestamp = item["timestamp"].asUInt64();

            // fps for L Module
            L_params.fps = item["framesPerSecond"].asUInt();

            // s_pck for L Module
            while (transV::Params::cur_pkt_size_list_30.size() >= 30) {
              transV::Params::cur_pkt_size_list_30.pop_front();
            }
            transV::Params::cur_pkt_size_list_30.push_back(static_cast<double>(item["bytesSent"].asUInt() - transV::Params::last_bytes_sent) / 
                                                           static_cast<double>(item["packetsSent"].asUInt() - transV::Params::last_packets_sent));
            L_params.s_pck = transV::list_avg(transV::Params::cur_pkt_size_list_30);
            transV::Params::last_bytes_sent = item["bytesSent"].asUInt();
            transV::Params::last_packets_sent = item["packetsSent"].asUInt();

            // targetBitrate for L Module
            while (transV::Params::cur_bitrate_list_30.size() >= 30) {
              transV::Params::cur_bitrate_list_30.pop_front();
            }
            transV::Params::cur_bitrate_list_30.push_back(item["targetBitrate"].asDouble());
            L_params.target_bitrate = transV::list_avg(transV::Params::cur_bitrate_list_30);

         } else if (type == "candidate-pair" && item["packetsReceived"].asUInt64()>0) {

            // RTT for L Module
            while (transV::Params::cur_RTT_list_30.size()>=30) {
              transV::Params::cur_RTT_list_30.pop_front();
            }
            transV::Params::cur_RTT_list_30.push_back(item["currentRoundTripTime"].asDouble());
            L_params.RTT = transV::list_avg(transV::Params::cur_RTT_list_30);

            // StdDevRTT_5 for M Module
            while (transV::Params::cur_RTT_list5.size()>=5) {
              transV::Params::cur_RTT_list5.pop_front();
            }
            transV::Params::cur_RTT_list5.push_back(item["currentRoundTripTime"].asDouble());
            M_states.StdDevRTT_5 = transV::list_stdDev(transV::Params::cur_RTT_list5);
    
            // RTT_EMARTT_30 for M Module
            if (L_params.RTT == 0) {
              M_states.RTT_EMARTT_30 = 0;
            }
            else {
              M_states.RTT_EMARTT_30 = item["currentRoundTripTime"].asDouble() / L_params.RTT;
            }

         } else if (type == "remote-inbound-rtp") {

            // lostRate_list10 for I Module
            while (transV::Params::cur_lostrate_list10.size() >= 10) {
              transV::Params::cur_lostrate_list10.pop_front();
            }
            transV::Params::cur_lostrate_list10.push_back(item["fractionLost"].asDouble());

            // lostRate_list3 for I Module
            while (transV::Params::cur_lostrate_list3.size() >= 3) {
              transV::Params::cur_lostrate_list3.pop_front();
            }
            transV::Params::cur_lostrate_list3.push_back(item["fractionLost"].asDouble());

            // lostRate_list5 for M Module
            while (transV::Params::cur_lostrate_list5.size() >= 5) {
              transV::Params::cur_lostrate_list5.pop_front();
            }
            transV::Params::cur_lostrate_list5.push_back(item["fractionLost"].asDouble());
            
            // calculate EMALR_10, EMALA_5, and EMALR_3
            double EMALR_10 = transV::list_avg(transV::Params::cur_lostrate_list10);
            double EMALR_5 = transV::list_avg(transV::Params::cur_lostrate_list5);
            double EMALR_3 = transV::list_avg(transV::Params::cur_lostrate_list3);

            // calculate EMALR_10_3 for I Module
            I_states.EMALR_10_3 = EMALR_10 - EMALR_3;
            I_rewards.EMALR_10_3 = EMALR_10 - EMALR_3;

            // calculate EMALR_3_5_10 for M Module
            M_states.EMALR_3_5_10 = EMALR_3 - 2*EMALR_5 + EMALR_10;

            // calculate I_inv_EMALR_10 for I Module & M Module
            double target_I;
            if (EMALR_10 == 0) {
              target_I = transV::Params::L;
            }
            else {
              target_I = 1.0L / (EMALR_10*2.5);
              if (target_I > transV::Params::L) {
                target_I = transV::Params::L;
              }
              if (target_I < 1.0L) {
                target_I = 1.0L;
              }
            }
            I_states.I_inv_EMALR_10 = transV::Params::I / target_I;
            M_states.I_inv_EMALR_10 = transV::Params::I / target_I;

            // Init default_I
            default_I = target_I;

            // cur_jetter_list5 for M Module
            while (transV::Params::cur_jitter_list5.size()>=5) {
              transV::Params::cur_jitter_list5.pop_front();
            }
            transV::Params::cur_jitter_list5.push_back(item["jitter"].asDouble());
            M_states.VarJitter_5 = transV::list_stdDev(transV::Params::cur_jitter_list5);

         } else if (type == "remote-outbound-rtp") {
            
         } else if (type == "remote-fec-receiver") {

            // cur_EFR_list5 & cur_EFR_list2 for I-Module
            while (transV::Params::cur_EFR_list5.size() >= 5) {
              transV::Params::cur_EFR_list5.pop_front();
            }
            transV::Params::cur_EFR_list5.push_back(item["fractionEffectiveFECPkt"].asDouble() / 255.0L);
            while (transV::Params::cur_EFR_list2.size() >= 2) {
              transV::Params::cur_EFR_list2.pop_front();
            }
            transV::Params::cur_EFR_list2.push_back(item["fractionEffectiveFECPkt"].asDouble() / 255.0L);

            // cur_LRR_list5 & cur_LRR_list2 for I-Module
            while (transV::Params::cur_LRR_list5.size() >= 5) {
              transV::Params::cur_LRR_list5.pop_front();
            }
            transV::Params::cur_LRR_list5.push_back(item["fractionLostRecover"].asDouble() / 255.0L);
            while (transV::Params::cur_LRR_list2.size() >= 2) {
              transV::Params::cur_LRR_list2.pop_front();
            }
            transV::Params::cur_LRR_list2.push_back(item["fractionLostRecover"].asDouble() / 255.0L);

            // calculate EMAEFR_5 & EMAEFR_2
            double EMAEFR_5 = transV::list_avg(transV::Params::cur_EFR_list5);
            double EMAEFR_2 = transV::list_avg(transV::Params::cur_EFR_list2);

            // calculate EMALRR_5 & EMALRR_2
            double EMALRR_5 = transV::list_avg(transV::Params::cur_LRR_list5);
            double EMALRR_2 = transV::list_avg(transV::Params::cur_LRR_list2);

            // calculate EMAEFR_5_2 & EMALRR_5_2 for I-Module
            I_states.EMAEFR_5_2 = EMAEFR_5 - EMAEFR_2;
            I_states.EMALRR_5_2 = EMALRR_5 - EMALRR_2;

            // set EMAEFR_5 & EMALRR_5 for I-Module & M-Module
            I_rewards.EMAEFR_5 = EMAEFR_5;
            I_rewards.EMALRR_5 = EMALRR_5;
            M_rewards.EMALRR_5 = EMALRR_5;

            // set EFR & LRR for I-Module & M-Module
            I_rewards.EFR = item["fractionEffectiveFECPkt"].asDouble() / 255.0L;
            I_rewards.LRR = item["fractionLostRecover"].asDouble() / 255.0L;
            M_rewards.LRR = item["fractionLostRecover"].asDouble() / 255.0L;

            // calculate EFR_LRR_diff for I-Module
            I_states.EFR_LRR_diff = (I_rewards.EFR - I_rewards.LRR) / (I_rewards.EFR + I_rewards.LRR + 0.0001);

            // cur_burst_list3 for M Module
            while (transV::Params::cur_burst_list3.size() >= 3) {
              transV::Params::cur_burst_list3.pop_front();
            }
            transV::Params::cur_burst_list3.push_back(item["burstLostNum"].asDouble());

            // cur_burst_list10 for M Moudle
            while (transV::Params::cur_burst_list10.size() >= 10) {
              transV::Params::cur_burst_list10.pop_front();
            }
            transV::Params::cur_burst_list10.push_back(item["burstLostNum"].asDouble());
            
            // calculate EMAburst_3 and EMAburst_10
            double EMAburst_3 = transV::list_avg(transV::Params::cur_burst_list3);
            double EMAburst_10 = transV::list_avg(transV::Params::cur_burst_list10);

            // set EMAburst_3 for M module
            M_states.EMAburst_3 = EMAburst_3;
            M_rewards.EMAburst_3 = EMAburst_3;
            M_states.EMAburst_3_10 = EMAburst_3 / (EMAburst_10 + 0.0001);
         }
     }

      if (transV::Params::do_RL_times>=5 && transV::checkLState(L_params)) {
          transV::Params::L = getL(L_params);
      }
      else {
          transV::Params::L = 32;
      }
     
      double cur_IReward = 0;
      double cur_baseline_Ireward = 0;
      if (inputV::Params::type == inputV::ExpType::RLSRSFEC) {
        if (transV::Params::do_RL_times>=5 && transV::checkIReward(I_rewards) && transV::checkIState(I_states)) {
            std::vector<double> cur_state = transV::IState_to_vector(I_states);
            
            // double cur_reward = 0;
            // if (transV::Params::I+transV::Params::delta_I<=0 || transV::Params::I+transV::Params::delta_I>transV::Params::L) {
            //   cur_reward = -1;
            // }
            // else {
            //   cur_reward = transV::calculate_Ireward(I_rewards,1.875,1,0.5,0.8,0.05,0.05,0.1);
            // }

            double cur_reward = transV::calculate_Ireward(I_rewards,1,1,0.8,0.7,0.1,0.1,0.1);
            cur_IReward = cur_reward;
            cur_baseline_Ireward = cur_reward - transV::Params::baseline_Ireward;
            auto rl_model = IModuleRLWrapper::get_instance();
            transV::Params::delta_I = static_cast<int> (rl_model.predict(cur_state, cur_reward - transV::Params::baseline_Ireward));
            transV::Params::I = transV::Params::I + transV::Params::delta_I;
            if (transV::Params::baseline_Ireward == 0) {
              transV::Params::baseline_Ireward = cur_reward;
            }
            else {
              transV::Params::baseline_Ireward = transV::Params::baseline_Ireward * 0.9 + cur_reward * 0.1;
            }
            if (inputV::Params::ifTrainI && transV::Params::I<=transV::Params::L && transV::Params::I>=1) {
              rl_model.train_step();
            }
            if (transV::Params::I > transV::Params::L) transV::Params::I = transV::Params::L;
            if (transV::Params::I < 1) transV::Params::I = 1;
        }
        else {
            transV::Params::I = static_cast<uint32_t> (default_I);
        }

        // transV::Params::I = static_cast<uint32_t> (default_I);
      }

      if (I_states.I_inv_EMALR_10 <0.75 || I_states.I_inv_EMALR_10>1.5) {
        transV::Params::I = static_cast<uint32_t> (default_I);
      }

      // transV::Params::I = 20;
      // transV::Params::I = static_cast<uint32_t> (default_I);

      // std::cout<<"I value:"<<transV::Params::I<<" "<<default_I<<std::endl;

      // transV::Params::I = static_cast<uint32_t> (default_I);

      // For debug
      // if (transV::Params::do_RL_times < 30) {
      //   transV::Params::I = static_cast<uint32_t> (std::floor(1.0L / inputV::Params::FecRate *10));
      //   transV::Params::do_RL_times++;
      // }
      // else {
      //   transV::Params::I = static_cast<uint32_t> (std::floor(1.0L / inputV::Params::FecRate));
      // }
      
      double cur_MReward = 0;
      double cur_baseline_Mreward = 0;

      if (inputV::Params::type == inputV::ExpType::RLSRSFEC) {
        if (transV::Params::do_RL_times>=5 && transV::checkMReward(M_rewards) && transV::checkMState(M_states)) {
            std::vector<double> cur_state = transV::MState_to_vector(M_states);
            double cur_reward = transV::calculate_Mreward(M_rewards, 0.5 ,0.3, 0.1, 0.1);
            cur_MReward = cur_reward;
            cur_baseline_Mreward = cur_reward - transV::Params::baseline_Mreward;
            std::cout<<"Get MModule Instance?"<<std::endl;
            auto rl_model = MModuleRLWrapper::get_instance();
            transV::Params::M = static_cast<uint32_t> (rl_model.predict(cur_state, cur_reward - transV::Params::baseline_Mreward));
            if (transV::Params::baseline_Mreward == 0) {
              transV::Params::baseline_Mreward = cur_reward;
            }
            else {
              transV::Params::baseline_Mreward = transV::Params::baseline_Mreward * 0.9 + cur_reward * 0.1;
            }
            
            transV::Params::baseline_Mreward = transV::Params::baseline_Mreward * 0.9 + cur_reward * 0.1;
            if (inputV::Params::ifTrainM) {
              rl_model.train_step();
            }
        }
        else {
            transV::Params::M = 1;
        }

        // transV::Params::M = 1;
      }

      if (M_states.EMAburst_3 < 0.5) {
        transV::Params::M = 1;
      }

      // if (transV::Params::M > 1) {
      //   transV::Params::I = transV::Params::I * 2;
      //   if (transV::Params::I > transV::Params::L) {
      //     transV::Params::I = transV::Params::L;
      //   }
      // }

      std::cout<<"M value:"<<transV::Params::M<<std::endl;

      // To record the Params Parameters
      while (transV::Params::cur_M_list5.size()>=5) {
        transV::Params::cur_M_list5.pop_front();
      }
      transV::Params::cur_M_list5.push_back(static_cast<double>(transV::Params::M));
      transV::Params::delta_M = std::abs(transV::Params::last_M - static_cast<int>(transV::Params::M));
      transV::Params::last_M = transV::Params::M; 

      transV::Params::do_RL_times += 1;

     //Write file!
     // write LModule.csv
     if (transV::checkLState(L_params)) {
        std::ofstream LModuleFile(inputV::Params::output+"LModule.csv",std::ios::app);
        LModuleFile << rec_timestamp << ","
                    << L_params.target_bitrate << ","
                    << L_params.RTT << ","
                    << L_params.s_pck << ","
                    << L_params.fps << ","
                    << transV::Params::L << "\n";
     }
    
     // write IModule.csv
     if (transV::checkIReward(I_rewards) && transV::checkIState(I_states)){
        std::ofstream IModuleFile(inputV::Params::output+"IModule.csv",std::ios::app);
        IModuleFile << rec_timestamp << ","
                    << I_states.EMALR_10_3 << ","
                    << I_states.EMAEFR_5_2 << ","
                    << I_states.EMALRR_5_2 << ","
                    << I_states.I_inv_EMALR_10 << ","
                    << I_states.EFR_LRR_diff << ","
                    << I_states.M_5 << ","
                    << I_rewards.EFR << ","
                    << I_rewards.LRR << ","
                    << I_rewards.EMAEFR_5 << ","
                    << I_rewards.EMALRR_5 << ","
                    << I_rewards.delta_I << ","
                    << I_rewards.M << ","
                    << I_rewards.EMALR_10_3 << ","
                    << cur_IReward << ","
                    << cur_baseline_Ireward << ","
                    << transV::Params::I << "\n";
     }

     // write MModule.csv
     if (transV::checkMReward(M_rewards) && transV::checkMState(M_states)) {
        std::ofstream MModuleFile(inputV::Params::output+"MModule.csv",std::ios::app);
        MModuleFile << rec_timestamp << ","
                    << M_states.EMAburst_3 << ","
                    << M_states.EMAburst_3_10 << ","
                    << M_states.EMALR_3_5_10 << ","
                    << M_states.StdDevRTT_5 << ","
                    << M_states.RTT_EMARTT_30 << ","
                    << M_states.VarJitter_5 << ","
                    << M_states.I_inv_EMALR_10 << ","
                    << M_states.EMAM_5 << ","
                    << M_rewards.LRR << ","
                    << M_rewards.EMALRR_5 << ","
                    << M_rewards.M << ","
                    << M_rewards.EMAburst_3 << ","
                    << M_rewards.delta_M << ","
                    << cur_MReward << ","
                    << cur_baseline_Mreward << ","
                    << transV::Params::M << "\n";
     }
   }
};

std::unique_ptr<MP4VideoPlayer> CreatePlayer(webrtc::TaskQueueFactory& task_queue_factory, const char* file_path){
  const size_t kWidth = 1920;
  const size_t kHeight = 1080;
  const size_t kFps = 30;

  std::unique_ptr<MP4VideoPlayer> player = webrtc::test::CreateMP4VideoPlayer(kWidth, kHeight, kFps, file_path);
  if (player){
    std::cout<<"Create Player Success!!!"<<std::endl;
    return player;
  }
  else {
    std::cout<<"Create Player Failed... Use Default Video Stream!"<<std::endl;
  }

  auto frame_generator = webrtc::test::CreateSquareFrameGenerator(kWidth, kHeight, std::nullopt, std::nullopt);
  return std::make_unique<webrtc::test::FrameGeneratorPlayer>(
      webrtc::Clock::GetRealTimeClock(), std::move(frame_generator), kFps, task_queue_factory);
}

class MP4TrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<MP4TrackSource> Create(webrtc::TaskQueueFactory& task_queue_factory, const char* file_path) {
    std::unique_ptr<MP4VideoPlayer> player = CreatePlayer(task_queue_factory, file_path);
    if (player){
        player->Start();
        return rtc::make_ref_counted<MP4TrackSource>(std::move(player));
    }
    return nullptr;
  }

 protected:
  explicit MP4TrackSource(std::unique_ptr<MP4VideoPlayer> player) 
      : VideoTrackSource(/*remote=*/false), player_(std::move(player)) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return player_.get();
  }

  std::unique_ptr<MP4VideoPlayer> player_;
};

} // namespace

Conductor::Conductor(PeerConnectionClient* client, MainWindow* main_wnd) : client_(client), main_wnd_(main_wnd) {
  client_->RegisterObserver(this);
  main_wnd->RegisterObserver(this);
  state_ = NOT_CONNECTED;
}

Conductor::~Conductor() {
  RTC_DCHECK(!peer_connection_);
}

bool Conductor::connection_active() const {
  return peer_connection_ != nullptr;
}

void Conductor::Close() {
  client_->SignOut();
  DeletePeerConnection();
}

bool Conductor::InitializePeerConnection() {
  RTC_DCHECK(!peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);

  if (!signaling_thread_.get()) {
    signaling_thread_ = rtc::Thread::CreateWithSocketServer();
    signaling_thread_->Start();
  }

  webrtc::PeerConnectionFactoryDependencies deps;
  deps.signaling_thread = signaling_thread_.get();
  deps.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory(),
  deps.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  deps.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  deps.video_encoder_factory = std::make_unique<webrtc::VideoEncoderFactoryTemplate<
          webrtc::LibvpxVp8EncoderTemplateAdapter,
          webrtc::LibvpxVp9EncoderTemplateAdapter,
          webrtc::OpenH264EncoderTemplateAdapter,
          webrtc::LibaomAv1EncoderTemplateAdapter>>();
  deps.video_decoder_factory = std::make_unique<webrtc::VideoDecoderFactoryTemplate<
          webrtc::LibvpxVp8DecoderTemplateAdapter,
          webrtc::LibvpxVp9DecoderTemplateAdapter,
          webrtc::OpenH264DecoderTemplateAdapter,
          webrtc::Dav1dDecoderTemplateAdapter>>();
  webrtc::EnableMedia(deps);
  task_queue_factory_ = deps.task_queue_factory.get();
  peer_connection_factory_ = webrtc::CreateModularPeerConnectionFactory(std::move(deps));
  state_time_initialize = false;
  rl_state_time_initialize = false;

  if (!peer_connection_factory_) {
    DeletePeerConnection();
    return false;
  }

  if (!CreatePeerConnection()) {
    DeletePeerConnection();
  }

  AddTracks();

  return peer_connection_ != nullptr;
}

bool Conductor::CreatePeerConnection() {
  RTC_DCHECK(peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = GetPeerConnectionString();
  config.servers.push_back(server);

  webrtc::PeerConnectionDependencies pc_dependencies(this);
  auto error_or_peer_connection = peer_connection_factory_->CreatePeerConnectionOrError(
          config, std::move(pc_dependencies));
  if (error_or_peer_connection.ok()) {
    peer_connection_ = std::move(error_or_peer_connection.value());
  }
  return peer_connection_ != nullptr;
}

void Conductor::DeletePeerConnection() {
  stopGetState();
  main_wnd_->StopLocalRenderer();
  main_wnd_->StopRemoteRenderer();
  peer_connection_ = nullptr;
  peer_connection_factory_ = nullptr;
  state_ = NOT_CONNECTED;
}

//
// Statistical network information
//

int32_t Conductor::startGetState(){
  if (state_!=CONNECTTOPEER){
    if (!_getStateThread.empty()){
      stopGetState();
      return -1;
    }
    else {
      return -1;
    }
  }

  if (_getStateThread.empty()){
    _getStateThread = rtc::PlatformThread::SpawnJoinable(
      [this] {
        while (doGetState()){
        }
      },
      "getStateThread",
      rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh)
    );
    return 0;
  }
  else {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " : getStateThread has been started! Just Return!\n";
    return 0;
  }
}

int32_t Conductor::stopGetState(){
  if (!_getStateThread.empty()){
    _getStateThread.Finalize();
  }

  state_time_initialize = false;

  return 0;
}

int32_t Conductor::startRLState() {

  if (state_!=CONNECTTOPEER){
    if (!_getRLStateThread.empty()){
      stopRLState();
      return -1;
    }
    else {
      return -1;
    }
  }

  if (_getRLStateThread.empty()){
    _getRLStateThread = rtc::PlatformThread::SpawnJoinable(
      [this] {
        while (doFECRLState()){
        }
      },
      "getRLStateThread",
      rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh)
    );
    return 0;
  }
  else {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " : getRLStateThread has been started! Just Return!\n";
    return 0;
  }
}

int32_t Conductor::stopRLState(){
  if (!_getRLStateThread.empty()){
    _getRLStateThread.Finalize();
  }

  rl_state_time_initialize = false;

  return 0;
}

bool Conductor::doGetState(){
  if (state_!=CONNECTTOPEER){
    return false;
  }
  auto now = std::chrono::steady_clock::now();
  if (!state_time_initialize){
    start_state_time = now;
    last_state_time = now - std::chrono::milliseconds(int64_t(inputV::Params::interval_ms));
    state_time_initialize = true;
  }

  double elapsed_time = std::chrono::duration<double>(now-start_state_time).count();
  double state_time = std::chrono::duration<double>(last_state_time-start_state_time).count()+double(inputV::Params::interval_ms)/1000;
  if (elapsed_time < state_time){
    std::this_thread::sleep_for(std::chrono::duration<double>(state_time - elapsed_time));
  }
  now = std::chrono::steady_clock::now();
  elapsed_time = std::chrono::duration<double>(now-start_state_time).count();
  last_state_time = now;


  rtc::scoped_refptr<RTCStatsObserver> rtc_stats_observer_;
  rtc_stats_observer_ = new rtc::RefCountedObject<RTCStatsObserver>();
  peer_connection_->GetStats(rtc_stats_observer_.get());

  return true;
}

bool Conductor::doFECRLState(){

  // std::cout<<"Run doFECRLState"<<std::endl;

  if (state_!=CONNECTTOPEER){
    return false;
  }
  auto now = std::chrono::steady_clock::now();
  if (!rl_state_time_initialize){
    start_rl_time = now;
    last_rl_time = now - std::chrono::milliseconds(int64_t(inputV::Params::RL_interval_ms));
    rl_state_time_initialize = true;
  }

  double elapsed_time = std::chrono::duration<double>(now-start_rl_time).count();
  double state_time = std::chrono::duration<double>(last_rl_time-start_rl_time).count()+double(inputV::Params::RL_interval_ms)/1000;
  if (elapsed_time < state_time){
    std::this_thread::sleep_for(std::chrono::duration<double>(state_time - elapsed_time));
  }
  now = std::chrono::steady_clock::now();
  elapsed_time = std::chrono::duration<double>(now-start_rl_time).count();
  last_rl_time = now;


  rtc::scoped_refptr<FECStatsObserver> rl_stats_observer_;
  rl_stats_observer_ = new rtc::RefCountedObject<FECStatsObserver>();
  peer_connection_->GetStats(rl_stats_observer_.get());

  return true;
}


//
// PeerConnectionObserver implementation.
//

void Conductor::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << receiver->id();
  main_wnd_->QueueUIThreadCallback(NEW_TRACK_ADDED, receiver->track().release());
}

void Conductor::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << receiver->id();
  main_wnd_->QueueUIThreadCallback(TRACK_REMOVED, receiver->track().release());
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
  Json::Value jmessage;
  jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
  jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
    return;
  }
  jmessage[kCandidateSdpName] = sdp;
  Json::StreamWriterBuilder factory;
  SendMessage(Json::writeString(factory, jmessage));
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::OnSignedIn() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void Conductor::OnDisconnected() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  DeletePeerConnection();
}

void Conductor::OnOtherJoined() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  ConnectToPeer();
}

void Conductor::OnPeerConnected() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void Conductor::OnPeerDisconnected() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  if (state_ == CONNECTTOPEER) {
    RTC_LOG(LS_INFO) << "Our peer disconnected";
    DeletePeerConnection();
  }
}

void Conductor::OnMessageFromPeer(const std::string& message) {
  RTC_DCHECK(!message.empty());
  if (!peer_connection_.get()) {
    RTC_DCHECK(state_ == NOT_CONNECTED);
    state_ = CONNECTTOPEER;
    if (!InitializePeerConnection()) {
      RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
      client_->SignOut();
      return;
    } else {
      startGetState();
      if (inputV::Params::type == inputV::ExpType::RLSRSFEC || inputV::Params::type == inputV::ExpType::RSFECStreamStableRate) {
        startRLState();
      }
    }
  }

  Json::CharReaderBuilder factory;
  std::unique_ptr<Json::CharReader> reader = absl::WrapUnique(factory.newCharReader());
  Json::Value jmessage;
  if (!reader->parse(message.data(), message.data() + message.length(), &jmessage, nullptr)) {
    RTC_LOG(LS_WARNING) << "Received unknown message. " << message;
    return;
  }
  std::string type_str;
  std::string json_object;

  rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type_str);
  if (!type_str.empty()) {
    std::optional<webrtc::SdpType> type_maybe = webrtc::SdpTypeFromString(type_str);
    if (!type_maybe) {
      RTC_LOG(LS_ERROR) << "Unknown SDP type: " << type_str;
      return;
    }
    webrtc::SdpType type = *type_maybe;
    std::string sdp;
    if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) {
      RTC_LOG(LS_WARNING) << "Can't parse received session description message.";
      return;
    }
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
    if (!session_description) {
      RTC_LOG(LS_WARNING) << "Can't parse received session description message. "
             "SdpParseError was: " << error.description;
      return;
    }
    RTC_LOG(LS_INFO) << " Received session description :" << message;
    peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create().get(),
        session_description.release());
    if (type == webrtc::SdpType::kOffer) {
      peer_connection_->CreateAnswer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }
  } else {
    std::string sdp_mid;
    int sdp_mlineindex = 0;
    std::string sdp;
    if (!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) ||
        !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) ||
        !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) {
      RTC_LOG(LS_WARNING) << "Can't parse received message.";
      return;
    }
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
    if (!candidate.get()) {
      RTC_LOG(LS_WARNING) << "Can't parse received candidate message. "
                             "SdpParseError was: " << error.description;
      return;
    }
    if (!peer_connection_->AddIceCandidate(candidate.get())) {
      RTC_LOG(LS_WARNING) << "Failed to apply the received candidate";
      return;
    }
    RTC_LOG(LS_INFO) << " Received candidate :" << message;
  }
}

void Conductor::OnMessageSent(int err) {
  // Process the next pending message if any.
  main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
}

void Conductor::OnServerConnectionFailure() {

}

//
// API called by main.cc
//

void Conductor::StartLogin(const std::string& server, int port) {
  if (client_->is_connected()) return;
  server_ = server;
  client_->Connect(server, port);
}

void Conductor::DisconnectFromServer() {
  if (client_->is_connected()) client_->SignOut();
}

void Conductor::ConnectToPeer() {
  if (peer_connection_.get()) {
    return;
  }

  if (InitializePeerConnection()) {
    state_ = CONNECTTOPEER;
    startGetState();
    if (inputV::Params::type == inputV::ExpType::RLSRSFEC || inputV::Params::type == inputV::ExpType::RSFECStreamStableRate) {
      startRLState();
    }
    peer_connection_->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }
}

void Conductor::AddTracks() {
  if (!peer_connection_->GetSenders().empty()) {
    return;  // Already added tracks.
  }

  // rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(peer_connection_factory_->CreateAudioTrack(kAudioLabel,
  //         peer_connection_factory_->CreateAudioSource(cricket::AudioOptions()).get()));
  // auto result_or_error = peer_connection_->AddTrack(audio_track, {kStreamId});
  // if (!result_or_error.ok()) {
  //   RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: " << result_or_error.error().message();
  // }

  rtc::scoped_refptr<MP4TrackSource> video_track_source = MP4TrackSource::Create(*task_queue_factory_, file_path_.c_str());

  if (video_track_source) {
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(peer_connection_factory_->CreateVideoTrack(video_track_source, kVideoLabel));
    main_wnd_->StartLocalRenderer(video_track_.get());

    auto result_or_error = peer_connection_->AddTrack(video_track_, {kStreamId});
    if (!result_or_error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: " << result_or_error.error().message();
    }
  } else {
    RTC_LOG(LS_ERROR) << "OpenVideoCaptureDevice failed";
  }

  main_wnd_->InitializeUI();
}

void Conductor::DisconnectFromCurrentPeer() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  if (peer_connection_.get()) {
    client_->SendHangUp();
    DeletePeerConnection();
  }
}

void Conductor::UIThreadCallback(int msg_id, void* data) {
  switch (msg_id) {
    case PEER_CONNECTION_CLOSED:
      RTC_LOG(LS_INFO) << "PEER_CONNECTION_CLOSED";
      DeletePeerConnection();
      break;

    case SEND_MESSAGE_TO_PEER: {
      RTC_LOG(LS_INFO) << "SEND_MESSAGE_TO_PEER";
      std::string* msg = reinterpret_cast<std::string*>(data);
      if (msg) {
        pending_messages_.push_back(msg);
      }

      if (!pending_messages_.empty()) {
        msg = pending_messages_.front();
        pending_messages_.pop_front();

        if (!client_->PCSendMessage(*msg)) {
          RTC_LOG(LS_ERROR) << "PCSendMessage failed";
          DisconnectFromServer();
        }
        delete msg;
      }

      if (!peer_connection_.get()) state_ = NOT_CONNECTED;

      break;
    }

    case NEW_TRACK_ADDED: {
      auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
      if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
        main_wnd_->StartRemoteRenderer(video_track);
      }
      track->Release();
      break;
    }

    case TRACK_REMOVED: {
      // Remote peer stopped sending a track.
      auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
      track->Release();
      break;
    }

    default:
      RTC_DCHECK_NOTREACHED();
      break;
  }
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create().get(), desc);

  std::string sdp;
  desc->ToString(&sdp);

  Json::Value jmessage;
  jmessage[kSessionDescriptionTypeName] = webrtc::SdpTypeToString(desc->GetType());
  jmessage[kSessionDescriptionSdpName] = sdp;

  Json::StreamWriterBuilder factory;
  SendMessage(Json::writeString(factory, jmessage));
}

void Conductor::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_ERROR) << ToString(error.type()) << ": " << error.message();
}

void Conductor::SendMessage(const std::string& json_object) {
  std::string* msg = new std::string(json_object);
  main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, msg);
}

void Conductor::SetFilePath(std::string file_path){
    file_path_ = file_path;
}
