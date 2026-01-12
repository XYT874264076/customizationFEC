#ifndef EXAMPLES_CUSTOMIZATION_CONDUCTOR_H_
#define EXAMPLES_CUSTOMIZATION_CONDUCTOR_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"

#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "examples/customizationFEC/main_wnd.h"
#include "examples/customizationFEC/peer_connection_client.h"
#include "rtc_base/thread.h"
#include "rtc_base/platform_thread.h"

namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class Conductor : public webrtc::PeerConnectionObserver,
                  public webrtc::CreateSessionDescriptionObserver,
                  public PeerConnectionClientObserver,
                  public MainWndCallback {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    NEW_TRACK_ADDED,
    TRACK_REMOVED,
  };

  enum CState {
    NOT_CONNECTED,
    CONNECTTOPEER
  };
    
  Conductor(PeerConnectionClient* client, MainWindow* main_wnd);
  void SetFilePath(std::string file_path);

  bool connection_active() const;

  void Close() override;

  //
  // API called by main.cc
  //

  void StartLogin(const std::string& server, int port);
  void DisconnectFromServer();
  void ConnectToPeer();
  void DisconnectFromCurrentPeer();

  CState state_;
 
 protected:
  ~Conductor();
  bool InitializePeerConnection();
  bool CreatePeerConnection();
  void DeletePeerConnection();
  void AddTracks();

  //
  // Statistical network information
  //

  int32_t startGetState();
  int32_t stopGetState();
  int32_t startRLState();
  int32_t stopRLState();
  bool doGetState();
  bool doFECRLState();

  //
  // PeerConnectionObserver implementation.
  //

  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {}

  //
  // PeerConnectionClientObserver implementation.
  //

  void OnSignedIn() override;
  void OnDisconnected() override;
  void OnOtherJoined() override;
  void OnPeerConnected() override;
  void OnPeerDisconnected() override;
  void OnMessageFromPeer(const std::string& message) override;
  void OnMessageSent(int err) override;
  void OnServerConnectionFailure() override;

  //
  // MainWndCallback implementation.
  //

  void UIThreadCallback(int msg_id, void* data) override;

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

 protected:
  // Send a message to the remote peer.
  void SendMessage(const std::string& json_object);

  std::unique_ptr<rtc::Thread> signaling_thread_;
  webrtc::TaskQueueFactory* task_queue_factory_ = nullptr;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
  PeerConnectionClient* client_;
  MainWindow* main_wnd_;
  std::deque<std::string*> pending_messages_;
  std::string server_;
  std::string file_path_;
  std::chrono::steady_clock::time_point start_state_time;
  std::chrono::steady_clock::time_point last_state_time;
  bool state_time_initialize;
  std::chrono::steady_clock::time_point start_rl_time;
  std::chrono::steady_clock::time_point last_rl_time;
  bool rl_state_time_initialize;
  rtc::PlatformThread _getStateThread;
  rtc::PlatformThread _getRLStateThread;
};

#endif  // EXAMPLES_CUSTOMIZATION_CONDUCTOR_H_