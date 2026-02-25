/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/customizationFEC/rtp_video_stream_receiver2.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <iostream>
#include <chrono>
#include <fstream>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/types/variant.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "common_video/corruption_detection_converters.h"
#include "common_video/corruption_detection_message.h"
#include "common_video/frame_instrumentation_data.h"
#include "media/base/media_constants.h"
#include "modules/pacing/packet_router.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/source/corruption_detection_extension.h"
#include "modules/rtp_rtcp/source/create_video_rtp_depacketizer.h"
#include "modules/rtp_rtcp/source/frame_object.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "examples/customizationFEC/UlpFEC/ulpfec_receiver.h"
#include "examples/customizationFEC/RS_FEC/RSfec_receiver.h"
#include "examples/customizationFEC/Tambur/TamburFecReceiver.h"
#include "examples/customizationFEC/Tambur/Tamburdecoder.hh"
#include "examples/customizationFEC/Tambur/src/fec/fec_multi_receiver.hh"
#include "examples/customizationFEC/Tambur/src/fec/streaming_code/streaming_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/streaming_code/multi_fec_header_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/quality_reporter.hh"
#include "examples/customizationFEC/Tambur/src/fec/quality_reporting/simple_quality_report_generator.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/multi_frame_fec_helpers.hh"
#include "examples/customizationFEC/Tambur/src/fec/multi_fec/block_code.hh"
#include "examples/customizationFEC/Tambur/src/fec/logger.hh"
#include "examples/customizationFEC/Tambur/src/fec/logging/timing_logger.hh"
#include "examples/customizationFEC/Tambur/src/fec/logging/frame_logger.hh"
#include "examples/customizationFEC/Tambur/src/fec/metric_logger.hh"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_raw.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp8.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "modules/video_coding/h264_sps_pps_tracker.h"
#include "modules/video_coding/nack_requester.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/ntp_time.h"
#include "examples/customizationFEC/Params.h"

namespace webrtc {

namespace {
// TODO(philipel): Change kPacketBufferStartSize back to 32 in M63 see:
//                 crbug.com/752886
constexpr int kPacketBufferStartSize = 512;
constexpr int kPacketBufferMaxSize = 2048;

constexpr int kMaxPacketAgeToNack = 450;

int PacketBufferMaxSize(const FieldTrialsView& field_trials) {
  // The group here must be a positive power of 2, in which case that is used as
  // size. All other values shall result in the default value being used.
  const std::string group_name =
      field_trials.Lookup("WebRTC-PacketBufferMaxSize");
  int packet_buffer_max_size = kPacketBufferMaxSize;
  if (!group_name.empty() &&
      (sscanf(group_name.c_str(), "%d", &packet_buffer_max_size) != 1 ||
       packet_buffer_max_size <= 0 ||
       // Verify that the number is a positive power of 2.
       (packet_buffer_max_size & (packet_buffer_max_size - 1)) != 0)) {
    RTC_LOG(LS_WARNING) << "Invalid packet buffer max size: " << group_name;
    packet_buffer_max_size = kPacketBufferMaxSize;
  }
  return packet_buffer_max_size;
}

std::unique_ptr<ModuleRtpRtcpImpl2> CreateRtpRtcpModule(
    const Environment& env,
    ReceiveStatistics* receive_statistics,
    FecReceiveStatistics* fec_receive_statistics,
    Transport* outgoing_transport,
    RtcpRttStats* rtt_stats,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    RtcpCnameCallback* rtcp_cname_callback,
    bool non_sender_rtt_measurement,
    uint32_t local_ssrc) {
  RtpRtcpInterface::Configuration configuration;
  configuration.audio = false;
  configuration.receiver_only = true;
  configuration.receive_statistics = receive_statistics;
  configuration.fec_receive_statistics = fec_receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer =
      rtcp_packet_type_counter_observer;
  configuration.rtcp_cname_callback = rtcp_cname_callback;
  configuration.local_media_ssrc = local_ssrc;
  configuration.non_sender_rtt_measurement = non_sender_rtt_measurement;

  auto rtp_rtcp = std::make_unique<ModuleRtpRtcpImpl2>(env, configuration);
  rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);

  return rtp_rtcp;
}

std::unique_ptr<NackRequester> MaybeConstructNackModule(
    const Environment& env,
    TaskQueueBase* current_queue,
    NackPeriodicProcessor* nack_periodic_processor,
    const NackConfig& nack,
    NackSender* nack_sender,
    KeyFrameRequestSender* keyframe_request_sender) {
  if (nack.rtp_history_ms == 0)
    return nullptr;

  // TODO(bugs.webrtc.org/12420): pass rtp_history_ms to the nack module.
  return std::make_unique<NackRequester>(
      current_queue, nack_periodic_processor, &env.clock(), nack_sender,
      keyframe_request_sender, env.field_trials());
}

std::unique_ptr<fecReceiver> MaybeConstructfecReceiver(
    uint32_t remote_ssrc,
    int red_payload_type,
    int fec_payload_type,
    RecoveredPacketReceiver* callback,
    Clock* clock) {
  RTC_DCHECK_GE(red_payload_type, -1);
  RTC_DCHECK_GE(fec_payload_type, -1);
  if (red_payload_type == -1)
    return nullptr;

  // TODO(tommi, brandtr): Consider including this check too once
  // `UlpfecReceiver` has been updated to not consider both red and ulpfec
  // payload ids.
  //  if (ulpfec_payload_type == -1)
  //    return nullptr;


  if (inputV::Params::type == inputV::ExpType::WebRTCSource){
    return std::make_unique<UlpfecReceiver>(remote_ssrc, fec_payload_type, callback, clock);
  }
  else if (inputV::Params::type == inputV::ExpType::RSFECBlock || inputV::Params::type == inputV::ExpType::RSFECStreamStableRate || 
            inputV::Params::type == inputV::ExpType::RSFECStreamSourceRate || inputV::Params::type == inputV::ExpType::RLSRSFEC){
    return std::make_unique<RSfecReceiver>(remote_ssrc, fec_payload_type, callback, clock);
  }
  else if (inputV::Params::type == inputV::ExpType::TamburFEC){
    return std::make_unique<TamburFecReceiver>(remote_ssrc, fec_payload_type, callback, clock);
  }
  else if (inputV::Params::type == inputV::ExpType::FECClose){
    return std::make_unique<UlpfecReceiver>(remote_ssrc, fec_payload_type, callback, clock);
  }
  else {
    return std::make_unique<UlpfecReceiver>(remote_ssrc, fec_payload_type, callback, clock);
  }
}

static const int kPacketLogIntervalMs = 10000;

}  // namespace

RtpVideoStreamReceiver2::RtcpFeedbackBuffer::RtcpFeedbackBuffer(
    KeyFrameRequestSender* key_frame_request_sender,
    NackSender* nack_sender,
    LossNotificationSender* loss_notification_sender)
    : key_frame_request_sender_(key_frame_request_sender),
      nack_sender_(nack_sender),
      loss_notification_sender_(loss_notification_sender),
      request_key_frame_(false) {
  RTC_DCHECK(key_frame_request_sender_);
  RTC_DCHECK(nack_sender_);
  RTC_DCHECK(loss_notification_sender_);
  packet_sequence_checker_.Detach();
}

void RtpVideoStreamReceiver2::RtcpFeedbackBuffer::RequestKeyFrame() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  request_key_frame_ = true;
}

void RtpVideoStreamReceiver2::RtcpFeedbackBuffer::SendNack(
    const std::vector<uint16_t>& sequence_numbers,
    bool buffering_allowed) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK(!sequence_numbers.empty());
  nack_sequence_numbers_.insert(nack_sequence_numbers_.end(),
                                sequence_numbers.cbegin(),
                                sequence_numbers.cend());
  if (!buffering_allowed) {
    // Note that while *buffering* is not allowed, *batching* is, meaning that
    // previously buffered messages may be sent along with the current message.
    SendBufferedRtcpFeedback();
  }
}

void RtpVideoStreamReceiver2::RtcpFeedbackBuffer::SendLossNotification(
    uint16_t last_decoded_seq_num,
    uint16_t last_received_seq_num,
    bool decodability_flag,
    bool buffering_allowed) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK(buffering_allowed);
  RTC_DCHECK(!lntf_state_)
      << "SendLossNotification() called twice in a row with no call to "
         "SendBufferedRtcpFeedback() in between.";
  lntf_state_ = std::make_optional<LossNotificationState>(
      last_decoded_seq_num, last_received_seq_num, decodability_flag);
}

void RtpVideoStreamReceiver2::RtcpFeedbackBuffer::SendBufferedRtcpFeedback() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  bool request_key_frame = false;
  std::vector<uint16_t> nack_sequence_numbers;
  std::optional<LossNotificationState> lntf_state;

  std::swap(request_key_frame, request_key_frame_);
  std::swap(nack_sequence_numbers, nack_sequence_numbers_);
  std::swap(lntf_state, lntf_state_);

  if (lntf_state) {
    // If either a NACK or a key frame request is sent, we should buffer
    // the LNTF and wait for them (NACK or key frame request) to trigger
    // the compound feedback message.
    // Otherwise, the LNTF should be sent out immediately.
    const bool buffering_allowed =
        request_key_frame || !nack_sequence_numbers.empty();

    loss_notification_sender_->SendLossNotification(
        lntf_state->last_decoded_seq_num, lntf_state->last_received_seq_num,
        lntf_state->decodability_flag, buffering_allowed);
  }

  if (request_key_frame) {
    key_frame_request_sender_->RequestKeyFrame();
  } else if (!nack_sequence_numbers.empty()) {
    nack_sender_->SendNack(nack_sequence_numbers, true);
  }
}

void RtpVideoStreamReceiver2::RtcpFeedbackBuffer::ClearLossNotificationState() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  lntf_state_.reset();
}

RtpVideoStreamReceiver2::RtpVideoStreamReceiver2(
    const Environment& env,
    TaskQueueBase* current_queue,
    Transport* transport,
    RtcpRttStats* rtt_stats,
    PacketRouter* packet_router,
    const VideoReceiveStreamInterface::Config* config,
    ReceiveStatistics* rtp_receive_statistics,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    RtcpCnameCallback* rtcp_cname_callback,
    NackPeriodicProcessor* nack_periodic_processor,
    OnCompleteFrameCallback* complete_frame_callback,
    scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    scoped_refptr<FrameTransformerInterface> frame_transformer)
    : env_(env),
      worker_queue_(current_queue),
      config_(*config),
      packet_router_(packet_router),
      ntp_estimator_(&env_.clock()),
      forced_playout_delay_max_ms_("max_ms", std::nullopt),
      forced_playout_delay_min_ms_("min_ms", std::nullopt),
      rtp_receive_statistics_(rtp_receive_statistics),
      fec_receiver_(
          MaybeConstructfecReceiver(config->rtp.remote_ssrc,
                                       config->rtp.red_payload_type,
                                       config->rtp.ulpfec_payload_type,
                                       this,
                                       &env_.clock())),
      red_payload_type_(config_.rtp.red_payload_type),
      packet_sink_(config->rtp.packet_sink_),
      receiving_(false),
      last_packet_log_ms_(-1),
      fec_receive_statistics_(FecReceiveStatistics::Create(&env_.clock())),
      rtp_rtcp_(CreateRtpRtcpModule(
          env_,
          rtp_receive_statistics_,
          fec_receive_statistics_.get(),
          transport,
          rtt_stats,
          rtcp_packet_type_counter_observer,
          rtcp_cname_callback,
          config_.rtp.rtcp_xr.receiver_reference_time_report,
          config_.rtp.local_ssrc)),
      nack_periodic_processor_(nack_periodic_processor),
      complete_frame_callback_(complete_frame_callback),
      keyframe_request_method_(config_.rtp.keyframe_method),
      // TODO(bugs.webrtc.org/10336): Let `rtcp_feedback_buffer_` communicate
      // directly with `rtp_rtcp_`.
      rtcp_feedback_buffer_(this, this, this),
      nack_module_(MaybeConstructNackModule(env_,
                                            current_queue,
                                            nack_periodic_processor,
                                            config_.rtp.nack,
                                            &rtcp_feedback_buffer_,
                                            &rtcp_feedback_buffer_)),
      packet_buffer_(kPacketBufferStartSize,
                     PacketBufferMaxSize(env_.field_trials())),
      reference_finder_(std::make_unique<RtpFrameReferenceFinder>()),
      has_received_frame_(false),
      frames_decryptable_(false),
      absolute_capture_time_interpolator_(&env_.clock()) {
  
  // Initialize Tambur FEC components if in TamburFEC mode
  if (inputV::Params::type == inputV::ExpType::TamburFEC) {
    InitializeTamburFEC();
  }
  
  packet_sequence_checker_.Detach();
  if (packet_router_) {
    // Do not register as REMB candidate, this is only done when starting to
    // receive.
    packet_router_->AddReceiveRtpModule(rtp_rtcp_.get(),
                                        /*remb_candidate=*/false);
  }

  RTC_DCHECK(config_.rtp.rtcp_mode != RtcpMode::kOff)
      << "A stream should not be configured with RTCP disabled. This value is "
         "reserved for internal usage.";
  // TODO(pbos): What's an appropriate local_ssrc for receive-only streams?
  RTC_DCHECK(config_.rtp.local_ssrc != 0);
  RTC_DCHECK(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);

  rtp_rtcp_->SetRTCPStatus(config_.rtp.rtcp_mode);
  rtp_rtcp_->SetRemoteSSRC(config_.rtp.remote_ssrc);

  if (config_.rtp.nack.rtp_history_ms > 0) {
    rtp_receive_statistics_->SetMaxReorderingThreshold(config_.rtp.remote_ssrc,
                                                       kMaxPacketAgeToNack);
  }
  ParseFieldTrial(
      {&forced_playout_delay_max_ms_, &forced_playout_delay_min_ms_},
      env_.field_trials().Lookup("WebRTC-ForcePlayoutDelay"));

  if (config_.rtp.lntf.enabled) {
    loss_notification_controller_ =
        std::make_unique<LossNotificationController>(&rtcp_feedback_buffer_,
                                                     &rtcp_feedback_buffer_);
  }

  // Only construct the encrypted receiver if frame encryption is enabled.
  if (config_.crypto_options.sframe.require_frame_encryption) {
    buffered_frame_decryptor_ = std::make_unique<BufferedFrameDecryptor>(
        this, this, env_.field_trials());
    if (frame_decryptor != nullptr) {
      buffered_frame_decryptor_->SetFrameDecryptor(std::move(frame_decryptor));
    }
  }

  if (frame_transformer) {
    frame_transformer_delegate_ =
        rtc::make_ref_counted<RtpVideoStreamReceiverFrameTransformerDelegate>(
            this, &env_.clock(), std::move(frame_transformer),
            rtc::Thread::Current(), config_.rtp.remote_ssrc);
    frame_transformer_delegate_->Init();
  }
}

std::optional<Datagram> RtpVideoStreamReceiver2::ConvertRtpToDatagram(
    const RtpPacketReceived& rtp_packet) const {
  const auto payload = rtp_packet.payload();
  if (payload.empty()) {
    return std::nullopt;
  }
  const std::string binary(reinterpret_cast<const char*>(payload.data()),
                           payload.size());
  Datagram datagram;
  if (!datagram.parse_from_string(binary)) {
    return std::nullopt;
  }
  return datagram;
}

// Initialize Tambur FEC components
void RtpVideoStreamReceiver2::InitializeTamburFEC() {
  RTC_LOG(LS_INFO) << "Initializing Tambur FEC receiver";
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  if (!tambur_decoder_) {

    // Get configuration parameters from WebRTC config
    uint16_t tau = inputV::Params::tambur_tau; // default FEC protection window size 
    int stripe_size = inputV::Params::tambur_stripe_size; // default stripe size in bytes
    uint16_t w = inputV::Params::tambur_w; // Default Galois field size
    int max_data_stripes_per_frame = inputV::Params::tambur_max_data_stripes_per_frame; // Default Max data stripes per frame
    int max_fec_stripes_per_frame = inputV::Params::tambur_max_fec_stripes_per_frame; // Default Max FEC stripes per frame
    int packet_size = inputV::Params::tambur_packet_size; // Default Packet size 
    uint8_t max_qr = inputV::Params::tambur_max_qr; // Default Max QR value
    uint8_t num_qrs_no_reduce = inputV::Params::tambur_num_qrs_no_reduce;

    // Calculate number of frames for delay
    uint16_t num_frames = num_frames_for_delay(tau);
    uint16_t n_cols = num_frames;
    uint16_t n_rows = 256;
    while (n_rows * n_cols > 255 or (n_rows % num_frames))
    {
      n_rows--;
    }

    std::string log_folder = "logs/";
    std::vector<MetricLogger *> metricLoggers;
    auto timingLogger = std::make_unique<TimingLogger>(log_folder + "receiver/");
    auto frameLogger = std::make_unique<FrameLogger>(log_folder + "receiver/");
    metricLoggers.push_back(timingLogger.get());
    auto logger = std::make_unique<Logger>(log_folder + "receiver/", metricLoggers, frameLogger.get());

    // 1. Create header coding matrix info
    printf("\t\t\t== Receiver == 1. Create header coding matrix info\n");
    CodingMatrixInfo coding_matrix_info_header{n_rows, n_cols, 8};

    // 2. Create FEC coding matrix info
    printf("\t\t\t== Receiver == 2. Create FEC coding matrix info\n");
    CodingMatrixInfo coding_matrix_info_fec{
        uint16_t(num_frames_for_delay(tau) * max_fec_stripes_per_frame),
        uint16_t(num_frames_for_delay(tau) * max_data_stripes_per_frame), 
        w};

    // 3. Create header block code
    printf("\t\t\t== Receiver == 3. Create header block code\n");
    auto block_code_header = std::make_unique<BlockCode>(
        coding_matrix_info_header, tau, std::pair<uint16_t, uint16_t>{1, 0}, 8, false);

    // 4. Create FEC block code
    printf("\t\t\t== Receiver == 4. Create FEC block code\n");    
    auto block_code_fec = std::make_unique<BlockCode>(
        coding_matrix_info_fec, tau, std::pair<uint16_t, uint16_t>{1, 1}, uint16_t(packet_size), false, logger.get());
    // 5. Create header code
    printf("\t\t\t== Receiver == 5. Create header code\n");
    auto tambur_header_code = std::make_unique<MultiFECHeaderCode>(
        block_code_header.get(), tau, coding_matrix_info_header.n_rows / num_frames_for_delay(tau));

    // 6. Create streaming code
    printf("\t\t\t== Receiver == 6. Create streaming code\n");
    auto tambur_code = std::make_unique<StreamingCode>(
        tau, stripe_size, block_code_fec.get(), w, max_data_stripes_per_frame, max_fec_stripes_per_frame);
    
    // 7. Create quality reporter generator
    printf("\t\t\t== Receiver == 7. Create quality reporter generator\n");
    auto quality_report_generator = std::make_unique<SimpleQualityReportGenerator>(
        num_qrs_no_reduce, max_qr);
    
    // 8. Create quality reporter
    printf("\t\t\t== Receiver == 8. Create quality reporter\n");
    auto quality_reporter = std::make_unique<QualityReporter>(nullptr, nullptr, quality_report_generator.get());

    // 9. Create FEC multi receiver
    printf("\t\t\t== Receiver == 9. Create FEC multi receiver\n");
    tambur_fec_receiver_ = std::make_unique<FECMultiReceiver>(
        tambur_code.get(), 
        tau,
        num_frames_for_delay(tau),
        tambur_header_code.get(),
        quality_reporter.get(),
        logger.get(),
        2000.0,
        true,
        true);

    // 10. Create Tambur Decoder
    printf("\t\t\t== Receiver == 10. Create Tambur Decoder\n");
    tambur_decoder_ = std::make_unique<TamburDecoder>(tambur_fec_receiver_.get());

    // Store Tambur FEC components
    block_code_header_ = std::move(block_code_header);
    block_code_fec_ = std::move(block_code_fec);
    tambur_header_code_ = std::move(tambur_header_code);
    tambur_code_ = std::move(tambur_code);
    tambur_quality_reporter_ = std::move(quality_reporter);
    logger_ = std::move(logger);
    frameLogger_ = std::move(frameLogger);
    timingLogger_ = std::move(timingLogger);
  }
}

RtpVideoStreamReceiver2::~RtpVideoStreamReceiver2() {
  if (packet_router_)
    packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
  fec_receiver_.reset();
  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Reset();
}

void RtpVideoStreamReceiver2::AddReceiveCodec(
    uint8_t payload_type,
    VideoCodecType video_codec,
    const webrtc::CodecParameterMap& codec_params,
    bool raw_payload) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (codec_params.count(cricket::kH264FmtpSpsPpsIdrInKeyframe) > 0 ||
      env_.field_trials().IsEnabled("WebRTC-SpsPpsIdrIsH264Keyframe")) {
    packet_buffer_.ForceSpsPpsIdrIsH264Keyframe();
    sps_pps_idr_is_h264_keyframe_ = true;
  }
  payload_type_map_.emplace(
      payload_type, raw_payload ? std::make_unique<VideoRtpDepacketizerRaw>()
                                : CreateVideoRtpDepacketizer(video_codec));
  pt_codec_params_.emplace(payload_type, codec_params);
}

void RtpVideoStreamReceiver2::RemoveReceiveCodecs() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  pt_codec_params_.clear();
  payload_type_map_.clear();
  packet_buffer_.ResetSpsPpsIdrIsH264Keyframe();
  h26x_packet_buffer_.reset();
}

std::optional<Syncable::Info> RtpVideoStreamReceiver2::GetSyncInfo() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  Syncable::Info info;
  std::optional<RtpRtcpInterface::SenderReportStats> last_sr =
      rtp_rtcp_->GetSenderReportStats();
  if (!last_sr.has_value()) {
    return std::nullopt;
  }
  info.capture_time_ntp_secs = last_sr->last_remote_ntp_timestamp.seconds();
  info.capture_time_ntp_frac = last_sr->last_remote_ntp_timestamp.fractions();
  info.capture_time_source_clock = last_sr->last_remote_rtp_timestamp;

  if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_) {
    return std::nullopt;
  }
  info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
  info.latest_receive_time_ms = last_received_rtp_system_time_->ms();

  // Leaves info.current_delay_ms uninitialized.
  return info;
}

RtpVideoStreamReceiver2::ParseGenericDependenciesResult
RtpVideoStreamReceiver2::ParseGenericDependenciesExtension(
    const RtpPacketReceived& rtp_packet,
    RTPVideoHeader* video_header) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (DependencyDescriptorMandatory dd_mandatory;
      rtp_packet.GetExtension<RtpDependencyDescriptorExtension>(
          &dd_mandatory)) {
    const int64_t frame_id =
        frame_id_unwrapper_.Unwrap(dd_mandatory.frame_number());
    DependencyDescriptor dependency_descriptor;
    if (!rtp_packet.GetExtension<RtpDependencyDescriptorExtension>(
            video_structure_.get(), &dependency_descriptor)) {
      if (!video_structure_frame_id_ || frame_id < video_structure_frame_id_) {
        return kDropPacket;
      } else {
        return kStashPacket;
      }
    }

    if (dependency_descriptor.attached_structure != nullptr &&
        !dependency_descriptor.first_packet_in_frame) {
      RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.Ssrc()
                          << "Invalid dependency descriptor: structure "
                             "attached to non first packet of a frame.";
      return kDropPacket;
    }
    video_header->is_first_packet_in_frame =
        dependency_descriptor.first_packet_in_frame;
    video_header->is_last_packet_in_frame =
        dependency_descriptor.last_packet_in_frame;

    auto& generic_descriptor_info = video_header->generic.emplace();
    generic_descriptor_info.frame_id = frame_id;
    generic_descriptor_info.spatial_index =
        dependency_descriptor.frame_dependencies.spatial_id;
    generic_descriptor_info.temporal_index =
        dependency_descriptor.frame_dependencies.temporal_id;
    for (int fdiff : dependency_descriptor.frame_dependencies.frame_diffs) {
      generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
    }
    generic_descriptor_info.decode_target_indications =
        dependency_descriptor.frame_dependencies.decode_target_indications;
    if (dependency_descriptor.resolution) {
      video_header->width = dependency_descriptor.resolution->Width();
      video_header->height = dependency_descriptor.resolution->Height();
    }

    // FrameDependencyStructure is sent in dependency descriptor of the first
    // packet of a key frame and required for parsed dependency descriptor in
    // all the following packets until next key frame.
    // Save it if there is a (potentially) new structure.
    if (dependency_descriptor.attached_structure) {
      RTC_DCHECK(dependency_descriptor.first_packet_in_frame);
      if (video_structure_frame_id_ > frame_id) {
        RTC_LOG(LS_WARNING)
            << "Arrived key frame with id " << frame_id << " and structure id "
            << dependency_descriptor.attached_structure->structure_id
            << " is older than the latest received key frame with id "
            << *video_structure_frame_id_ << " and structure id "
            << video_structure_->structure_id;
        return kDropPacket;
      }
      video_structure_ = std::move(dependency_descriptor.attached_structure);
      video_structure_frame_id_ = frame_id;
      video_header->frame_type = VideoFrameType::kVideoFrameKey;
    } else {
      video_header->frame_type = VideoFrameType::kVideoFrameDelta;
    }
    return kHasGenericDescriptor;
  }

  RtpGenericFrameDescriptor generic_frame_descriptor;
  if (!rtp_packet.GetExtension<RtpGenericFrameDescriptorExtension00>(
          &generic_frame_descriptor)) {
    return kNoGenericDescriptor;
  }

  video_header->is_first_packet_in_frame =
      generic_frame_descriptor.FirstPacketInSubFrame();
  video_header->is_last_packet_in_frame =
      generic_frame_descriptor.LastPacketInSubFrame();

  if (generic_frame_descriptor.FirstPacketInSubFrame()) {
    video_header->frame_type =
        generic_frame_descriptor.FrameDependenciesDiffs().empty()
            ? VideoFrameType::kVideoFrameKey
            : VideoFrameType::kVideoFrameDelta;

    auto& generic_descriptor_info = video_header->generic.emplace();
    int64_t frame_id =
        frame_id_unwrapper_.Unwrap(generic_frame_descriptor.FrameId());
    generic_descriptor_info.frame_id = frame_id;
    generic_descriptor_info.spatial_index =
        generic_frame_descriptor.SpatialLayer();
    generic_descriptor_info.temporal_index =
        generic_frame_descriptor.TemporalLayer();
    for (uint16_t fdiff : generic_frame_descriptor.FrameDependenciesDiffs()) {
      generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
    }
  }
  video_header->width = generic_frame_descriptor.Width();
  video_header->height = generic_frame_descriptor.Height();
  return kHasGenericDescriptor;
}

void RtpVideoStreamReceiver2::SetLastCorruptionDetectionIndex(
    const absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>&
        frame_instrumentation_data) {
  if (const auto* sync_data = absl::get_if<FrameInstrumentationSyncData>(
          &frame_instrumentation_data)) {
    last_corruption_detection_index_ = sync_data->sequence_index;
  } else if (const auto* data = absl::get_if<FrameInstrumentationData>(
                 &frame_instrumentation_data)) {
    last_corruption_detection_index_ =
        data->sequence_index + data->sample_values.size();
  } else {
    RTC_DCHECK_NOTREACHED();
  }
}

bool RtpVideoStreamReceiver2::OnReceivedPayloadData(
    rtc::CopyOnWriteBuffer codec_payload,
    const RtpPacketReceived& rtp_packet,
    const RTPVideoHeader& video,
    int times_nacked) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  int64_t unwrapped_rtp_seq_num =
      rtp_seq_num_unwrapper_.Unwrap(rtp_packet.SequenceNumber());

  auto packet = std::make_unique<video_coding::PacketBuffer::Packet>(
      rtp_packet, unwrapped_rtp_seq_num, video);

  RtpPacketInfo& packet_info =
      packet_infos_
          .emplace(unwrapped_rtp_seq_num,
                   RtpPacketInfo(rtp_packet.Ssrc(), rtp_packet.Csrcs(),
                                 rtp_packet.Timestamp(),
                                 /*receive_time=*/env_.clock().CurrentTime()))
          .first->second;

  // Try to extrapolate absolute capture time if it is missing.
  packet_info.set_absolute_capture_time(
      absolute_capture_time_interpolator_.OnReceivePacket(
          AbsoluteCaptureTimeInterpolator::GetSource(packet_info.ssrc(),
                                                     packet_info.csrcs()),
          packet_info.rtp_timestamp(),
          // Assume frequency is the same one for all video frames.
          kVideoPayloadTypeFrequency,
          rtp_packet.GetExtension<AbsoluteCaptureTimeExtension>()));
  if (packet_info.absolute_capture_time().has_value()) {
    packet_info.set_local_capture_clock_offset(
        capture_clock_offset_updater_.ConvertsToTimeDela(
            capture_clock_offset_updater_.AdjustEstimatedCaptureClockOffset(
                packet_info.absolute_capture_time()
                    ->estimated_capture_clock_offset)));
  }
  RTPVideoHeader& video_header = packet->video_header;
  video_header.rotation = kVideoRotation_0;
  video_header.content_type = VideoContentType::UNSPECIFIED;
  video_header.video_timing.flags = VideoSendTiming::kInvalid;
  video_header.is_last_packet_in_frame |= rtp_packet.Marker();

  rtp_packet.GetExtension<VideoOrientation>(&video_header.rotation);
  rtp_packet.GetExtension<VideoContentTypeExtension>(
      &video_header.content_type);
  rtp_packet.GetExtension<VideoTimingExtension>(&video_header.video_timing);
  if (forced_playout_delay_max_ms_ && forced_playout_delay_min_ms_) {
    if (!video_header.playout_delay.emplace().Set(
            TimeDelta::Millis(*forced_playout_delay_min_ms_),
            TimeDelta::Millis(*forced_playout_delay_max_ms_))) {
      video_header.playout_delay = std::nullopt;
    }
  } else {
    video_header.playout_delay = rtp_packet.GetExtension<PlayoutDelayLimits>();
  }

  if (!rtp_packet.recovered()) {
    UpdatePacketReceiveTimestamps(
        rtp_packet, video_header.frame_type == VideoFrameType::kVideoFrameKey);
  }

  ParseGenericDependenciesResult generic_descriptor_state =
      ParseGenericDependenciesExtension(rtp_packet, &video_header);

  if (generic_descriptor_state == kStashPacket) {
    return true;
  } else if (generic_descriptor_state == kDropPacket) {
    Timestamp now = env_.clock().CurrentTime();
    if (now - last_logged_failed_to_parse_dd_ > TimeDelta::Seconds(1)) {
      last_logged_failed_to_parse_dd_ = now;
      RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.Ssrc()
                          << " Failed to parse dependency descriptor.";
    }
    if (video_structure_ == nullptr &&
        next_keyframe_request_for_missing_video_structure_ < now) {
      // No video structure received yet, most likely part of the initial
      // keyframe was lost.
      RequestKeyFrame();
      next_keyframe_request_for_missing_video_structure_ =
          now + TimeDelta::Seconds(1);
    }
    return false;
  }

  // Extensions that should only be transmitted in the last packet of a frame.
  if (video_header.is_last_packet_in_frame) {
    video_header.color_space = rtp_packet.GetExtension<ColorSpaceExtension>();
    if (video_header.color_space ||
        video_header.frame_type == VideoFrameType::kVideoFrameKey) {
      // Store color space since it's only transmitted when changed or for key
      // frames. Color space will be cleared if a key frame is transmitted
      // without color space information.
      last_color_space_ = video_header.color_space;
    } else if (last_color_space_) {
      video_header.color_space = last_color_space_;
    }
    CorruptionDetectionMessage message;
    rtp_packet.GetExtension<CorruptionDetectionExtension>(&message);
    if (message.sample_values().empty()) {
      video_header.frame_instrumentation_data =
          ConvertCorruptionDetectionMessageToFrameInstrumentationSyncData(
              message, last_corruption_detection_index_);
    } else {
      video_header.frame_instrumentation_data =
          ConvertCorruptionDetectionMessageToFrameInstrumentationData(
              message, last_corruption_detection_index_);
    }
    if (video_header.frame_instrumentation_data.has_value()) {
      SetLastCorruptionDetectionIndex(*video_header.frame_instrumentation_data);
    }
  }
  video_header.video_frame_tracking_id =
      rtp_packet.GetExtension<VideoFrameTrackingIdExtension>();

  if (loss_notification_controller_) {
    if (rtp_packet.recovered()) {
      // TODO(bugs.webrtc.org/10336): Implement support for reordering.
      RTC_LOG(LS_INFO)
          << "LossNotificationController does not support reordering.";
    } else if (generic_descriptor_state == kNoGenericDescriptor) {
      RTC_LOG(LS_WARNING) << "LossNotificationController requires generic "
                             "frame descriptor, but it is missing.";
    } else {
      if (video_header.is_first_packet_in_frame) {
        RTC_DCHECK(video_header.generic);
        LossNotificationController::FrameDetails frame;
        frame.is_keyframe =
            video_header.frame_type == VideoFrameType::kVideoFrameKey;
        frame.frame_id = video_header.generic->frame_id;
        frame.frame_dependencies = video_header.generic->dependencies;
        loss_notification_controller_->OnReceivedPacket(
            rtp_packet.SequenceNumber(), &frame);
      } else {
        loss_notification_controller_->OnReceivedPacket(
            rtp_packet.SequenceNumber(), nullptr);
      }
    }
  }

  packet->times_nacked = times_nacked;

  if (codec_payload.size() == 0) {
    NotifyReceiverOfEmptyPacket(packet->seq_num());
    rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
    return false;
  }

  // 我认为我应该在这里进入 Tambur 处理逻辑
  // Tambur FEC processing branch - handle Tambur FEC packets here
  if (inputV::Params::type == inputV::ExpType::TamburFEC) {
    return ProcessTamburFECPacket(std::shared_ptr<const video_coding::PacketBuffer::Packet>(std::move(packet)), rtp_packet);
  }

  if (packet->codec() == kVideoCodecH264) {
    // Only when we start to receive packets will we know what payload type
    // that will be used. When we know the payload type insert the correct
    // sps/pps into the tracker.
    if (packet->payload_type != last_payload_type_) {
      last_payload_type_ = packet->payload_type;
      InsertSpsPpsIntoTracker(packet->payload_type);
    }
  }

  if (packet->codec() == kVideoCodecH264 && !h26x_packet_buffer_) {
    video_coding::H264SpsPpsTracker::FixedBitstream fixed =
        tracker_.CopyAndFixBitstream(
            rtc::MakeArrayView(codec_payload.cdata(), codec_payload.size()),
            &packet->video_header);

    switch (fixed.action) {
      case video_coding::H264SpsPpsTracker::kRequestKeyframe:
        rtcp_feedback_buffer_.RequestKeyFrame();
        rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
        [[fallthrough]];
      case video_coding::H264SpsPpsTracker::kDrop:
        return false;
      case video_coding::H264SpsPpsTracker::kInsert:
        packet->video_payload = std::move(fixed.bitstream);
        break;
    }

  } else {
    packet->video_payload = std::move(codec_payload);
  }

  rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
  frame_counter_.Add(packet->timestamp);

  if ((packet->codec() == kVideoCodecH264 ||
       packet->codec() == kVideoCodecH265) &&
      h26x_packet_buffer_) {
    OnInsertedPacket(h26x_packet_buffer_->InsertPacket(std::move(packet)));
  } else {
    OnInsertedPacket(packet_buffer_.InsertPacket(std::move(packet)));
  }
  return false;
}

// Tambur FEC处理方法 - 正确处理Tambur数据并调用原生解包器
bool RtpVideoStreamReceiver2::ProcessTamburFECPacket(std::shared_ptr<const video_coding::PacketBuffer::Packet> packet, const RtpPacketReceived& rtp_packet) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  RTC_LOG(LS_INFO) << "Processing Tambur FEC packet: RTP_seq=" << packet->sequence_number
                   << ", timestamp=" << packet->timestamp
                   << ", payload_type=" << static_cast<int>(packet->payload_type)
                   << ", size=" << rtp_packet.payload().size();
  
  // printf 输出 RTP payload 的前 40 个字节，以16进制输出，例如 AA BB CC 这样，输出到一行即可，开头为 \t == Receive == \t
  printf("\t== Receive RTP ==\t");
  for (size_t i = 0; i < 40 && i < rtp_packet.payload().size(); i++) {
    printf("%02X ", (unsigned char)rtp_packet.payload().data()[i]);
  }
  printf("    size: %lu", rtp_packet.payload().size());
  printf("\n");

  if (!tambur_decoder_) {
    RTC_LOG(LS_ERROR) << "TamburDecoder not initialized";
    return false;
  }

  // Convert RTP packet payload to Tambur Datagram
  const std::string binary(reinterpret_cast<const char*>(rtp_packet.payload().data()), 
                          rtp_packet.payload().size());
  Datagram datagram;
  if (!datagram.parse_from_string(binary)) {
    RTC_LOG(LS_WARNING) << "Failed to parse Tambur datagram from RTP packet payload";
    return false;
  }

  // printf 输出 datagram 的前 40 个字节，以16进制输出，例如 AA BB CC 这样，输出到一行即可，开头为 \t == Receive == \t
  printf("\t== Receive Datagram ==\t");
  for (size_t i = 0; i < 40 && i < datagram.payload.size(); i++) {
    printf("%02X ", (unsigned char)datagram.payload[i]);
  }
  printf("    size: %lu", datagram.payload.size());
  printf("\n");

  // Add datagram to TamburDecoder with complete packet object
  bool success = tambur_decoder_->add_datagram(datagram, packet);
  
  if (!success) {
    RTC_LOG(LS_WARNING) << "Failed to add RTP packet to TamburDecoder";
    return false;
  }

  // Check if there are complete frames ready
  while (auto timestamped_frame = tambur_decoder_->get_next_timestamped_frame()) {
    ProcessDecodedTamburFrame(timestamped_frame->frame_data,
                              timestamped_frame->rtp_timestamp,
                              timestamped_frame->frame_info,
                              timestamped_frame->packets,
                              timestamped_frame->first_packet,
                              timestamped_frame->last_packet);
  }

  return true;
}

// 实现ProcessDecodedTamburFrame方法
void RtpVideoStreamReceiver2::ProcessDecodedTamburFrame(
    const std::string& frame_data,
    uint32_t rtp_timestamp,
    const TamburDecoder::FrameInfo& frame_info,
    const std::vector<std::shared_ptr<const video_coding::PacketBuffer::Packet>>& packets,
    const std::optional<std::shared_ptr<const video_coding::PacketBuffer::Packet>>& first_packet,
    const std::optional<std::shared_ptr<const video_coding::PacketBuffer::Packet>>& last_packet) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  printf("\t\t\t== ProcessDecodedTamburFrame\n");

  RTC_LOG(LS_INFO) << "Processing decoded Tambur frame: "
                   << "size=" << frame_data.size()
                   << ", timestamp=" << rtp_timestamp
                   << ", frame_id=" << frame_info.frame_id
                   << ", type=" << (frame_info.is_key_frame ? "KEY" : "DELTA")
                   << ", payload_type=" << static_cast<int>(frame_info.payload_type)
                   << ", packets=" << packets.size()
                   << ", has_first_packet=" << first_packet.has_value()
                   << ", has_last_packet=" << last_packet.has_value();
  
  if (!first_packet.has_value() || !last_packet.has_value()) {
    RTC_LOG(LS_WARNING) << "No first or last packet available for decoded Tambur frame";
    return;
  }

  RTPVideoHeader video_header = first_packet.value()->video_header;
  video_header.frame_type = frame_info.is_key_frame ? VideoFrameType::kVideoFrameKey
                                                    : VideoFrameType::kVideoFrameDelta;

  // Convert Tambur decoded data to WebRTC format
  // auto image_buffer = EncodedImageBuffer::Create(
  //     reinterpret_cast<const uint8_t*>(frame_data.data()), frame_data.size());

  rtc::scoped_refptr<EncodedImageBuffer> image_buffer;
  if (first_packet.value()->codec() == kVideoCodecH264 && !h26x_packet_buffer_) {
    // Keep tracker payload-type state updated even though we bypass OnReceivedPayloadData.
    if (first_packet.value()->payload_type != last_payload_type_) {
      last_payload_type_ = first_packet.value()->payload_type;
      InsertSpsPpsIntoTracker(first_packet.value()->payload_type);
    }

    video_coding::H264SpsPpsTracker::FixedBitstream fixed =
        tracker_.CopyAndFixBitstream(
            rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(frame_data.data()),
                               frame_data.size()),
            &video_header);

    switch (fixed.action) {
      case video_coding::H264SpsPpsTracker::kRequestKeyframe:
        rtcp_feedback_buffer_.RequestKeyFrame();
        rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
        return;
      case video_coding::H264SpsPpsTracker::kDrop:
        return;
      case video_coding::H264SpsPpsTracker::kInsert:
        image_buffer = EncodedImageBuffer::Create(
            reinterpret_cast<const uint8_t*>(fixed.bitstream.cdata()),
            fixed.bitstream.size());
        break;
    }
  } else {
    image_buffer = EncodedImageBuffer::Create(reinterpret_cast<const uint8_t*>(frame_data.data()), frame_data.size());
  }

  if (!image_buffer) {
    RTC_LOG(LS_WARNING) << "Failed to create EncodedImageBuffer for decoded Tambur frame";
    return;
  }

  printf("\t\t\t== Receive video header: width:%d\theight:%d\n", video_header.width, video_header.height);
  printf("\t\t\t== payload size: %zu", image_buffer->size());
  printf("\t\t\t== first_packet seq_num:%d\t last_packet seq_num:%d\n", first_packet.value()->seq_num(), last_packet.value()->seq_num());
  printf("\t\t\t== max_nack_count:%d\t min_recv_time:%ld\t max_recv_time:%ld\n", 0, frame_info.min_receive_time, frame_info.max_receive_time);
  printf("\t\t\t== payload_type:%d\t codec:%d\t rotation:%d\t content_type:%d\n", first_packet.value()->payload_type, first_packet.value()->codec(), last_packet.value()->video_header.rotation, last_packet.value()->video_header.content_type);

  // 用 printf 输出 image_buffer 的前 128 字节，用十六进制输出，例如 AA BB CC 这样，每 32 字节一行
  printf("\t\t\t== Decoded Frame Data ==\n");
  for (size_t i = 0; i < 128 && i < image_buffer->size(); i++) {
    if (i % 32 == 0) {
      printf("\n\t\t\t");
    }
    printf("%02X ", image_buffer->data()[i]);
  }
  printf("\n\t\t\t ... ... ...");
  // 再输出 image_buffer 的后 128 字节，同上
  printf("\n\t\t\t");
  for (size_t i = image_buffer->size() - 128; i < image_buffer->size(); i++) {
    if (i % 32 == 0) {
      printf("\n\t\t\t");
    }
    printf("%02X ", image_buffer->data()[i]);
  }
  printf("\n");  

  // Create RtpFrameObject with complete RTP packet information from TamburDecoder
  const int max_nack_count = 0;     // No retransmission info available

  rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
  
  // Create RtpFrameObject and call OnAssembledFrame directly
  OnAssembledFrame(std::make_unique<RtpFrameObject>(
      first_packet.value()->seq_num(),                           // first_seq_num
      last_packet.value()->seq_num(),                            // last_seq_num
      last_packet.value()->marker_bit,                           // marker_bit
      max_nack_count,                                            // max_nack_count
      frame_info.min_receive_time,                               // min_recv_time
      frame_info.max_receive_time,                               // max_recv_time
      first_packet.value()->timestamp,                           // timestamp
      ntp_estimator_.Estimate(first_packet.value()->timestamp),  // ntp_time
      last_packet.value()->video_header.video_timing,            // video_timing
      first_packet.value()->payload_type,                        // payload_type
      first_packet.value()->codec(),                             // codec
      last_packet.value()->video_header.rotation,                // rotation
      last_packet.value()->video_header.content_type,            // content_type
      video_header,                                              // video_header (fixed)
      last_packet.value()->video_header.color_space,             // color_space
      last_packet.value()->video_header.frame_instrumentation_data,  // frame_instrumentation_data
      RtpPacketInfos(),                                          // packet_infos
      std::move(image_buffer)));                                 // bitstream

  RTC_LOG(LS_INFO) << "Successfully processed Tambur decoded frame through OnAssembledFrame";

}



void RtpVideoStreamReceiver2::OnRecoveredPacket(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (packet.PayloadType() == red_payload_type_) {
    RTC_LOG(LS_WARNING) << "Discarding recovered packet with RED encapsulation";
    return;
  }

  //Write file!
  if (packet.recovered()){
    auto nowtime = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(nowtime.time_since_epoch()).count();
    std::fstream rtp_video_recv_file(inputV::Params::output+"rtp_video_stream_receiver2.csv",std::ios::app);
    rtp_video_recv_file<<milliseconds_since_epoch<<","<<packet.SequenceNumber()<<","<<packet.payload_size()<<",";
    rtp_video_recv_file<<"RecoverVideoPacket"<<"\n";
    rtp_video_recv_file.close();
  }

  if (packet.recovered()){
    // std::cout<<"Record Recover Packet"<<std::endl;
    fec_receive_statistics_->OnRecoverPacket(packet);
  }

  ReceivePacket(packet);
}

// This method handles both regular RTP packets and packets recovered
// via FlexFEC.
void RtpVideoStreamReceiver2::OnRtpPacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  if (packet.payload()[0] == fec_receiver_->fec_payload_type()){
    // std::cout<<"Record FEC Packet"<<std::endl;
    fec_receive_statistics_->OnFECPacket(packet);
  }
  else if (packet.recovered()){
    // std::cout<<"Record Retransmit Packet"<<std::endl;
    fec_receive_statistics_->OnRetransmitPacket(packet);
  }
  else {
    // std::cout<<"Record Video Packet"<<std::endl;
    fec_receive_statistics_->OnVideoPacket(packet);
  }

  //Write file!
  if (packet.payload()[0] == fec_receiver_->fec_payload_type()){
    auto nowtime = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(nowtime.time_since_epoch()).count();
    std::fstream rtp_video_recv_file(inputV::Params::output+"rtp_video_stream_receiver2.csv",std::ios::app);
    rtp_video_recv_file<<milliseconds_since_epoch<<","<<packet.SequenceNumber()<<","<<packet.payload_size()<<",";
    rtp_video_recv_file<<"FECPacket"<<"\n";
    rtp_video_recv_file.close();
  }
  else if (packet.recovered()){
    auto nowtime = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(nowtime.time_since_epoch()).count();
    std::fstream rtp_video_recv_file(inputV::Params::output+"rtp_video_stream_receiver2.csv",std::ios::app);
    rtp_video_recv_file<<milliseconds_since_epoch<<","<<packet.SequenceNumber()<<","<<packet.payload_size()<<",";
    rtp_video_recv_file<<"RetransmissionPacket"<<"\n";
    rtp_video_recv_file.close();
  }
  else {
    auto nowtime = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(nowtime.time_since_epoch()).count();
    std::fstream rtp_video_recv_file(inputV::Params::output+"rtp_video_stream_receiver2.csv",std::ios::app);
    rtp_video_recv_file<<milliseconds_since_epoch<<","<<packet.SequenceNumber()<<","<<packet.payload_size()<<",";
    rtp_video_recv_file<<"VideoPacket"<<"\n";
    rtp_video_recv_file.close();
  }

  // std::cout<<"\t\tWe receive new packet! seq_num:" <<packet.SequenceNumber() <<std::endl;
  // if (!packet.recovered()) {
  //   std::cout<<"This is a source packet!"<<std::endl;
  // }
  // else {
  //   std::cout<<"This is a recover packet!"<<std::endl;
  // }

  if (!receiving_)
    return;

  ReceivePacket(packet);

  // Update receive statistics after ReceivePacket.
  // Receive statistics will be reset if the payload type changes (make sure
  // that the first packet is included in the stats).
  if (!packet.recovered()) {
    rtp_receive_statistics_->OnRtpPacket(packet);
  }

  if (packet_sink_) {
    packet_sink_->OnRtpPacket(packet);
  }
}

void RtpVideoStreamReceiver2::RequestKeyFrame() {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  // TODO(bugs.webrtc.org/10336): Allow the sender to ignore key frame requests
  // issued by anything other than the LossNotificationController if it (the
  // sender) is relying on LNTF alone.
  if (keyframe_request_method_ == KeyFrameReqMethod::kPliRtcp) {
    rtp_rtcp_->SendPictureLossIndication();
  } else if (keyframe_request_method_ == KeyFrameReqMethod::kFirRtcp) {
    rtp_rtcp_->SendFullIntraRequest();
  }
}

void RtpVideoStreamReceiver2::SendNack(
    const std::vector<uint16_t>& sequence_numbers,
    bool /*buffering_allowed*/) {
  rtp_rtcp_->SendNack(sequence_numbers);
}

void RtpVideoStreamReceiver2::SendLossNotification(
    uint16_t last_decoded_seq_num,
    uint16_t last_received_seq_num,
    bool decodability_flag,
    bool buffering_allowed) {
  RTC_DCHECK(config_.rtp.lntf.enabled);
  rtp_rtcp_->SendLossNotification(last_decoded_seq_num, last_received_seq_num,
                                  decodability_flag, buffering_allowed);
}

bool RtpVideoStreamReceiver2::IsDecryptable() const {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  return frames_decryptable_;
}

void RtpVideoStreamReceiver2::OnInsertedPacket(
    video_coding::PacketBuffer::InsertResult result) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  video_coding::PacketBuffer::Packet* first_packet = nullptr;
  int max_nack_count;
  int64_t min_recv_time;
  int64_t max_recv_time;
  std::vector<rtc::ArrayView<const uint8_t>> payloads;
  RtpPacketInfos::vector_type packet_infos;

  bool frame_boundary = true;
  for (auto& packet : result.packets) {
    // PacketBuffer promisses frame boundaries are correctly set on each
    // packet. Document that assumption with the DCHECKs.
    RTC_DCHECK_EQ(frame_boundary, packet->is_first_packet_in_frame());
    int64_t unwrapped_rtp_seq_num = packet->sequence_number;
    RTC_DCHECK_GT(packet_infos_.count(unwrapped_rtp_seq_num), 0);
    RtpPacketInfo& packet_info = packet_infos_[unwrapped_rtp_seq_num];
    if (packet->is_first_packet_in_frame()) {
      first_packet = packet.get();
      max_nack_count = packet->times_nacked;
      min_recv_time = packet_info.receive_time().ms();
      max_recv_time = packet_info.receive_time().ms();
    } else {
      max_nack_count = std::max(max_nack_count, packet->times_nacked);
      min_recv_time = std::min(min_recv_time, packet_info.receive_time().ms());
      max_recv_time = std::max(max_recv_time, packet_info.receive_time().ms());
    }
    payloads.emplace_back(packet->video_payload);
    packet_infos.push_back(packet_info);

    frame_boundary = packet->is_last_packet_in_frame();
    if (packet->is_last_packet_in_frame()) {
      auto depacketizer_it = payload_type_map_.find(first_packet->payload_type);
      RTC_CHECK(depacketizer_it != payload_type_map_.end());
      RTC_CHECK(depacketizer_it->second);

      rtc::scoped_refptr<EncodedImageBuffer> bitstream =
          depacketizer_it->second->AssembleFrame(payloads);
      if (!bitstream) {
        // Failed to assemble a frame. Discard and continue.
        continue;
      }

      const video_coding::PacketBuffer::Packet& last_packet = *packet;

      printf("\t\t\t== Receive video header: width:%d\theight:%d\n", first_packet->video_header.width, first_packet->video_header.height);
      printf("\t\t\t== payload size: %zu", bitstream->size());
      printf("\t\t\t== first_packet seq_num:%d\t last_packet seq_num:%d\n", first_packet->seq_num(), last_packet.seq_num());
      printf("\t\t\t== max_nack_count:%d\t min_recv_time:%ld\t max_recv_time:%ld\n", max_nack_count, min_recv_time, max_recv_time);
      printf("\t\t\t== payload_type:%d\t codec:%d\t rotation:%d\t content_type:%d\n", first_packet->payload_type, first_packet->codec(), last_packet.video_header.rotation, last_packet.video_header.content_type);
      
      // 用 printf 输出 bitstream 的前 128 字节，用十六进制输出，例如 AA BB CC 这样，每 32 字节一行
      printf("\t\t\t== Decoded Frame Data ==\n");
      for (size_t i = 0; i < 128 && i < bitstream->size(); i++) {
        if (i % 32 == 0) {
          printf("\n\t\t\t");
        }
        printf("%02X ", bitstream->data()[i]);
      }
      printf("\n\t\t\t ... ... ...");
      // 再输出 bitstream 的后 128 字节，同上
      printf("\n\t\t\t");
      for (size_t i = bitstream->size() - 128; i < bitstream->size(); i++) {
        if (i % 32 == 0) {
          printf("\n\t\t\t");
        }
        printf("%02X ", bitstream->data()[i]);
      }
      printf("\n");  

      OnAssembledFrame(std::make_unique<RtpFrameObject>(
          first_packet->seq_num(),                              //
          last_packet.seq_num(),                                //
          last_packet.marker_bit,                               //
          max_nack_count,                                       //
          min_recv_time,                                        //
          max_recv_time,                                        //
          first_packet->timestamp,                              //
          ntp_estimator_.Estimate(first_packet->timestamp),     //
          last_packet.video_header.video_timing,                //
          first_packet->payload_type,                           //
          first_packet->codec(),                                //
          last_packet.video_header.rotation,                    //
          last_packet.video_header.content_type,                //
          first_packet->video_header,                           //
          last_packet.video_header.color_space,                 //
          last_packet.video_header.frame_instrumentation_data,  //
          RtpPacketInfos(std::move(packet_infos)),              //
          std::move(bitstream)));
      payloads.clear();
      packet_infos.clear();
    }
  }
  RTC_DCHECK(frame_boundary);
  if (result.buffer_cleared) {
    last_received_rtp_system_time_.reset();
    last_received_keyframe_rtp_system_time_.reset();
    last_received_keyframe_rtp_timestamp_.reset();
    packet_infos_.clear();
    RequestKeyFrame();
  }
}

void RtpVideoStreamReceiver2::OnAssembledFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK(frame);

  const std::optional<RTPVideoHeader::GenericDescriptorInfo>& descriptor =
      frame->GetRtpVideoHeader().generic;

  if (loss_notification_controller_ && descriptor) {
    loss_notification_controller_->OnAssembledFrame(
        frame->first_seq_num(), descriptor->frame_id,
        absl::c_linear_search(descriptor->decode_target_indications,
                              DecodeTargetIndication::kDiscardable),
        descriptor->dependencies);
  }

  // If frames arrive before a key frame, they would not be decodable.
  // In that case, request a key frame ASAP.
  if (!has_received_frame_) {
    if (frame->FrameType() != VideoFrameType::kVideoFrameKey) {
      // `loss_notification_controller_`, if present, would have already
      // requested a key frame when the first packet for the non-key frame
      // had arrived, so no need to replicate the request.
      if (!loss_notification_controller_) {
        RequestKeyFrame();
      }
    }
    has_received_frame_ = true;
  }

  // Reset `reference_finder_` if `frame` is new and the codec have changed.
  if (current_codec_) {
    bool frame_is_newer =
        AheadOf(frame->RtpTimestamp(), last_assembled_frame_rtp_timestamp_);

    if (frame->codec_type() != current_codec_) {
      if (frame_is_newer) {
        // When we reset the `reference_finder_` we don't want new picture ids
        // to overlap with old picture ids. To ensure that doesn't happen we
        // start from the `last_completed_picture_id_` and add an offset in case
        // of reordering.
        reference_finder_ = std::make_unique<RtpFrameReferenceFinder>(
            last_completed_picture_id_ + std::numeric_limits<uint16_t>::max());
        current_codec_ = frame->codec_type();
      } else {
        // Old frame from before the codec switch, discard it.
        return;
      }
    }

    if (frame_is_newer) {
      last_assembled_frame_rtp_timestamp_ = frame->RtpTimestamp();
    }
  } else {
    current_codec_ = frame->codec_type();
    last_assembled_frame_rtp_timestamp_ = frame->RtpTimestamp();
  }

  if (buffered_frame_decryptor_ != nullptr) {
    buffered_frame_decryptor_->ManageEncryptedFrame(std::move(frame));
  } else if (frame_transformer_delegate_) {
    frame_transformer_delegate_->TransformFrame(std::move(frame));
  } else {
    OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
  }
}

void RtpVideoStreamReceiver2::OnCompleteFrames(
    RtpFrameReferenceFinder::ReturnVector frames) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  for (auto& frame : frames) {
    last_seq_num_for_pic_id_[frame->Id()] = frame->last_seq_num();

    last_completed_picture_id_ =
        std::max(last_completed_picture_id_, frame->Id());
    complete_frame_callback_->OnCompleteFrame(std::move(frame));
  }
}

void RtpVideoStreamReceiver2::OnDecryptedFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
}

void RtpVideoStreamReceiver2::OnDecryptionStatusChange(
    FrameDecryptorInterface::Status status) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  // Called from BufferedFrameDecryptor::DecryptFrame.
  frames_decryptable_ =
      (status == FrameDecryptorInterface::Status::kOk) ||
      (status == FrameDecryptorInterface::Status::kRecoverable);
}

void RtpVideoStreamReceiver2::SetFrameDecryptor(
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) {
  // TODO(bugs.webrtc.org/11993): Update callers or post the operation over to
  // the network thread.
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (buffered_frame_decryptor_ == nullptr) {
    buffered_frame_decryptor_ = std::make_unique<BufferedFrameDecryptor>(
        this, this, env_.field_trials());
  }
  buffered_frame_decryptor_->SetFrameDecryptor(std::move(frame_decryptor));
}

void RtpVideoStreamReceiver2::SetDepacketizerToDecoderFrameTransformer(
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  frame_transformer_delegate_ =
      rtc::make_ref_counted<RtpVideoStreamReceiverFrameTransformerDelegate>(
          this, &env_.clock(), std::move(frame_transformer),
          rtc::Thread::Current(), config_.rtp.remote_ssrc);
  frame_transformer_delegate_->Init();
}

void RtpVideoStreamReceiver2::UpdateRtt(int64_t max_rtt_ms) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (nack_module_)
    nack_module_->UpdateRtt(max_rtt_ms);
}

void RtpVideoStreamReceiver2::OnLocalSsrcChange(uint32_t local_ssrc) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  rtp_rtcp_->SetLocalSsrc(local_ssrc);
}

void RtpVideoStreamReceiver2::SetRtcpMode(RtcpMode mode) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  rtp_rtcp_->SetRTCPStatus(mode);
}

void RtpVideoStreamReceiver2::SetReferenceTimeReport(bool enabled) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  rtp_rtcp_->SetNonSenderRttMeasurement(enabled);
}

void RtpVideoStreamReceiver2::SetPacketSink(
    RtpPacketSinkInterface* packet_sink) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  packet_sink_ = packet_sink;
}

void RtpVideoStreamReceiver2::SetLossNotificationEnabled(bool enabled) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (enabled && !loss_notification_controller_) {
    loss_notification_controller_ =
        std::make_unique<LossNotificationController>(&rtcp_feedback_buffer_,
                                                     &rtcp_feedback_buffer_);
  } else if (!enabled && loss_notification_controller_) {
    loss_notification_controller_.reset();
    rtcp_feedback_buffer_.ClearLossNotificationState();
  }
}

void RtpVideoStreamReceiver2::SetNackHistory(TimeDelta history) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (history.ms() == 0) {
    nack_module_.reset();
  } else if (!nack_module_) {
    nack_module_ = std::make_unique<NackRequester>(
        worker_queue_, nack_periodic_processor_, &env_.clock(),
        &rtcp_feedback_buffer_, &rtcp_feedback_buffer_, env_.field_trials());
  }

  rtp_receive_statistics_->SetMaxReorderingThreshold(
      config_.rtp.remote_ssrc,
      history.ms() > 0 ? kMaxPacketAgeToNack : kDefaultMaxReorderingThreshold);
}

int RtpVideoStreamReceiver2::ulpfec_payload_type() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return fec_receiver_ ? fec_receiver_->fec_payload_type() : -1;
}

int RtpVideoStreamReceiver2::red_payload_type() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return red_payload_type_;
}

void RtpVideoStreamReceiver2::SetProtectionPayloadTypes(
    int red_payload_type,
    int fec_payload_type) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK(red_payload_type >= -1 && red_payload_type < 0x80);
  RTC_DCHECK(fec_payload_type >= -1 && fec_payload_type < 0x80);
  red_payload_type_ = red_payload_type;
  fec_receiver_ =
      MaybeConstructfecReceiver(config_.rtp.remote_ssrc, red_payload_type,
                                   fec_payload_type, this, &env_.clock());
}

std::optional<int64_t> RtpVideoStreamReceiver2::LastReceivedPacketMs() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (last_received_rtp_system_time_) {
    return std::optional<int64_t>(last_received_rtp_system_time_->ms());
  }
  return std::nullopt;
}

std::optional<uint32_t> RtpVideoStreamReceiver2::LastReceivedFrameRtpTimestamp()
    const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return last_received_rtp_timestamp_;
}

std::optional<int64_t> RtpVideoStreamReceiver2::LastReceivedKeyframePacketMs()
    const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (last_received_keyframe_rtp_system_time_) {
    return std::optional<int64_t>(
        last_received_keyframe_rtp_system_time_->ms());
  }
  return std::nullopt;
}

std::optional<RtpRtcpInterface::SenderReportStats>
RtpVideoStreamReceiver2::GetSenderReportStats() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return rtp_rtcp_->GetSenderReportStats();
}

void RtpVideoStreamReceiver2::ManageFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
}

void RtpVideoStreamReceiver2::ReceivePacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  if (packet.payload_size() == 0) {
    // Padding or keep-alive packet.
    // TODO(nisse): Could drop empty packets earlier, but need to figure out how
    // they should be counted in stats.
    NotifyReceiverOfEmptyPacket(packet.SequenceNumber());
    return;
  }
  if (packet.PayloadType() == red_payload_type_) {
    ParseAndHandleEncapsulatingHeader(packet);
    return;
  }

  const auto type_it = payload_type_map_.find(packet.PayloadType());
  if (type_it == payload_type_map_.end()) {
    return;
  }

  auto parse_and_insert = [&](const RtpPacketReceived& packet) {
    RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
    
    // TODO: 我认为这一段不需要
    // // Check if this is a Tambur FEC packet and handle it in OnReceivedPayloadData
    // if (inputV::Params::type == inputV::ExpType::TamburFEC) {
    //   // For Tambur FEC packets, create a minimal video header and pass to OnReceivedPayloadData
    //   RTPVideoHeader video_header;
    //   video_header.frame_type = VideoFrameType::kVideoFrameDelta;
    //   video_header.codec = VideoCodecType::kVideoCodecGeneric;
      
    //   int times_nacked = nack_module_
    //                          ? nack_module_->OnReceivedPacket(
    //                                packet.SequenceNumber(), packet.recovered())
    //                          : -1;

    //   return OnReceivedPayloadData(packet.PayloadBuffer(),
    //                                packet, video_header,
    //                                times_nacked);
    // }
    
    std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload =
        type_it->second->Parse(packet.PayloadBuffer());
    if (parsed_payload == std::nullopt) {
      RTC_LOG(LS_WARNING) << "Failed parsing payload.";
      return false;
    }

    int times_nacked = nack_module_
                           ? nack_module_->OnReceivedPacket(
                                 packet.SequenceNumber(), packet.recovered())
                           : -1;

    return OnReceivedPayloadData(std::move(parsed_payload->video_payload),
                                 packet, parsed_payload->video_header,
                                 times_nacked);
  };

  // When the dependency descriptor is used and the descriptor fail to parse
  // then `OnReceivedPayloadData` may return true to signal the the packet
  // should be retried at a later stage, which is why they are stashed here.
  //
  // TODO(bugs.webrtc.org/15782):
  // This is an ugly solution. The way things should work is for the
  // `RtpFrameReferenceFinder` to stash assembled frames until the keyframe with
  // the relevant template structure has been received, but unfortunately the
  // `frame_transformer_delegate_` is called before the frames are inserted into
  // the `RtpFrameReferenceFinder`, and it expects the dependency descriptor to
  // be parsed at that stage.
  if (parse_and_insert(packet)) {
    if (stashed_packets_.size() == 100) {
      stashed_packets_.clear();
    }
    stashed_packets_.push_back(packet);
  } else {
    for (auto it = stashed_packets_.begin(); it != stashed_packets_.end();) {
      if (parse_and_insert(*it)) {
        ++it;  // keep in the stash.
      } else {
        it = stashed_packets_.erase(it);
      }
    }
  }
}

void RtpVideoStreamReceiver2::ParseAndHandleEncapsulatingHeader(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK_EQ(packet.PayloadType(), red_payload_type_);

  if (!fec_receiver_ || packet.payload_size() == 0U)
    return;

  if (packet.payload()[0] == fec_receiver_->fec_payload_type()) {
    // Notify video_receiver about received FEC packets to avoid NACKing these
    // packets.
    NotifyReceiverOfEmptyPacket(packet.SequenceNumber());
  }
  if (fec_receiver_->AddReceivedRedPacket(packet)) {
    fec_receiver_->ProcessReceivedFec();
  }
}

// In the case of a video stream without picture ids and no rtx the
// RtpFrameReferenceFinder will need to know about padding to
// correctly calculate frame references.
void RtpVideoStreamReceiver2::NotifyReceiverOfEmptyPacket(uint16_t seq_num) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK_RUN_ON(&worker_task_checker_);

  OnCompleteFrames(reference_finder_->PaddingReceived(seq_num));

  OnInsertedPacket(packet_buffer_.InsertPadding(seq_num));
  if (nack_module_) {
    nack_module_->OnReceivedPacket(seq_num, /*is_recovered=*/false);
  }
  if (loss_notification_controller_) {
    // TODO(bugs.webrtc.org/10336): Handle empty packets.
    RTC_LOG(LS_WARNING)
        << "LossNotificationController does not expect empty packets.";
  }
}

bool RtpVideoStreamReceiver2::DeliverRtcp(const uint8_t* rtcp_packet,
                                          size_t rtcp_packet_length) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

  if (!receiving_) {
    return false;
  }

  rtp_rtcp_->IncomingRtcpPacket(
      rtc::MakeArrayView(rtcp_packet, rtcp_packet_length));

  std::optional<TimeDelta> rtt = rtp_rtcp_->LastRtt();
  if (!rtt.has_value()) {
    // Waiting for valid rtt.
    return true;
  }

  std::optional<RtpRtcpInterface::SenderReportStats> last_sr =
      rtp_rtcp_->GetSenderReportStats();
  if (!last_sr.has_value()) {
    // Waiting for RTCP.
    return true;
  }
  int64_t time_since_received = env_.clock().CurrentNtpInMilliseconds() -
                                last_sr->last_arrival_ntp_timestamp.ToMs();
  // Don't use old SRs to estimate time.
  if (time_since_received <= 1) {
    ntp_estimator_.UpdateRtcpTimestamp(*rtt, last_sr->last_remote_ntp_timestamp,
                                       last_sr->last_remote_rtp_timestamp);
    std::optional<int64_t> remote_to_local_clock_offset =
        ntp_estimator_.EstimateRemoteToLocalClockOffset();
    if (remote_to_local_clock_offset.has_value()) {
      capture_clock_offset_updater_.SetRemoteToLocalClockOffset(
          *remote_to_local_clock_offset);
    }
  }

  return true;
}

void RtpVideoStreamReceiver2::FrameContinuous(int64_t picture_id) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (!nack_module_)
    return;

  int seq_num = -1;
  auto seq_num_it = last_seq_num_for_pic_id_.find(picture_id);
  if (seq_num_it != last_seq_num_for_pic_id_.end())
    seq_num = seq_num_it->second;
  if (seq_num != -1)
    nack_module_->ClearUpTo(seq_num);
}

void RtpVideoStreamReceiver2::FrameDecoded(int64_t picture_id) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  int seq_num = -1;
  auto seq_num_it = last_seq_num_for_pic_id_.find(picture_id);
  if (seq_num_it != last_seq_num_for_pic_id_.end()) {
    seq_num = seq_num_it->second;
    last_seq_num_for_pic_id_.erase(last_seq_num_for_pic_id_.begin(),
                                   ++seq_num_it);
  }

  if (seq_num != -1) {
    int64_t unwrapped_rtp_seq_num = rtp_seq_num_unwrapper_.Unwrap(seq_num);
    packet_infos_.erase(packet_infos_.begin(),
                        packet_infos_.upper_bound(unwrapped_rtp_seq_num));
    packet_buffer_.ClearTo(seq_num);
    reference_finder_->ClearTo(seq_num);
  }
}

void RtpVideoStreamReceiver2::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  rtp_rtcp_->SetRTCPStatus(state == kNetworkUp ? config_.rtp.rtcp_mode
                                               : RtcpMode::kOff);
}

void RtpVideoStreamReceiver2::StartReceive() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  // |h26x_packet_buffer_| is created here instead of in the ctor because we
  // need to know the value of |sps_pps_id_is_h264_keyframe_|.
  if (env_.field_trials().IsEnabled("WebRTC-Video-H26xPacketBuffer") &&
      !h26x_packet_buffer_) {
    h26x_packet_buffer_ =
        std::make_unique<H26xPacketBuffer>(!sps_pps_idr_is_h264_keyframe_);
  }
  if (!receiving_ && packet_router_) {
    // Change REMB candidate egibility.
    packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
    packet_router_->AddReceiveRtpModule(rtp_rtcp_.get(),
                                        /*remb_candidate=*/true);
  }
  receiving_ = true;
}

void RtpVideoStreamReceiver2::StopReceive() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (receiving_ && packet_router_) {
    // Change REMB candidate egibility.
    packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
    packet_router_->AddReceiveRtpModule(rtp_rtcp_.get(),
                                        /*remb_candidate=*/false);
  }
  receiving_ = false;
}

void RtpVideoStreamReceiver2::InsertSpsPpsIntoTracker(uint8_t payload_type) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK_RUN_ON(&worker_task_checker_);

  auto codec_params_it = pt_codec_params_.find(payload_type);
  if (codec_params_it == pt_codec_params_.end())
    return;

  RTC_LOG(LS_INFO) << "Found out of band supplied codec parameters for"
                      " payload type: "
                   << static_cast<int>(payload_type);

  H264SpropParameterSets sprop_decoder;
  auto sprop_base64_it =
      codec_params_it->second.find(cricket::kH264FmtpSpropParameterSets);

  if (sprop_base64_it == codec_params_it->second.end())
    return;

  if (!sprop_decoder.DecodeSprop(sprop_base64_it->second.c_str()))
    return;

  tracker_.InsertSpsPpsNalus(sprop_decoder.sps_nalu(),
                             sprop_decoder.pps_nalu());

  if (h26x_packet_buffer_) {
    h26x_packet_buffer_->SetSpropParameterSets(sprop_base64_it->second);
  }
}

void RtpVideoStreamReceiver2::UpdatePacketReceiveTimestamps(
    const RtpPacketReceived& packet,
    bool is_keyframe) {
  Timestamp now = env_.clock().CurrentTime();
  if (is_keyframe ||
      last_received_keyframe_rtp_timestamp_ == packet.Timestamp()) {
    last_received_keyframe_rtp_timestamp_ = packet.Timestamp();
    last_received_keyframe_rtp_system_time_ = now;
  }
  last_received_rtp_system_time_ = now;
  last_received_rtp_timestamp_ = packet.Timestamp();

  // Periodically log the RTP header of incoming packets.
  if (now.ms() - last_packet_log_ms_ > kPacketLogIntervalMs) {
    rtc::StringBuilder ss;
    ss << "Packet received on SSRC: " << packet.Ssrc()
       << " with payload type: " << static_cast<int>(packet.PayloadType())
       << ", timestamp: " << packet.Timestamp()
       << ", sequence number: " << packet.SequenceNumber()
       << ", arrival time: " << ToString(packet.arrival_time());
    int32_t time_offset;
    if (packet.GetExtension<TransmissionOffset>(&time_offset)) {
      ss << ", toffset: " << time_offset;
    }
    uint32_t send_time;
    if (packet.GetExtension<AbsoluteSendTime>(&send_time)) {
      ss << ", abs send time: " << send_time;
    }
    RTC_LOG(LS_INFO) << ss.str();
    last_packet_log_ms_ = now.ms();
  }
}

}  // namespace webrtc
