
#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <fstream>
#include <pybind11/embed.h>
#include <signal.h>
#include <stdlib.h>

#include "absl/flags/parse.h"
#include "api/scoped_refptr.h"
#include "examples/customizationFEC/conductor.h"
#include "examples/customizationFEC/flag_defs.h"
#include "examples/customizationFEC/main_wnd.h"
#include "examples/customizationFEC/peer_connection_client.h"
#include "examples/customizationFEC/Params.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "examples/customizationFEC/IModule/IModulePyWrapper.h"

class CustomSocketServer : public rtc::PhysicalSocketServer {
 public:
  explicit CustomSocketServer(GtkMainWnd* wnd) : wnd_(wnd), conductor_(NULL), client_(NULL) {}
  virtual ~CustomSocketServer() {}

  void SetMessageQueue(rtc::Thread* queue) override { message_queue_ = queue; }

  void set_client(PeerConnectionClient* client) { client_ = client; }
  void set_conductor(Conductor* conductor) { conductor_ = conductor; }

  // Override so that we can also pump the GTK message loop.
  // This function never waits.
  bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override {
    while (gtk_events_pending()) gtk_main_iteration();

    if (!wnd_->IsWindow() && !conductor_->connection_active() && client_ != NULL && !client_->is_connected()) {
      message_queue_->Quit();
    }
    return rtc::PhysicalSocketServer::Wait(webrtc::TimeDelta::Zero(), process_io);
  }

 protected:
  rtc::Thread* message_queue_;
  GtkMainWnd* wnd_;
  Conductor* conductor_;
  PeerConnectionClient* client_;
};

void output_parameters(){
  std::cout<<"Current Parameters Setup:"<<std::endl;
  std::cout<<"\tplayVideo="<<inputV::Params::playVideo<<std::endl;
  std::cout<<"\tduration="<<inputV::Params::duration<<std::endl;
  std::cout<<"\tinterval(ms)="<<inputV::Params::interval_ms<<std::endl;
  std::cout<<"\toutput="<<inputV::Params::output<<std::endl;
  std::cout<<"\tFecRate="<<inputV::Params::FecRate<<std::endl;
  if (inputV::Params::type==inputV::ExpType::WebRTCSource){
    std::cout<<"\ttype=WebRTCSource"<<std::endl;
  }
  else if (inputV::Params::type==inputV::ExpType::WebRTCFlexFEC){
    std::cout<<"\ttype=WebRTCFlexFEC"<<std::endl;
  }
  else if (inputV::Params::type==inputV::ExpType::StableRate){
    std::cout<<"\ttype=StableRate"<<std::endl;
  }
  else if (inputV::Params::type==inputV::ExpType::RSFECBlock){
    std::cout<<"\ttype=RSFECBlock"<<std::endl;
  }
  else if (inputV::Params::type==inputV::ExpType::RSFECStreamStableRate){
    std::cout<<"\ttype=RSFECStreamStableRate"<<std::endl;
  }
  else if (inputV::Params::type==inputV::ExpType::RSFECStreamSourceRate){
    std::cout<<"\ttype=RSFECStreamSourceRate"<<std::endl;
  }
  else if (inputV::Params::type==inputV::ExpType::FECClose){
    std::cout<<"\ttype=FECClose"<<std::endl;
  }
}

int init_output_file(){
  std::string path=inputV::Params::output+"inbound.csv";
  std::ofstream file1(path);
  if (file1.is_open()){
    file1 <<"timestamp,jitter,packetLost,packetsReceived,bytesReceived,retransmittedPacketsReceived,retransmittedBytesReceived"
      <<",jitterBufferDelay,jitterBufferEmittedCount,jitterBufferTargetDelay,jitterBufferMinimumDelay,framesReceived,frameWidth,frameHeight,framesPerSecond,framesDecoded,keyFramesDecoded"
        <<",framesDropped,totalDecodeTime,totalInterFrameDelay,totalSquaredInterFrameDelay,freezeCount,totalFreezesDuration,pauseCount,totalPausesDuration,totalAssemblyTime,framesAssembledFromMultiplePackets,totalProcessingDelay,firCount,pliCount,nackCount,qpSum,minPlayoutDelay\n";
    file1.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"outbound.csv";
  std::ofstream file2(path);
  if (file2.is_open()){
    file2<<"timestamp,packetsSent,bytesSent,retransmittedPacketsSent,retransmittedBytesSent,targetBitrate"
      <<",framesEncoded,keyFramesEncoded,totalEncodeTime,totalEncodedBytesTarget,frameWidth,frameHeight,framesPerSecond,framesSent,totalPacketSendDelay,nackCount,qualityLimitationReason,qualityLimitationResolutionChanges"
        <<",firCount,pliCount\n";
    file2.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"candidate_pair.csv";
  std::ofstream file2_5(path);
  if (file2_5.is_open()){
    file2_5<<"timestamp,totalRoundTripTime,currentRoundTripTime,availableOutgoingBitrate"
      <<",packetsSent,packetsReceived,bytesSent,bytesReceived\n";
    file2_5.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"remote_inbound.csv";
  std::ofstream file2_6(path);
  if (file2_6.is_open()){
    file2_6<<"timestamp,jitter,packetsLost,fractionLost,roundTripTime,totalRoundTripTime,roundTripTimeMeasurements\n";
    file2_6.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"remote_outbound.csv";
  std::ofstream file2_7(path);
  if (file2_7.is_open()){
    file2_7<<"timestamp,packetsSent,bytesSent,reportsSent,totalRoundTripTime,roundTripTimeMeasurements\n";
    file2_7.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"rtp_video_sender.csv";
  std::ofstream file3(path);
  if (file3.is_open()){
    file3<<"timestamp,sent_video_rate_bps,sent_nack_rate_bps,sent_fec_rate_bps\n";
    file3.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"forward_error_correction.csv";
  std::ofstream file4(path);
  if (file4.is_open()){
    file4<<"timestamp,FEC_seq_num,status,recover_seq_num,protect_seq_nums,decode_time,search_time,fec_store_time\n";
    file4.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"rtp_sender_egress.csv";
  std::ofstream file5(path);
  if (file5.is_open()){
    file5<<"timestamp,seq_num,payload_size,is_key_frame,packet_type\n";
    file5.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }
  
  path = inputV::Params::output+"rtp_video_stream_receiver2.csv";
  std::ofstream file6(path);
  if (file6.is_open()){
    file6<<"timestamp,seq_num,payload_size,packet_type\n";
    file6.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"remote_fec.csv";
  std::ofstream file7(path);
  if (file7.is_open()) {
    file7<<"timestamp,fractionLostRecover,fixedFractionLostRecover,fractionEffectiveFECPkt,fixedFractionEffectiveFECPkt,fixedAverageRecoverTime,fixedAverageRetransmitTime,fixedTimeAheadForRecover,AverageRecoverTime,AverageRetransmitTime,TimeAheadForRecover,recoverPktNumRaw,cumulativeRecoverPkt,retransmitPktNumRaw,cumulativeRetransmitPkt,bothSuccPktNumRaw,cumulativeBothSuccPkt,burstLostNum,cumulativeBurstLostNum,EFR,LRR\n";
    file7.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"LModule.csv";
  std::ofstream file8(path);
  if (file8.is_open()) {
    file8<<"timestamp,target_bitrate,RTT,s_pck,fps,L\n";
    file8.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"IModule.csv";
  std::ofstream file9(path);
  if (file9.is_open()) {
    file9<<"timestamp,EMALR_10_3,EMAEFR_5_2,EMALRR_5_2,I_inv_EMALR_10,EFR_LRR_diff,M_5,EFR,LRR,EMAEFR_5,EMALRR_5,delta_I,M,EMALR_10_3,Reward,cur_baseline_Ireward,I\n";
    file9.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"MModule.csv";
  std::ofstream file10(path);
  if (file10.is_open()) {
    file10<<"timestamp,EMAburst_3,EMAburst_3_10,EMALR_3_5_10,StdDevRTT_5,RTT_EMARTT_30,VarJitter_5,I_inv_EMALR_10,EMAM_5,LRR,EMALRR_5,last_M,EMAburst_3,delta_M,Reward,cur_baseline_Mreward,M\n";
    file10.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }


  // path = inputV::Params::output+"frame_receiver.csv";
  // std::ofstream file7(path);
  // if (file7.is_open()){
  //   file7<<"timestamp,frame_latency\n";
  //   file7.close();
  // }
  // else {
  //   std::cerr << "Error opening file "<<path<<std::endl;
  //   return -1;
  // }

  return 0;
}

int init_output_dict(){
  size_t pos = 0;
  bool success = true;
  std::string path = inputV::Params::output;

  while (success && (pos = path.find_first_of('/', pos)) != std::string::npos){
    std::string subdir = path.substr(0,pos);
    if (subdir.empty()||subdir=="."){
      pos++;
      continue;
    }

    if (mkdir(subdir.c_str(), 0777)!=0 && errno !=EEXIST) {
      std::cerr << "Error creating directory: "<<subdir<<"(" <<strerror(errno)<<")"<<std::endl;
      success = false;
    }
    pos++;
  }

  if (success && mkdir(path.c_str(), 0777) != 0 && errno != EEXIST) {
    std::cerr << "Error creating directory: " << path << " (" << strerror(errno) << ")" << std::endl;
    success = false;
  }
  
  if (success) return 0;
  else return -1;
}

void testPyFunction(){
  pybind11::scoped_interpreter guard{};

  pybind11::module sys = pybind11::module::import("sys");
  sys.attr("path").attr("insert")(0, "/home/ubuntu/Desktop/WebRTC/src/examples/customizationFEC/");
  
  pybind11::module my_module = pybind11::module::import("RLCode.PyBindExample");
  pybind11::function add_func = my_module.attr("add");
  int result = add_func(3, 4).cast<int>();
  std::cout << "Result from Python function: " << result << std::endl;
}

void initPyFunction() {
  auto I_rl_model = IModuleRLWrapper::get_instance();
}

void signal_handler(int signal) {
  std::cout << "\nInterrupt signal (" << signal << ") received. Exiting...\n";
  exit(signal);
}

int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);
  // g_type_init API is deprecated (and does nothing) since glib 2.35.0, see:
  // https://mail.gnome.org/archives/commits-list/2012-November/msg07809.html
  #if !GLIB_CHECK_VERSION(2, 35, 0)
    g_type_init();
  #endif
  // g_thread_init API is deprecated since glib 2.31.0, see release note:
  // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
  #if !GLIB_CHECK_VERSION(2, 31, 0)
    g_thread_init(NULL);
  #endif

  signal(SIGINT, signal_handler);

  // testPyFunction();
  // initPyFunction();
  // checkPy();

  // absl::ParseCommandLine(argc, argv);

  if (inputV::Params::init(argc, argv)==-1){
    return -1;
  }
  else {
    output_parameters();
  }

  if (inputV::Params::output==""||inputV::Params::playVideo==""){
    std::cerr<<"Output dictionary and playVideo file must be set!"<<std::endl;
    return -1;
  }

  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  const std::string forced_field_trials = absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(forced_field_trials.c_str());

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((absl::GetFlag(FLAGS_port) < 1) || (absl::GetFlag(FLAGS_port) > 65535)) {
    printf("Error: %i is not a valid port.\n", absl::GetFlag(FLAGS_port));
    return -1;
  }

  // Init the output directory!
  if (init_output_dict() == 0){
    std::cout<<"All result will be saved in "<<inputV::Params::output<<std::endl;
  }
  else {
    std::cerr<<"Error creating directory!"<<std::endl;
    return -1;
  }

  // Init the output files!
  if (init_output_file() != 0){
    std::cerr<<"Error creating files!"<<std::endl;
    return -1;
  }

  const std::string server = absl::GetFlag(FLAGS_server);
  const std::string file_path = inputV::Params::playVideo;
  const int port = absl::GetFlag(FLAGS_port);
  GtkMainWnd wnd(server.c_str(), absl::GetFlag(FLAGS_port));
  wnd.Create();

  CustomSocketServer socket_server(&wnd);
  rtc::AutoSocketServerThread thread(&socket_server);

  rtc::InitializeSSL();
  // Must be constructed after we set the socketserver.
  PeerConnectionClient client;
  auto conductor = rtc::make_ref_counted<Conductor>(&client, &wnd);
  socket_server.set_client(&client);
  socket_server.set_conductor(conductor.get());
  conductor->SetFilePath(file_path);
  conductor->StartLogin(server, port);

  thread.Run();

  // gtk_main();
  wnd.Destroy();

  rtc::CleanupSSL();
  return 0;
}