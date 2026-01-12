#include "examples/MyFECExp/peer_connection_client.h"

#include <iostream>

#include "api/units/time_delta.h"
#include "rtc_base/async_dns_resolver.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/thread.h"

namespace {

    // This is our magical hangup signal.
    constexpr char kByeMessage[] = "BYE";
    // Delay between server connection retries, in milliseconds
    constexpr webrtc::TimeDelta kReconnectDelay = webrtc::TimeDelta::Seconds(2);

    rtc::Socket* CreateClientSocket(int family) {
        rtc::Thread* thread = rtc::Thread::Current();
        RTC_DCHECK(thread != NULL);
        return thread->socketserver()->CreateSocket(family, SOCK_STREAM);
    }

}


PeerConnectionClient::PeerConnectionClient() : callback_(NULL), state_(NOT_CONNECTED){}
PeerConnectionClient::~PeerConnectionClient() = default;

bool PeerConnectionClient::is_connected() const {
    return state_ != NOT_CONNECTED;
}

const Peers& PeerConnectionClient::peers() const {
    return peers_;
}


//Initialize
void PeerConnectionClient::InitSocketSignals() {
    RTC_DCHECK(control_socket_.get() != NULL);

    // Set the life cycle function for control_socket_
    control_socket_->SignalCloseEvent.connect(this, &PeerConnectionClient::OnClose);
    control_socket_->SignalConnectEvent.connect(this, &PeerConnectionClient::OnConnect);
    control_socket_->SignalReadEvent.connect(this, &PeerConnectionClient::OnRead);
}


// Implement the life sycle function for control_socket_
void PeerConnectionClient::OnConnect(rtc::Socket* socket) {
    RTC_DCHECK(!signal_data_.empty());
    size_t sent = socket->Send(signal_data_.c_str(), signal_data_.length());
    RTC_DCHECK(sent == signal_data_.length());
    signal_data_.clear();
}

void PeerConnectionClient::OnRead(rtc::Socket* socket) {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  if (ReadIntoBuffer(socket, &control_data_)) {
    if (state_ == NOT_CONNECTED && control_data_.find("/joined",0)==0){
        printf("Joined Room Successfully! Now wait for other client!\n");
        control_data_=control_data_.substr(8);
        state_ = SIGNED_IN;
    }
    if (state_ == SIGNED_IN && control_data_.find("/other_join",0)==0){
        printf("Other client joined! Now let PeerConnection connect to Peer!\n");
        control_data_=control_data_.substr(12);
        callback_->OnOtherJoined();
        state_ = CONNECTTOPEER;
    }
    if (state_ != NOT_CONNECTED && control_data_.find("/message",0)==0){
        printf("Receive message from peer!\n");
        control_data_=control_data_.substr(9);
        OnMessageFromPeer(control_data_);
        state_ = CONNECTTOPEER;
        control_data_.clear();
    }
    if (state_ != NOT_CONNECTED && control_data_.find("/leaved",0)==0){
        printf("Leaved Room Successfully!\n");
        control_data_=control_data_.substr(8);
        callback_->OnDisconnected();
        state_ = NOT_CONNECTED;
    }
    if (control_data_.find("/wait",0)==0){
        // Receive wait, just do nothing!
        control_data_=control_data_.substr(6);
    }
    
    control_data_.clear();
  }
}

void PeerConnectionClient::OnClose(rtc::Socket* socket, int err) {
  RTC_LOG(LS_INFO) << __FUNCTION__;

  socket->Close();

#ifdef WIN32
  if (err != WSAECONNREFUSED) {
#else
  if (err != ECONNREFUSED) {
#endif
    callback_->OnMessageSent(err);
  } else {
    if (socket == control_socket_.get()) {
      RTC_LOG(LS_WARNING) << "Connection refused; retrying in 2 seconds";
      rtc::Thread::Current()->PostDelayedTask(SafeTask(safety_.flag(), [this] { DoConnect(); }), kReconnectDelay);
    } else {
      Close();
      callback_->OnDisconnected();
    }
  }
}


//API
void PeerConnectionClient::RegisterObserver(PeerConnectionClientObserver* callback) {
  RTC_DCHECK(!callback_);
  callback_ = callback;
}

void PeerConnectionClient::Connect(const std::string& server, int port) {
  RTC_DCHECK(!server.empty());

  if (state_ != NOT_CONNECTED) {
    RTC_LOG(LS_WARNING) << "The client must not be connected before you can call Connect()";
    callback_->OnServerConnectionFailure();
    return;
  }

  if (server.empty()) {
    callback_->OnServerConnectionFailure();
    return;
  }

  if (port <= 0) port = kDefaultServerPort;

  server_address_.SetIP(server);
  server_address_.SetPort(port);
  DoConnect();
}

void PeerConnectionClient::DoConnect() {
  control_socket_.reset(CreateClientSocket(server_address_.ipaddr().family()));
  InitSocketSignals();
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "/join");
  signal_data_ = buffer;

  bool ret = ConnectControlSocket();
  if (!ret) {
    callback_->OnServerConnectionFailure();
  }
}

bool PeerConnectionClient::PCSendMessage(const std::string& message) {
  if (state_ != CONNECTTOPEER) return false;

  RTC_DCHECK(is_connected());

  char headers[1024];
  snprintf(headers, sizeof(headers),"/message ");
  message_data_ = headers;
  message_data_ += message;

  size_t sent = control_socket_->Send(message_data_.c_str(), message_data_.length());
  RTC_DCHECK(sent == message_data_.length());
  message_data_.clear();

  return true;
}

bool PeerConnectionClient::SendHangUp() {
  return PCSendMessage(kByeMessage);
}

bool PeerConnectionClient::SignOut() {
  if (state_ == NOT_CONNECTED) return true;

  if (control_socket_->GetState() == rtc::Socket::CS_CLOSED) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "/leave");
    notification_data_ = buffer;
    size_t sent = control_socket_->Send(notification_data_.c_str(), notification_data_.length());
    return sent==notification_data_.length();
  }

  state_ = NOT_CONNECTED;

  return true;
}

void PeerConnectionClient::Close() {
  control_socket_->Close();
  signal_data_.clear();
  message_data_.clear();
  notification_data_.clear();
  peers_.clear();
  state_ = NOT_CONNECTED;
}

bool PeerConnectionClient::ConnectControlSocket() {
  RTC_DCHECK(control_socket_->GetState() == rtc::Socket::CS_CLOSED);
  int err = control_socket_->Connect(server_address_);
  if (err == SOCKET_ERROR) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " " << "Connecting Error!!!";
    Close();
    return false;
  }
  return true;
}


//Utils
void PeerConnectionClient::OnMessageFromPeer(const std::string& message) {
  if (message.length() == (sizeof(kByeMessage) - 1) && message.compare(kByeMessage) == 0) {
    callback_->OnPeerDisconnected();
  } else {
    callback_->OnMessageFromPeer(message);
  }
}

bool PeerConnectionClient::ReadIntoBuffer(rtc::Socket* socket, std::string* data) {
  char buffer[0xffff];
  do {
    int bytes = socket->Recv(buffer, sizeof(buffer), nullptr);
    if (bytes <= 0) break;
    data->append(buffer, bytes);
  } while (true);
  if (data->length() > 0) return true;
  else return false;
}
