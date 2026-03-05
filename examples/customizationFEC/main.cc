
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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <thread>
#include <time.h>

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

namespace {

std::atomic<bool> g_interrupt_requested{false};

uint64_t SteadyNowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int64_t GetProcessCpuTimeNs() {
  timespec ts;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
    return -1;
  }
  return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + static_cast<int64_t>(ts.tv_nsec);
}

bool StartsWith(const std::string& s, const char* prefix) {
  return s.rfind(prefix, 0) == 0;
}

// Returns kB fields from /proc/self/status (VmRSS/VmHWM), or -1 if not found.
int64_t ReadProcStatusKb(const char* key) {
  std::ifstream f("/proc/self/status");
  if (!f.is_open()) {
    return -1;
  }
  std::string line;
  while (std::getline(f, line)) {
    if (!StartsWith(line, key)) {
      continue;
    }
    // Format: "VmRSS:\t  12345 kB"
    std::istringstream iss(line);
    std::string k;
    int64_t value_kb = -1;
    std::string unit;
    iss >> k >> value_kb >> unit;
    return value_kb;
  }
  return -1;
}

// Returns PSS in kB from /proc/self/smaps_rollup, or -1 if not available.
int64_t ReadProcSmapsRollupPssKb() {
  std::ifstream f("/proc/self/smaps_rollup");
  if (!f.is_open()) {
    return -1;
  }
  std::string line;
  while (std::getline(f, line)) {
    if (!StartsWith(line, "Pss:")) {
      continue;
    }
    std::istringstream iss(line);
    std::string k;
    int64_t value_kb = -1;
    std::string unit;
    iss >> k >> value_kb >> unit;
    return value_kb;
  }
  return -1;
}

// Reads and sums all available RAPL "energy_uj" counters (Intel/AMD). Returns microjoules, or -1.
int64_t ReadRaplEnergyUj() {
  namespace fs = std::filesystem;
  const fs::path powercap("/sys/class/powercap");
  if (!fs::exists(powercap) || !fs::is_directory(powercap)) {
    return -1;
  }

  int64_t total_uj = 0;
  bool found_any = false;

  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(powercap, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_directory()) {
      continue;
    }

    const std::string name = entry.path().filename().string();
    // Common: intel-rapl:0, intel-rapl:0:0, amd-rapl:0
    if (!(StartsWith(name, "intel-rapl") || StartsWith(name, "amd-rapl"))) {
      continue;
    }

    const fs::path energy_path = entry.path() / "energy_uj";
    std::ifstream f(energy_path);
    if (!f.is_open()) {
      continue;
    }

    int64_t uj = 0;
    f >> uj;
    if (f.fail()) {
      continue;
    }

    total_uj += uj;
    found_any = true;
  }

  return found_any ? total_uj : -1;
}

class ResourceMonitor {
 public:
  ResourceMonitor() = default;
  ResourceMonitor(const ResourceMonitor&) = delete;
  ResourceMonitor& operator=(const ResourceMonitor&) = delete;

  void Start(const std::string& output_dir, int sample_interval_ms) {
    if (running_.exchange(true)) {
      return;
    }

    output_dir_ = output_dir;
    sample_interval_ms_ = sample_interval_ms > 0 ? sample_interval_ms : 1000;

    start_steady_ms_ = SteadyNowMs();
    start_cpu_ns_ = GetProcessCpuTimeNs();
    start_energy_uj_ = ReadRaplEnergyUj();

    // Create time-series CSV.
    timeseries_path_ = output_dir_ + "resource_usage.csv";
    {
      std::ofstream out(timeseries_path_, std::ios::trunc);
      out << "t_ms,cpu_time_ms,vmrss_kb,vmhwm_kb,pss_kb,rapl_energy_uj\n";
    }

    worker_ = std::thread([this] { this->RunLoop(); });
  }

  void StopAndWriteSummary() {
    if (!running_.exchange(false)) {
      return;
    }
    if (worker_.joinable()) {
      worker_.join();
    }

    const int64_t end_cpu_ns = GetProcessCpuTimeNs();
    const int64_t end_energy_uj = ReadRaplEnergyUj();
    const int64_t vmrss_kb = ReadProcStatusKb("VmRSS:");
    const int64_t vmhwm_kb = ReadProcStatusKb("VmHWM:");
    const int64_t pss_kb = ReadProcSmapsRollupPssKb();

    const double cpu_time_ms = (start_cpu_ns_ >= 0 && end_cpu_ns >= 0)
                                   ? static_cast<double>(end_cpu_ns - start_cpu_ns_) / 1e6
                                   : -1.0;
    const int64_t energy_delta_uj = (start_energy_uj_ >= 0 && end_energy_uj >= 0)
                                       ? (end_energy_uj - start_energy_uj_)
                                       : -1;

    // Summary CSV (one row) for easy plotting.
    summary_path_ = output_dir_ + "resource_summary.csv";
    std::ofstream s(summary_path_, std::ios::trunc);
    s << "cpu_time_ms,vmrss_kb,vmhwm_kb,pss_kb,rapl_energy_uj\n";
    s << cpu_time_ms << "," << vmrss_kb << "," << vmhwm_kb << "," << pss_kb << "," << energy_delta_uj
      << "\n";
  }

 private:
  void RunLoop() {
    while (running_.load()) {
      const uint64_t t_ms = SteadyNowMs() - start_steady_ms_;
      const int64_t cpu_ns = GetProcessCpuTimeNs();
      const int64_t vmrss_kb = ReadProcStatusKb("VmRSS:");
      const int64_t vmhwm_kb = ReadProcStatusKb("VmHWM:");
      const int64_t pss_kb = ReadProcSmapsRollupPssKb();
      const int64_t energy_uj = ReadRaplEnergyUj();

      const double cpu_ms = cpu_ns >= 0 ? static_cast<double>(cpu_ns) / 1e6 : -1.0;

      std::ofstream out(timeseries_path_, std::ios::app);
      out << t_ms << "," << cpu_ms << "," << vmrss_kb << "," << vmhwm_kb << "," << pss_kb << "," << energy_uj
          << "\n";

      std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms_));
    }
  }

  std::atomic<bool> running_{false};
  std::thread worker_;

  std::string output_dir_;
  int sample_interval_ms_ = 1000;
  uint64_t start_steady_ms_ = 0;
  int64_t start_cpu_ns_ = -1;
  int64_t start_energy_uj_ = -1;

  std::string timeseries_path_;
  std::string summary_path_;
};

}  // namespace

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

    if (g_interrupt_requested.load()) {
      message_queue_->Quit();
    }

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
  else if (inputV::Params::type==inputV::ExpType::TamburFEC){
    std::cout<<"\ttype=TamburFEC"<<std::endl;
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

  path = inputV::Params::output+"frame_collector.csv";
  std::ofstream file12(path);
  if (file12.is_open()){
    file12<<"timestamp,frame_first_ts,frame_last_ts\n";
    file12.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  path = inputV::Params::output+"encode_duration.csv";
  std::ofstream file13(path);
  if (file13.is_open()){
    file13<<"timestamp,frame_num,encode_duration,encode_size\n";
    file13.close();
  }
  else {
    std::cerr << "Error opening file "<<path<<std::endl;
    return -1;
  }

  if (inputV::Params::type==inputV::ExpType::RLSRSFEC){
    path = inputV::Params::output+"LModule.csv";
    std::ofstream file8(path);
    if (file8.is_open()) {
      file8<<"timestamp,target_bitrate,RTT,s_pck,fps,L,func_time\n";
      file8.close();
    }
    else {
      std::cerr << "Error opening file "<<path<<std::endl;
      return -1;
    }

    path = inputV::Params::output+"IModule.csv";
    std::ofstream file9(path);
    if (file9.is_open()) {
      file9<<"timestamp,EMALR_10_3,EMAEFR_5_2,EMALRR_5_2,I_inv_EMALR_10,EFR_LRR_diff,M_5,EFR,LRR,EMAEFR_5,EMALRR_5,delta_I,M,EMALR_10_3,Reward,cur_baseline_Ireward,I,func_time\n";
      file9.close();
    }
    else {
      std::cerr << "Error opening file "<<path<<std::endl;
      return -1;
    }

    path = inputV::Params::output+"MModule.csv";
    std::ofstream file10(path);
    if (file10.is_open()) {
      file10<<"timestamp,EMAburst_3,EMAburst_3_10,EMALR_3_5_10,StdDevRTT_5,RTT_EMARTT_30,VarJitter_5,I_inv_EMALR_10,EMAM_5,LRR,EMALRR_5,last_M,EMAburst_3,delta_M,Reward,cur_baseline_Mreward,M,func_time\n";
      file10.close();
    }
    else {
      std::cerr << "Error opening file "<<path<<std::endl;
      return -1;
    }
  }
  
  if (inputV::Params::type==inputV::ExpType::TamburFEC){
    path = inputV::Params::output+"TamburEFRLRR.csv";
    std::ofstream file11(path);
    if (file11.is_open()) {
      file11<<"timestamp,EFR,LRR\n";
      file11.close();
    }
    else {
      std::cerr << "Error opening file "<<path<<std::endl;
      return -1;
    }

    path = inputV::Params::output+"TamburDecode.csv";
    std::ofstream file15(path);
    if (file15.is_open()) {
      file15<<"timestamp,frame_num,decode_duration\n";
      file15.close();
    }
    else {
      std::cerr << "Error opening file "<<path<<std::endl;
      return -1;
    }
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

  // Start resource monitoring after output directory is ready.
  ResourceMonitor resource_monitor;
  // 1000ms sampling is typically enough and keeps overhead negligible.
  resource_monitor.Start(inputV::Params::output, /*sample_interval_ms=*/1000);

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

  // Stop resource monitoring and write summary for plotting.
  resource_monitor.StopAndWriteSummary();

  // gtk_main();
  wnd.Destroy();

  rtc::CleanupSSL();
  return 0;
}