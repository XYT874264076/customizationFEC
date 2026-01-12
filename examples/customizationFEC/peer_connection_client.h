#ifndef EXAMPLES_MYFECEXP_PEER_CONNECTION_CLIENT_H_
#define EXAMPLES_MYFECEXP_PEER_CONNECTION_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

typedef std::map<int, std::string> Peers;

struct PeerConnectionClientObserver {
  virtual void OnSignedIn() = 0;  // Called when we're logged on.
  virtual void OnDisconnected() = 0;
  virtual void OnOtherJoined() = 0;
  virtual void OnPeerConnected() = 0;
  virtual void OnPeerDisconnected() = 0;
  virtual void OnMessageFromPeer(const std::string& message) = 0;
  virtual void OnMessageSent(int err) = 0;
  virtual void OnServerConnectionFailure() = 0;

 protected:
  virtual ~PeerConnectionClientObserver() {}
};

class PeerConnectionClient : public sigslot::has_slots<>{
    
    public:
        enum State {
            NOT_CONNECTED,
            SIGNED_IN,
            CONNECTTOPEER
        };

        PeerConnectionClient();
        ~PeerConnectionClient();

        bool is_connected() const;
        const Peers& peers() const;

        void RegisterObserver(PeerConnectionClientObserver* callback);
        bool SignOut();


        void Connect(const std::string& server, int port);

        bool PCSendMessage(const std::string& message);
        bool SendHangUp();

    protected:
        void DoConnect();
        void Close();
        void InitSocketSignals();
        bool ConnectControlSocket();
        void OnConnect(rtc::Socket* socket);
        void OnHangingGetConnect(rtc::Socket* socket);
        void OnMessageFromPeer(const std::string& message);

        // Returns true if the whole response has been read.
        bool ReadIntoBuffer(rtc::Socket* socket, std::string* data);

        void OnRead(rtc::Socket* socket);
        void OnHangingGetRead(rtc::Socket* socket);

        void OnClose(rtc::Socket* socket, int err);

        PeerConnectionClientObserver* callback_;
        rtc::SocketAddress server_address_;
        std::unique_ptr<rtc::Socket> control_socket_;
        std::string signal_data_;
        std::string message_data_;
        std::string control_data_;
        std::string notification_data_;
        Peers peers_;
        State state_;
        webrtc::ScopedTaskSafety safety_;

        static const int kDefaultServerPort=3000;
};


#endif  // EXAMPLES_MYFECEXP_PEER_CONNECTION_CLIENT_H_