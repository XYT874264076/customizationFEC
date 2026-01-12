
#ifndef EXAMPLES_MYFECEXP_FEC_RECEIVER_H_
#define EXAMPLES_MYFECEXP_FEC_RECEIVER_H_

#include <stddef.h>
#include <stdint.h>

#include "system_wrappers/include/clock.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc{

struct FecPacketCounter {
  FecPacketCounter() = default;
  size_t num_packets = 0;  // Number of received packets.
  size_t num_bytes = 0;
  size_t num_fec_packets = 0;  // Number of received FEC packets.
  size_t num_recovered_packets =
      0;  // Number of recovered media packets using FEC.
  // Time when first packet is received.
  Timestamp first_packet_time = Timestamp::MinusInfinity();
};

class fecReceiver {
 public:
  fecReceiver() = default;
  virtual ~fecReceiver() = default;

  virtual int fec_payload_type() const = 0;
  virtual bool AddReceivedRedPacket(const RtpPacketReceived& rtp_packet) = 0;
  virtual void ProcessReceivedFec() = 0;
  virtual FecPacketCounter GetPacketCounter() const = 0;
};

}


#endif  // EXAMPLES_MYFECEXP_FEC_RECEIVER_H_