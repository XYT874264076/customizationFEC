
#ifndef EXAMPLES_MYFECEXP_RSFEC_RS_FORWARD_ERROR_CORRECTION_H_
#define EXAMPLES_MYFECEXP_RSFEC_RS_FORWARD_ERROR_CORRECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>
#include <bitset>

#include "absl/container/inlined_vector.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "examples/MyFECExp/RS_FEC/RS_forward_error_correction_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {

class RSFecHeaderReader;
class RSFecHeaderWriter;

class RSForwardErrorCorrection {
 public:
  class Packet{
    public:
      Packet();
      virtual ~Packet();
    
      // Add a reference.
      virtual int32_t AddRef();

      // Release a reference. Will delete the object if the reference count reaches zero.
      virtual int32_t Release();

      rtc::CopyOnWriteBuffer data;  // Packet data
    
    private:
      int32_t ref_count_;   // Counts the number of references to a packet.
  };

  class SortablePacket{
    public:
      // Functor which returns true if the sequence number of `first`
      // is < the sequence number of `second`. Should only ever be called for
      // packets belonging to the same SSRC.
      struct LessThan {
        template <typename S, typename T>
        bool operator()(const S& first, const T& second);
      };

      struct LessThanWeak {
        template <typename S, typename T>
        bool operator()(const S& first, const T& second);
      };

      uint32_t ssrc;
      uint16_t seq_num;
  };

  //Used for the input to DecodeRSFec()
  class ReceivedPacket : public SortablePacket {
    public:
      ReceivedPacket();
      ~ReceivedPacket();

      bool is_fec; // Set to true if this is an FEC packet and false otherwise.

      bool is_recovered;
      RtpHeaderExtensionMap extensions;
      rtc::scoped_refptr<Packet> pkt;   // Pointer to the packet storage.
  };

  // The recodered list parameter of DecodeRSFec() references structs of this type.
  class RecoveredPacket : public SortablePacket {
    public:
      RecoveredPacket();
      ~RecoveredPacket();

      bool was_recovered;   // Will be true if this packet was recoverec by RSFEC. Otherwise it was a media packet passed in through the received packet list.
      bool returned;        //True when the packet already has been returned to the caller through the callback.
      rtc::scoped_refptr<Packet> pkt;   // Pointer to the packet stroage.
  };

  // Used to link media packets to their protecting FEC packets.
  class ProtectedPacket : public SortablePacket {
    public:
      ProtectedPacket();
      ~ProtectedPacket();

      rtc::scoped_refptr<RSForwardErrorCorrection::Packet> pkt;
  };

  using ProtectedPacketList = std::list<std::unique_ptr<ProtectedPacket>>;

  //TODO: Maybe this will change later!
  struct ProtectedStream{
    uint32_t ssrc = 0;
    uint16_t seq_num_base = 0;
    uint16_t cur_base_num = 0;
    size_t packet_mask_offset = 0;  // Relative start of FEC header.
    std::bitset<32> mask = std::bitset<32>(0);
    std::bitset<32> arrive = std::bitset<32>(0);
    size_t packet_mask_size = 0;
  };

  class ConsiderLossPacket;
  using ConsiderLossPacketListShared = std::list<std::shared_ptr<ConsiderLossPacket>>;
  using ConsiderLossPacketListWeak = std::list<std::weak_ptr<ConsiderLossPacket>>;

  // Used for internal storage of received FEC packets in a list.
  class ReceivedFecPacket : public SortablePacket {
    public:
      static constexpr size_t kInlinedSsrcsVectorSize = 4;

      ReceivedFecPacket();
      ~ReceivedFecPacket();

      // List of media packets that this FEC packet protects.
      ProtectedPacketList protected_packets;
      // List of media packets that maybe loss just right now.
      ConsiderLossPacketListWeak maybe_loss_packets;
      // RTP header fields.
      uint32_t ssrc;
      // FEC header fields.
      size_t fec_header_size;
      absl::InlinedVector<ProtectedStream, kInlinedSsrcsVectorSize> protected_streams;
      size_t protection_length;
      // Raw data.
      rtc::scoped_refptr<RSForwardErrorCorrection::Packet> pkt;
      // Use for search decode opportunities!
      bool if_used;
  };

  using PacketList = std::list<std::unique_ptr<Packet>>;
  using RecoveredPacketList = std::list<std::unique_ptr<RecoveredPacket>>;
  using ReceivedFecPacketListShared = std::list<std::shared_ptr<ReceivedFecPacket>>;
  using ReceivedFecPacketListWeak = std::list<std::weak_ptr<ReceivedFecPacket>>;

  class ConsiderLossPacket : public SortablePacket {
    public:
      ConsiderLossPacket();
      ~ConsiderLossPacket();

      rtc::scoped_refptr<Packet> pkt;
      ReceivedFecPacketListWeak reference_fec_packets;
      // Use for search decode opportunities!
      bool if_used;
  };

  ~RSForwardErrorCorrection();

  // Creates a RSForwardErrorCorrection tailored for a specific FEC scheme.
  static std::unique_ptr<RSForwardErrorCorrection> CreateRSFec(uint32_t ssrc, Clock* clock);

  // Generates a list of RSFEC packets from supplied media packets.
  //
  // Input:  media_packets          List of media packets to protect, of type Packet. All packets are no need to belong to the
  //                                same frame but the list must not be empty.
  // Input:  protection_factor      FEC protection overhead in the [0, 255] domain. To obtain 100% overhead, or an
  //                                equal number of FEC packets as media packets, use 255.
  // Output: fec_packets            List of pointers to generated FEC packets,
  //                                of type Packet. Must be empty on entry.
  //                                The memory available through the list will
  //                                be valid until the next call to
  //                                EncodeRSFec().
  //
  // Returns 0 on success, -1 on failure.
  //
  int EncodeRSFec(const PacketList& media_packets,
                  uint8_t protection_factor,
                  std::list<Packet*>* fec_packets);

  // Get the number of generated FEC packets, maybe more infomation will be added later.
  struct DecodeFecResult{
    size_t num_recovered_packets = 0;
  };

  // Decodes a list of received media and FEC packets. It will parse the `received_packets`, storing FEC packets internally, and move
  // media packets to `recovered_packets`. The recovered list will be sorted by ascending sequence number and have duplicates removed.
  // The function should be called as new packets arrive, and `recovered_packets` will be progressively assembled with each call.
  // When the function returns, `received_packets` will be empty.
  //
  // The caller will allocate packets submitted through `received_packets`.
  // The function will handle allocation of recovered packets.
  //
  // Input:  received_packets   List of new received packets, of type ReceivedPacket, belonging to a single
  //                            frame. At output the list will be empty, with packets either stored internally,
  //                            or accessible through the recovered list.
  // Output: recovered_packets  List of recovered media packets, of type RecoveredPacket, belonging to a single
  //                            frame. The memory available through thelist will be valid until the next call to
  //                            DecodeRSFec().
  DecodeFecResult DecodeRSFec(const ReceivedPacket& received_packet, RecoveredPacketList* recovered_packets);

  // Gets the number of generated FEC packets, given the number of media packets and the protection factor.
  static int NumFecPackets(int num_media_packets, int protection_factor);

  // Gets the maximum size of the FEC headers in bytes, which must be accounted for as packet overhead.
  size_t MaxPacketOverhead() const;

  // Reset internal states from last frame and clear `recovered_packets`. Frees all memory allocated by this class.
  void ResetState(RecoveredPacketList* recovered_packets);
  
  static uint16_t ParseSequenceNumber(const uint8_t* packet);
  static uint32_t ParseSsrc(const uint8_t* packet);

 protected:
  RSForwardErrorCorrection(std::unique_ptr<RSFecHeaderReader> fec_header_reader,
                           std::unique_ptr<RSFecHeaderWriter> fec_header_writer,
                           uint32_t ssrc,
                           uint32_t protected_media_ssrc,
                           Clock* clock);
 private:
  // Writes the FEC payloads and some recovery fields in the FEC headers.
  void GenerateFecPayloads(const PacketList& media_packets,size_t n,size_t k,uint8_t* encoding_table);

  // Writes the FEC header fields that are not written by GenerateFecPayloeds.
  // This includes writing the packet masks and magic number.
  void FinalizeFecHeaders(size_t num_fec_packets, uint32_t media_ssrc, uint16_t seq_num_base, int* curBaseNum, int* pktMask, int maskLen);

  // Inserts the `received_packet` into the internal received FEC packet list
  // or into `recovered_packets`.
  void InsertPacket(const ReceivedPacket& received_packet, RecoveredPacketList* recovered_packets);

  // Inserts the `received_packet` into `recovered_packets`. Deletes duplicates.
  void InsertMediaPacket(RecoveredPacketList* recovered_packets, const ReceivedPacket& received_packet);

  // Assigns pointers to the recovered packet from all FEC packets which cover it.
  // Note: This reduces the complexity when we want to try to recover a packet
  // since we don't have to find the intersection between recovered packets and
  // packets covered by the FEC packet.
  void UpdateCoveringFecPackets(const RecoveredPacket& packet);

  void UpdateMaybeLossPackets(const RecoveredPacket& packet);

  // Insert `received_packet` into internal FEC list. Deletes duplicates.
  void InsertFecPacket(const RecoveredPacketList& recovered_packets, const ReceivedPacket& received_packet);

  // Assigns pointers to already recovered packets covered by `fec_packet`.
  static void AssignRecoveredPackets(const RecoveredPacketList& recovered_packets, ReceivedFecPacket* fec_packet);

  // Assigns pointers to the packets which may loss and we considered.
  void AssignMaybeLossPackets(std::shared_ptr<ReceivedFecPacket> fec_packet);

  // Delete the unavailable weak_ptr which the shared_ptr has been deleted.
  static void DeleteUnavailableWeakPtr(ConsiderLossPacketListWeak* consider_loss_packets);
  static void DeleteUnavailableWeakPtr(ReceivedFecPacketListWeak* fec_packets);

  // Attempt to recover missing packets, using the internally stored received FEC packets.
  int AttemptRecovery(RecoveredPacketList* recovered_packets);

  // Initializes headers and payload before the XOR operation that recovers a packet.
  static bool StartPacketRecovery(const std::vector<std::shared_ptr<ReceivedFecPacket>>& fec_packets, std::vector<std::unique_ptr<RecoveredPacket>>& recovered_packets);

  static void GFMulRTPHeaders(const Packet& src, Packet* dst, uint8_t GFValue);
  static void GFMulRTPPayloads(const Packet & src, size_t payload_length, size_t dst_offset, Packet * dst, uint8_t GFValue);

  static void GFMulFECHeaders(const Packet& src, Packet* dst, uint8_t GFValue);
  static void GFMulFECPayloads(const Packet & src, size_t payload_length, size_t src_offset, Packet * dst, uint8_t GFValue);

  // Finalizes recovery of packet by setting RTP header fields. This is not specific to the FEC scheme used.
  static bool FinishPacketRecovery(const std::vector<std::shared_ptr<ReceivedFecPacket>>& fec_packets, std::vector<std::unique_ptr<RecoveredPacket>>& recovered_packets);

  // Recover a missing packet.
  static bool RecoverPacket(const std::vector<std::shared_ptr<ReceivedFecPacket>>& fec_packets, std::vector<std::shared_ptr<ConsiderLossPacket>>& maybe_loss_packets, std::vector<std::unique_ptr<RecoveredPacket>>& recovered_packets);

  // Get the number of missing media packets which are covered by `fec_packet`.
  // An FEC packet can recover at most one packet, and if zero packets are
  // missing the FEC packet can be discarded. This function returns 2 when two
  // or more packets are missing.
  static int NumCoveredPacketsMissing(const ReceivedFecPacket& fec_packet);

  // Discards old packets in `recovered_packets`, which are no longer relevant
  // for recovering lost packets.
  void DiscardOldRecoveredPackets(RecoveredPacketList* recovered_packets);

  // Checks if the FEC packet is old enough and no longer relevant for
  // recovering lost media packets.
  bool IsOldFecPacket(const ReceivedFecPacket& fec_packet, const RecoveredPacketList* recovered_packets);

  static bool DfsFECPacket(std::vector<std::shared_ptr<ReceivedFecPacket>>& select_fec_packet, std::vector<std::shared_ptr<ConsiderLossPacket>>& select_cons_loss_packet, std::vector<uint16_t>& select_fec_pkt_seq_num, std::shared_ptr<ReceivedFecPacket> cur_fec_packet, uint16_t search_loss_seq_num);
  static bool DfsConsiderLossPacket(std::vector<std::shared_ptr<ReceivedFecPacket>>& select_fec_packet, std::vector<std::shared_ptr<ConsiderLossPacket>>& select_cons_loss_packet, std::vector<uint16_t>& select_fec_pkt_seq_num, std::shared_ptr<ConsiderLossPacket> cur_cons_loss_packet, uint16_t search_loss_seq_num);
  bool SearchFECPacket(std::vector<std::shared_ptr<ReceivedFecPacket>>& select_fec_packet, std::vector<std::shared_ptr<ConsiderLossPacket>>& select_cons_loss_packet, std::list<std::shared_ptr<ConsiderLossPacket>>& CMLP, std::list<std::shared_ptr<ReceivedFecPacket>>::iterator cur_fec_packet_it);

  // These SSRCs are only used by the decoder.
  const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;

  std::unique_ptr<RSFecHeaderReader> fec_header_reader_;
  std::unique_ptr<RSFecHeaderWriter> fec_header_writer_;

  std::vector<Packet> generated_fec_packets_;
  ReceivedFecPacketListShared received_fec_packets_;
  ConsiderLossPacketListShared consider_loss_packets_;

  // Arrays used to avoid dynamically allocating memory when generating the packet masks.
  // (There are never more than `kUlpfecMaxMediaPackets` FEC packets generated.)
  std::bitset<kRSfecMaxMediaPackets> packet_masks_;
  std::bitset<kRSfecMaxMediaPackets> tmp_packet_masks_;
  Clock* clock_;
  flat_map<uint16_t /*seq_num*/, std::optional<Timestamp>> fec_receive_time_;
};

// Classes derived from FecHeader{Reader,Writer} encapsulate the
// specifics of reading and writing FEC header for, e.g., ULPFEC
// and FlexFEC.
class RSFecHeaderReader {
 public:
  virtual ~RSFecHeaderReader();

  // The maximum number of media packets that can be covered by one FEC packet.
  size_t MaxMediaPackets() const;

  // The maximum number of FEC packets that is supported, per call to RSForwardErrorCorrection::EncodeRSFec().
  size_t MaxFecPackets() const;

  size_t MaxConsiderLossPackets() const;

  // Parses FEC header and stores information in ReceivedFecPacket members.
  virtual bool ReadFecHeader(RSForwardErrorCorrection::ReceivedFecPacket* fec_packet) const = 0;

 protected:
  RSFecHeaderReader(size_t max_media_packets, size_t max_fec_packets, size_t max_consider_loss_packets);

  const size_t max_media_packets_;
  const size_t max_fec_packets_;
  const size_t max_consider_loss_packets_;
};

class RSFecHeaderWriter {
 public:
  struct ProtectedStream{
    uint32_t ssrc = 0;
    uint16_t seq_num_base = 0;
    uint16_t cur_base_num = 0;
    std::bitset<32> mask = std::bitset<32>(0);
    size_t packet_mask_size = 0;
  };

  virtual ~RSFecHeaderWriter();

  // The maximum number of media packets that can be covered by one FEC packet.
  size_t MaxMediaPackets() const;

  // The maximum number of FEC packets that is supported, per call
  // to RSForwardErrorCorrection::EncodeRSFec().
  size_t MaxFecPackets() const;

  size_t MaxConsiderLossPackets() const;

  // The maximum overhead (in bytes) per packet, due to FEC headers.
  size_t MaxPacketOverhead() const;

  // Calculates the minimum packet mask size needed (in bytes),
  // given the discrete options of the ULPFEC masks and the bits
  // set in the current packet mask.
  virtual size_t MinPacketMaskSize(const uint8_t* packet_mask, size_t packet_mask_size) const = 0;

  // The header size (in bytes), given the packet mask size.
  virtual size_t FecHeaderSize() const = 0;

  // Writes FEC header.
  virtual void FinalizeFecHeader(rtc::ArrayView<const ProtectedStream> protected_streams, RSForwardErrorCorrection::Packet& fec_packet) const = 0;

 protected:
  RSFecHeaderWriter(size_t max_media_packets, size_t max_fec_packets, size_t max_consider_loss_packets, size_t max_packet_overhead);

  const size_t max_media_packets_;
  const size_t max_fec_packets_;
  const size_t max_consider_loss_packets_;
  const size_t max_packet_overhead_;
};

}

#endif // EXAMPLES_MYFECEXP_RSFEC_RS_FORWARD_ERROR_CORRECTION_H_