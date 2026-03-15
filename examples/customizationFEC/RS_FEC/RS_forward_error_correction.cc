
#include "examples/customizationFEC/RS_FEC/RS_forward_error_correction.h"

#include <string.h>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <fstream>

#include "absl/algorithm/container.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/flexfec_03_header_reader_writer.h"
#include "examples/customizationFEC/RS_FEC/RS_forward_error_correction_internal.h"
#include "examples/customizationFEC/RS_FEC/RS_fec_header_reader_writer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/mod_ops.h"
#include "examples/customizationFEC/Params.h"
#include "RS_forward_error_correction.h"

namespace webrtc{

namespace {
// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
constexpr size_t kTransportOverhead = 28;

constexpr uint16_t kOldSequenceThreshold = 128;  

constexpr uint16_t kConsiderLossSeqThreadshold = 128;

constexpr uint16_t kConsiderDecodeMaxN = 128;
}

//
// Implementation for RSForwardErrorCorrection::Packet
// 

RSForwardErrorCorrection::Packet::Packet() : data(0), ref_count_(0) {}
RSForwardErrorCorrection::Packet::~Packet() = default;

int32_t RSForwardErrorCorrection::Packet::AddRef() {
    return ++ref_count_;
}

int32_t RSForwardErrorCorrection::Packet::Release() {
    int32_t ref_count;
    ref_count = --ref_count_;
    if (ref_count == 0) delete this;
    return ref_count;
}

// This comparator is used to compare std::unique_ptr's pointing to
// subclasses of SortablePackets. It needs to be parametric since
// the std::unique_ptr's are not covariant w.r.t. the types that
// they are pointing to.
template <typename S, typename T>
bool RSForwardErrorCorrection::SortablePacket::LessThan::operator()(
    const S& first,
    const T& second) {
  RTC_DCHECK_EQ(first->ssrc, second->ssrc);
  return IsNewerSequenceNumber(second->seq_num, first->seq_num);
}

template <typename S, typename T>
bool RSForwardErrorCorrection::SortablePacket::LessThanWeak::operator()(
    const S& first,
    const T& second) {
  RTC_DCHECK_EQ(first.lock()->ssrc, second.lock()->ssrc);
  return IsNewerSequenceNumber(second.lock()->seq_num, first.lock()->seq_num);
}

//
// Constructors and destructors!
//

RSForwardErrorCorrection::ReceivedPacket::ReceivedPacket() = default;
RSForwardErrorCorrection::ReceivedPacket::~ReceivedPacket() = default;

RSForwardErrorCorrection::RecoveredPacket::RecoveredPacket() = default;
RSForwardErrorCorrection::RecoveredPacket::~RecoveredPacket() = default;

RSForwardErrorCorrection::ProtectedPacket::ProtectedPacket() = default;
RSForwardErrorCorrection::ProtectedPacket::~ProtectedPacket() = default;

RSForwardErrorCorrection::ReceivedFecPacket::ReceivedFecPacket() = default;
RSForwardErrorCorrection::ReceivedFecPacket::~ReceivedFecPacket() = default;

RSForwardErrorCorrection::ConsiderLossPacket::ConsiderLossPacket() = default;
RSForwardErrorCorrection::ConsiderLossPacket::~ConsiderLossPacket() = default;

RSForwardErrorCorrection::RSForwardErrorCorrection(
    std::unique_ptr<RSFecHeaderReader> fec_header_reader,
    std::unique_ptr<RSFecHeaderWriter> fec_header_writer,
    uint32_t ssrc,
    uint32_t protected_media_ssrc,
    Clock* clock)
    : ssrc_(ssrc),
      protected_media_ssrc_(protected_media_ssrc),
      fec_header_reader_(std::move(fec_header_reader)),
      fec_header_writer_(std::move(fec_header_writer)),
      generated_fec_packets_(fec_header_writer_->MaxFecPackets()),
      clock_(clock)
      {}

RSForwardErrorCorrection::~RSForwardErrorCorrection() = default;

std::unique_ptr<RSForwardErrorCorrection> RSForwardErrorCorrection::CreateRSFec(uint32_t ssrc, Clock* clock) {
  std::unique_ptr<RSFecHeaderReader> fec_header_reader(new RSfecHeaderReader());
  std::unique_ptr<RSFecHeaderWriter> fec_header_writer(new RSfecHeaderWriter());
  return std::unique_ptr<RSForwardErrorCorrection>(new RSForwardErrorCorrection(
      std::move(fec_header_reader), std::move(fec_header_writer), ssrc, ssrc, clock));
}

//
// Following Function is for Encoding!
//

int RSForwardErrorCorrection::EncodeRSFec(const PacketList& media_packets,uint8_t protection_factor, std::list<Packet*>* fec_packets)
{
    const size_t num_media_packets = media_packets.size();

    // std::cout<<"====Run EncodeRSFec with num_media_packets="<<num_media_packets<<std::endl;

    // int ii=0;
    // for (const auto& media_packet : media_packets) {
    //   std::cout<<"The front 30 bytes of the "<< ii <<" media pkt is:"<<std::endl;
    //   for (int jj=0;jj<30;jj++) {
    //     std::cout<<(int)media_packet->data[jj]<<" ";
    //   }
    //   std::cout<<std::endl;
    //   ii++;
    // }

    // Sanity check arguments.
    RTC_DCHECK_GT(num_media_packets, 0);
    RTC_DCHECK(fec_packets->empty());
    const size_t max_media_packets = fec_header_writer_->MaxMediaPackets();

    if (num_media_packets > max_media_packets) {
        RTC_LOG(LS_WARNING) << "Can't protect " << num_media_packets  << " media packets per frame. Max is " << max_media_packets << ".";
        return -1;
    }

    // std::cout<<"====We ensure num_media_packets > max_media_packets"<<std::endl;

    // Error check the media packets.
    for (const auto& media_packet : media_packets) {
        RTC_DCHECK(media_packet);
        if (media_packet->data.size() < kRtpHeaderSize) {
            RTC_LOG(LS_WARNING) << "Media packet " << media_packet->data.size() << " bytes is smaller than RTP header.";
            return -1;
        }
        // Ensure the FEC packets will fit in a typical MTU.
        if (media_packet->data.size() + MaxPacketOverhead() + kTransportOverhead > IP_PACKET_SIZE) {
            RTC_LOG(LS_WARNING) << "Media packet " << media_packet->data.size() << " bytes with overhead is larger than " << IP_PACKET_SIZE << " bytes.";
        }
    }

    // std::cout<<"====We ensure media_packet->data.size() < kRtpHeaderSize"<<std::endl;

    // Prepare generated FEC packets.
    int num_fec_packets = 0;
    if (inputV::Params::type == inputV::ExpType::RSFECStreamStableRate || inputV::Params::type == inputV::ExpType::SwiftFECAblM) {
      num_fec_packets = inputV::Params::generate_fec_num;
    }
    else if (inputV::Params::type == inputV::ExpType::RLSRSFEC || inputV::Params::type == inputV::ExpType::SwiftFECAblL || inputV::Params::type == inputV::ExpType::SwiftFECAblI) {
      num_fec_packets = transV::Params::M;
    }
    else if (inputV::Params::type == inputV::ExpType::RSFECBlock) {
      num_fec_packets = NumFecPackets(num_media_packets, protection_factor);
    }
    else if (inputV::Params::type == inputV::ExpType::RSFECStreamSourceRate) {
      num_fec_packets = NumFecPackets(num_media_packets, protection_factor);
    }
    else {
      num_fec_packets = NumFecPackets(num_media_packets, protection_factor);
    }

    // std::cout<<"====We calculate the num_fec_packets as num_fec_packets=" << num_fec_packets << std::endl;

    if (num_fec_packets == 0) {
        return 0;
    }

    for (int i = 0; i < num_fec_packets; ++i) {
        generated_fec_packets_[i].data.EnsureCapacity(IP_PACKET_SIZE);
        memset(generated_fec_packets_[i].data.MutableData(), 0, IP_PACKET_SIZE);
        // Use this as a marker for untouched packets.
        generated_fec_packets_[i].data.SetSize(0);
        fec_packets->push_back(&generated_fec_packets_[i]);
    }

    // ParseSequenceNumber for every packets to generate packets mask!
    auto media_packets_it = media_packets.cbegin();
    int* pktMask = (int*) malloc(sizeof(int) * 32);
    memset(pktMask, 0, sizeof(int) * 32);
    uint16_t prev_seq_num=ParseSequenceNumber((*media_packets_it)->data.data());
    size_t media_pkt_idx=0;
    while (media_packets_it != media_packets.end()){
        pktMask[media_pkt_idx]=1;
        media_packets_it++;
        if (media_packets_it!=media_packets.end()){
            uint16_t seq_num=ParseSequenceNumber((*media_packets_it)->data.data());
            media_pkt_idx += static_cast<uint16_t>(seq_num-prev_seq_num);
            if (media_pkt_idx>=32){
                RTC_LOG(LS_WARNING) << "Media packets Sequence Number difference beyond 32! Which cann't be protected!";
                if (pktMask!=NULL) free(pktMask);
                fec_packets->clear();
                return -1; 
            }
            prev_seq_num=seq_num;
        }
    }

    size_t maskLen=media_pkt_idx+1;

    int* curBaseNum=internal::GenerateBaseNumList(num_media_packets,num_fec_packets);
    if (curBaseNum==NULL) {
        RTC_LOG(LS_WARNING) << "Generate base number list fail!" ;
        if (pktMask!=NULL) free(pktMask);
        fec_packets->clear();
        return -1;
    }
    uint8_t* encoding_table = internal::GenerateCodingMatrix(num_media_packets,num_fec_packets,curBaseNum,pktMask,maskLen);
    if (encoding_table==NULL){
        RTC_LOG(LS_WARNING) << "Generate encoding table fail!" ;
        if (pktMask!=NULL) free(pktMask);
        if (curBaseNum!=NULL) free(curBaseNum);
        fec_packets->clear();
        return -1;
    }

    // std::cout<<"Generate based_num:"<<std::endl;
    // for (int ii=0;ii<num_fec_packets;ii++){
    //   std::cout<<curBaseNum[ii]<<" ";
    // }
    // std::cout<<std::endl;

    // std::cout<<"encoding_table:"<<std::endl;
    // for (int ii=0;ii<num_fec_packets;ii++) {
    //   for (int jj=0;jj<(int)num_media_packets;jj++) {
    //     std::cout<<(int)encoding_table[ii*num_media_packets+jj]<<" ";
    //   }
    //   std::cout<<std::endl;
    // }

    //Write RS-FEC packets to `generated_fec_pacekts_`.
    GenerateFecPayloads(media_packets, num_media_packets, num_fec_packets, encoding_table);
    const uint32_t media_ssrc = ParseSsrc(media_packets.front()->data.data());
    const uint16_t seq_num_base=ParseSequenceNumber(media_packets.front()->data.data());
    FinalizeFecHeaders(num_fec_packets, media_ssrc, seq_num_base, curBaseNum, pktMask, maskLen);

    if (pktMask!=NULL) free(pktMask);
    if (curBaseNum!=NULL) free(curBaseNum);
    if (encoding_table!=NULL) free(encoding_table);

    return 0;
}

int RSForwardErrorCorrection::NumFecPackets(int num_media_packets, int protection_factor) {
  // Result in Q0 with an unsigned round.
  int num_fec_packets = (num_media_packets * protection_factor + (1 << 7)) >> 8;
  // Generate at least one FEC packet if we need protection.
  if (protection_factor > 0 && num_fec_packets == 0) {
    num_fec_packets = 1;
  }
  RTC_DCHECK_LE(num_fec_packets, num_media_packets);
  return num_fec_packets;
}

void RSForwardErrorCorrection::GenerateFecPayloads(
    const PacketList& media_packets,
    size_t n,
    size_t k,
    uint8_t* encoding_table){
    RTC_DCHECK(!media_packets.empty());

    for (size_t i=0;i<k;i++){
        Packet* const fec_packet = &generated_fec_packets_[i];
        const size_t fec_header_size=fec_header_writer_->FecHeaderSize();
        size_t pkt_coding_idx=i*n;
        // Iterate all media_packets!
        auto media_packets_it = media_packets.cbegin();
        size_t j=0;
        while (media_packets_it != media_packets.end()){
            Packet* const media_packet = media_packets_it->get();
            size_t media_payload_length = media_packet->data.size()-kRtpHeaderSize;
            size_t fec_packet_length = fec_header_size + media_payload_length;
            if (fec_packet_length > fec_packet->data.size()){
                size_t old_size=fec_packet->data.size();
                fec_packet->data.SetSize(fec_packet_length);
                memset(fec_packet->data.MutableData()+old_size,0,fec_packet_length-old_size);
            }
            GFMulRTPHeaders(*media_packet, fec_packet, encoding_table[pkt_coding_idx+j]);
            GFMulRTPPayloads(*media_packet, media_payload_length, fec_header_size, fec_packet, encoding_table[pkt_coding_idx+j]);
            media_packets_it++;
            j++;
        }
        RTC_DCHECK_GT(fec_packet->data.size(), 0) << "Packet encoding table is wrong or poorly designed.";
    }
}

void RSForwardErrorCorrection::FinalizeFecHeaders(size_t num_fec_packets,
                                                uint32_t media_ssrc,
                                                uint16_t seq_num_base,
                                                int* curBaseNum,
                                                int* pktMask,
                                                int  maskLen) {
    std::bitset<32> curPktMask= std::bitset<32>(0);
    for (int i = 0;i < maskLen;i++){
        curPktMask[i]=pktMask[i];
    }
    for (size_t i = 0; i < num_fec_packets; ++i) {
        const RSFecHeaderWriter::ProtectedStream protected_streams[] = {
            {.ssrc = media_ssrc,
            .seq_num_base = seq_num_base,
            .cur_base_num = static_cast<uint16_t> (curBaseNum[i]),
            .mask = curPktMask}};
        fec_header_writer_->FinalizeFecHeader(protected_streams, generated_fec_packets_[i]);
    }
}

//
// Following Function is for Decoding!
//

void RSForwardErrorCorrection::ResetState(
    RecoveredPacketList* recovered_packets) {
  // Free the memory for any existing recovered packets, if the caller hasn't.
  recovered_packets->clear();
  received_fec_packets_.clear();
  consider_loss_packets_.clear();
}


void RSForwardErrorCorrection::InsertMediaPacket(
    RecoveredPacketList* recovered_packets,
    const ReceivedPacket& received_packet) {
  RTC_DCHECK_EQ(received_packet.ssrc, protected_media_ssrc_);

  // Search for duplicate packets.
  for (const auto& recovered_packet : *recovered_packets) {
    RTC_DCHECK_EQ(recovered_packet->ssrc, received_packet.ssrc);
    if (recovered_packet->seq_num == received_packet.seq_num) {
      // Duplicate packet, no need to add to list.
      return;
    }
  }

  std::unique_ptr<RecoveredPacket> recovered_packet(new RecoveredPacket());
  // This "recovered packet" was not recovered using parity packets.
  recovered_packet->was_recovered = false;
  // This media packet has already been passed on.
  recovered_packet->returned = true;
  recovered_packet->ssrc = received_packet.ssrc;
  recovered_packet->seq_num = received_packet.seq_num;
  recovered_packet->pkt = received_packet.pkt;
  // TODO(holmer): Consider replacing this with a binary search for the right
  // position, and then just insert the new packet. Would get rid of the sort.
  RecoveredPacket* recovered_packet_ptr = recovered_packet.get();
  recovered_packets->push_back(std::move(recovered_packet));
  recovered_packets->sort(SortablePacket::LessThan());
  UpdateCoveringFecPackets(*recovered_packet_ptr);
  UpdateMaybeLossPackets(*recovered_packet_ptr);
}

void RSForwardErrorCorrection::UpdateCoveringFecPackets(
    const RecoveredPacket& packet) {
  for (auto& fec_packet : received_fec_packets_) {
    // Is this FEC packet protecting the media packet `packet`?
    auto protected_it = absl::c_lower_bound(
        fec_packet->protected_packets, &packet, SortablePacket::LessThan());
    if (protected_it != fec_packet->protected_packets.end() &&
        (*protected_it)->seq_num == packet.seq_num) {
      // Found an FEC packet which is protecting `packet`.
      (*protected_it)->pkt = packet.pkt;
      uint16_t seq_num_diff = MinDiff((*protected_it)->seq_num, fec_packet->protected_streams[0].seq_num_base);
      fec_packet->protected_streams[0].arrive[seq_num_diff]=1;
    }
  }
}

void RSForwardErrorCorrection::UpdateMaybeLossPackets(const RecoveredPacket& packet) {
  auto maybe_loss_packet_it = absl::c_lower_bound(consider_loss_packets_, &packet, SortablePacket::LessThan());
  if (maybe_loss_packet_it != consider_loss_packets_.end() && (*maybe_loss_packet_it)->seq_num == packet.seq_num){
    DeleteUnavailableWeakPtr(&(*maybe_loss_packet_it)->reference_fec_packets);
    auto exist_fec_packet_it = (*maybe_loss_packet_it)->reference_fec_packets.cbegin();
    while (exist_fec_packet_it != (*maybe_loss_packet_it)->reference_fec_packets.end()) {
      DeleteUnavailableWeakPtr(&(*exist_fec_packet_it).lock()->maybe_loss_packets);
      auto fec_maybe_loss_packet_it = (*exist_fec_packet_it).lock()->maybe_loss_packets.cbegin();
      while (fec_maybe_loss_packet_it != (*exist_fec_packet_it).lock()->maybe_loss_packets.end()) {
        if ((*fec_maybe_loss_packet_it).lock() -> seq_num == packet.seq_num) {
          (*exist_fec_packet_it).lock()->maybe_loss_packets.erase(fec_maybe_loss_packet_it);
          break;
        }
        fec_maybe_loss_packet_it++;
      }
      exist_fec_packet_it++;
    }
    consider_loss_packets_.erase(maybe_loss_packet_it);
  }
}

void RSForwardErrorCorrection::InsertFecPacket(
    const RecoveredPacketList& recovered_packets,
    const ReceivedPacket& received_packet) {
  RTC_DCHECK_EQ(received_packet.ssrc, ssrc_);

  // Check for duplicate.
  for (const auto& existing_fec_packet : received_fec_packets_) {
    RTC_DCHECK_EQ(existing_fec_packet->ssrc, received_packet.ssrc);
    if (existing_fec_packet->seq_num == received_packet.seq_num) {
      // Drop duplicate FEC packet data.
      return;
    }
  }

  std::shared_ptr<ReceivedFecPacket> fec_packet(new ReceivedFecPacket());
  fec_packet->pkt = received_packet.pkt;
  fec_packet->ssrc = received_packet.ssrc;
  fec_packet->seq_num = received_packet.seq_num;
  fec_packet->if_used = false;
  // Parse ULPFEC/FlexFEC header specific info.
  bool ret = fec_header_reader_->ReadFecHeader(fec_packet.get());
  if (!ret) {
    return;
  }

  RTC_CHECK_EQ(fec_packet->protected_streams.size(), 1);

  if (fec_packet->protected_streams[0].ssrc != protected_media_ssrc_) {
    RTC_LOG(LS_INFO) << "Received FEC packet is protecting an unknown media SSRC; dropping.";
    return;
  }

  if (fec_packet->protected_streams[0].packet_mask_offset + fec_packet->protected_streams[0].packet_mask_size > fec_packet->pkt->data.size()) {
    RTC_LOG(LS_INFO) << "Received corrupted FEC packet; dropping.";
    return;
  }

  // std::cout<<"\t\t ====We receive fec packet:"<<fec_packet->seq_num<<std::endl;
  // std::cout<<"\t\t ====Which mask is:";
  // for (uint16_t idx=0; idx<fec_packet->protected_streams[0].packet_mask_size*8; idx++) {
  //   std::cout<<fec_packet->protected_streams[0].mask[idx];
  // }
  // std::cout<<std::endl;
  // std::cout<<"\t\t ====Which seqNumBase is:"<<fec_packet->protected_streams[0].seq_num_base<<std::endl;

  for (uint16_t idx=0; idx<fec_packet->protected_streams[0].packet_mask_size*8; idx++) {
    if (fec_packet->protected_streams[0].mask[idx] == 1) {
      
      // std::cout<<"\t\t ======Add protected pkt seq_num:"<< fec_packet->protected_streams[0].seq_num_base + idx <<std::endl;

      std::unique_ptr<ProtectedPacket> protected_packet(new ProtectedPacket());
      // This wraps naturally with the sequence number.
      protected_packet->ssrc = protected_media_ssrc_;
      protected_packet->seq_num = static_cast<uint16_t>(fec_packet->protected_streams[0].seq_num_base + idx);
      protected_packet->pkt = nullptr;
      fec_packet->protected_packets.push_back(std::move(protected_packet));
    }
  }

  if (fec_packet->protected_packets.empty()) {
    // All-zero packet mask; we can discard this FEC packet.
    RTC_LOG(LS_WARNING) << "Received FEC packet has an all-zero packet mask.";
  } else {
    AssignRecoveredPackets(recovered_packets, fec_packet.get());
    AssignMaybeLossPackets(fec_packet);
    // TODO(holmer): Consider replacing this with a binary search for the right
    // position, and then just insert the new packet. Would get rid of the sort.
    
    // std::cout<<"Join the fec packet to received_fec_packets_"<<std::endl;

    received_fec_packets_.push_back(fec_packet);
    received_fec_packets_.sort(SortablePacket::LessThan());
    const size_t max_fec_packets = fec_header_reader_->MaxFecPackets();
    if (received_fec_packets_.size() > max_fec_packets) {

      auto fec_packet_it = received_fec_packets_.cbegin();

      //Write file!
      auto now = std::chrono::system_clock::now();
      auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
      std::ofstream fec(inputV::Params::output+"forward_error_correction.csv",std::ios::app);
      std::string protect_seq_num="";
      bool first=true;
      for (const auto& protected_packet : (**fec_packet_it).protected_packets) {
        if (first){
          protect_seq_num=protect_seq_num+std::to_string(protected_packet->seq_num);
          first=false;
        }
        else{
          protect_seq_num=protect_seq_num+"_"+std::to_string(protected_packet->seq_num);
        }
      }

      TimeDelta fec_drop_time= clock_->CurrentTime() - clock_->CurrentTime();
      if (fec_receive_time_[(**fec_packet_it).seq_num] != std::nullopt) {
        fec_drop_time = clock_->CurrentTime() - *fec_receive_time_[(**fec_packet_it).seq_num];
        fec_receive_time_.erase((**fec_packet_it).seq_num);
      }
      else {
        std::cout<<"Maybe wrong! fec_receive_time_[(**fec_packet_it).seq_num] is not found!"<<std::endl;
      }
      
      fec<<milliseconds_since_epoch<<","<<(**fec_packet_it).seq_num<<","<<"fail_BufferLimit"<<","<<","<<protect_seq_num<<",,,"<<fec_drop_time.ns()<<"\n";
      fec.close();

      received_fec_packets_.pop_front();
    }
    RTC_DCHECK_LE(received_fec_packets_.size(), max_fec_packets);
  }

  Timestamp now = clock_->CurrentTime();
  fec_receive_time_[received_packet.seq_num] = now;
}

void RSForwardErrorCorrection::AssignRecoveredPackets(
    const RecoveredPacketList& recovered_packets,
    ReceivedFecPacket* fec_packet) {
  ProtectedPacketList* protected_packets = &fec_packet->protected_packets;
  std::vector<RecoveredPacket*> recovered_protected_packets;
  uint16_t baseSN = fec_packet->protected_streams[0].seq_num_base;

  // Find intersection between the (sorted) containers `protected_packets`
  // and `recovered_packets`, i.e. all protected packets that have already
  // been recovered. Update the corresponding protected packets to point to
  // the recovered packets.
  auto it_p = protected_packets->cbegin();
  auto it_r = recovered_packets.cbegin();
  SortablePacket::LessThan less_than;
  while (it_p != protected_packets->end() && it_r != recovered_packets.end()) {
    if (less_than(*it_p, *it_r)) {
      ++it_p;
    } else if (less_than(*it_r, *it_p)) {
      ++it_r;
    } else {  // *it_p == *it_r.
      // This protected packet has already been recovered.
      (*it_p)->pkt = (*it_r)->pkt;
      uint16_t seq_num_diff = MinDiff(baseSN, (*it_p)->seq_num);
      if (seq_num_diff < 32) fec_packet->protected_streams[0].arrive[seq_num_diff]=1;
      else RTC_LOG(LS_WARNING) << "The FEC packet protects the Packet which sequence number beyond 32 from baseSN.";
      ++it_p;
      ++it_r;
    }
  }
}

void RSForwardErrorCorrection::AssignMaybeLossPackets(std::shared_ptr<ReceivedFecPacket> fec_packet) {
  ProtectedPacketList* protected_packets = &fec_packet->protected_packets;

  auto it_p = protected_packets->cbegin();
  auto it_c = consider_loss_packets_.cbegin();
  SortablePacket::LessThan less_than;
  while (it_p != protected_packets->end()){
    if ((*it_p)->pkt == nullptr){
      while (it_c != consider_loss_packets_.end() && less_than(*it_c,*it_p)) ++it_c;
      if (it_c != consider_loss_packets_.end() && (*it_c) -> seq_num == (*it_p) -> seq_num){
        (*it_c) -> reference_fec_packets.push_back(std::weak_ptr<ReceivedFecPacket>(fec_packet));
        fec_packet -> maybe_loss_packets.push_back(std::weak_ptr<ConsiderLossPacket> (*it_c));
      }
      else {
        std::shared_ptr<ConsiderLossPacket> loss_packet(new ConsiderLossPacket());
        loss_packet->ssrc = (*it_p) -> ssrc;
        loss_packet->seq_num = (*it_p) -> seq_num; 
        loss_packet->pkt = nullptr;
        loss_packet->if_used = false;
        loss_packet->reference_fec_packets.push_back(std::weak_ptr<ReceivedFecPacket>(fec_packet));
        fec_packet -> maybe_loss_packets.push_back(std::weak_ptr<ConsiderLossPacket>(loss_packet));
        consider_loss_packets_.push_back(loss_packet);
      }
    }
    it_p++;
  }
  consider_loss_packets_.sort(SortablePacket::LessThan());
  const size_t max_consider_packets = fec_header_reader_->MaxConsiderLossPackets();
  while (consider_loss_packets_.size() > max_consider_packets) {
    consider_loss_packets_.pop_front();
  }
  RTC_DCHECK_LE(consider_loss_packets_.size(), max_consider_packets);
}

void RSForwardErrorCorrection::DeleteUnavailableWeakPtr(ConsiderLossPacketListWeak* consider_loss_packets) {
  auto consider_loss_pkt_it = consider_loss_packets->cbegin();
  while (consider_loss_pkt_it != consider_loss_packets->end()) {
    if (!(*consider_loss_pkt_it).lock()) {
      consider_loss_pkt_it = consider_loss_packets->erase(consider_loss_pkt_it);
    }
    else {
      consider_loss_pkt_it++;
    }
  }
}

void RSForwardErrorCorrection::DeleteUnavailableWeakPtr(ReceivedFecPacketListWeak* fec_packets) {
  auto fec_pkt_it = fec_packets->cbegin();
  while (fec_pkt_it != fec_packets->end()) {
    if (!(*fec_pkt_it).lock()) {
      fec_pkt_it = fec_packets->erase(fec_pkt_it);
    }
    else {
      fec_pkt_it++;
    }
  }
}

void RSForwardErrorCorrection::InsertPacket(
    const ReceivedPacket& received_packet,
    RecoveredPacketList* recovered_packets) {
    if (!received_fec_packets_.empty() && received_packet.ssrc == received_fec_packets_.front()->ssrc) {
      // It only makes sense to detect wrap-around when `received_packet`
      // and `front_received_fec_packet` belong to the same sequence number
      // space, i.e., the same SSRC. This happens when `received_packet`
      // is a FEC packet, or if `received_packet` is a media packet and
      // RED+ULPFEC is used.
      auto it = received_fec_packets_.begin();
      while (it != received_fec_packets_.end()) {
        uint16_t seq_num_diff = MinDiff(received_packet.seq_num, (*it)->seq_num);
        if (seq_num_diff > kOldSequenceThreshold) {
          it = received_fec_packets_.erase(it);
        } else {
          // No need to keep iterating, since `received_fec_packets_` is sorted.
          break;
        }
      }
    }

    if (!consider_loss_packets_.empty() && received_packet.ssrc == consider_loss_packets_.front()->ssrc) {
      auto it = consider_loss_packets_.begin();
      while (it != consider_loss_packets_.end()) {
        uint16_t seq_num_diff = MinDiff(received_packet.seq_num, (*it)->seq_num);
        if (seq_num_diff > kConsiderLossSeqThreadshold) {
          it = consider_loss_packets_.erase(it);
        } else {
          break;
        }
      }
    }

    if (received_packet.is_fec) {
      InsertFecPacket(*recovered_packets, received_packet);
    } else {
      InsertMediaPacket(recovered_packets, received_packet);
    }

    DiscardOldRecoveredPackets(recovered_packets);
}

bool RSForwardErrorCorrection::StartPacketRecovery(const std::vector<std::shared_ptr<ReceivedFecPacket>>& fec_packets, std::vector<std::unique_ptr<RecoveredPacket>>& recovered_packets) {
  // Init recovered_packets.
  size_t fec_pkt_n = fec_packets.size();
  for (size_t i=0; i<fec_pkt_n; i++) {
    std::unique_ptr<RecoveredPacket> tmp_recovered_packet(new RecoveredPacket());
    tmp_recovered_packet->pkt = new Packet();
    recovered_packets.push_back(std::move(tmp_recovered_packet));
  }

  // Sanity check packet length.
  auto fec_pkt_it = fec_packets.cbegin();
  while (fec_pkt_it != fec_packets.end()) {
    if ((*fec_pkt_it)->pkt->data.size() < (*fec_pkt_it)->fec_header_size + (*fec_pkt_it)->protection_length) {
      RTC_LOG(LS_WARNING) << "The FEC packet is truncated: it does not contain enough room for its own header.";
      return false;
    }
    if ((*fec_pkt_it)->protection_length > std::min(size_t{IP_PACKET_SIZE - kRtpHeaderSize}, IP_PACKET_SIZE - (*fec_pkt_it)->fec_header_size)) {
      RTC_LOG(LS_WARNING) << "Incorrect protection length, dropping FEC packet.";
      return false;
    }
    fec_pkt_it++;
  }

  // Init space of recovered packets.
  size_t max_protection_length = 0;
  auto fec_it = fec_packets.cbegin();
  while (fec_it != fec_packets.end()) {
    if (max_protection_length < (*fec_it)->protection_length) {
      max_protection_length = (*fec_it)->protection_length;
    }
    fec_it++;
  }
  for (size_t rec_pkt_i=0; rec_pkt_i<fec_pkt_n; rec_pkt_i++) {
    recovered_packets[rec_pkt_i]->pkt->data.EnsureCapacity(IP_PACKET_SIZE);
    recovered_packets[rec_pkt_i]->pkt->data.SetSize(max_protection_length + kRtpHeaderSize);
    memset(recovered_packets[rec_pkt_i]->pkt->data.MutableData(), 0, max_protection_length + kRtpHeaderSize);
    recovered_packets[rec_pkt_i]->returned = false;
    recovered_packets[rec_pkt_i]->was_recovered = true;
  }

  return true;
}

bool RSForwardErrorCorrection::FinishPacketRecovery(const std::vector<std::shared_ptr<ReceivedFecPacket>>& fec_packets, std::vector<std::unique_ptr<RecoveredPacket>>& recovered_packets) {
  auto cur_rec_pkt_it = recovered_packets.cbegin();
  while (cur_rec_pkt_it != recovered_packets.end()) {
    uint8_t* data = (*cur_rec_pkt_it)->pkt->data.MutableData();
    // Set the RTP version to 2.
    // data[0] |= 0x80;  // Set the 1st bit.
    // data[0] &= 0xbf;  // Clear the 2nd bit.
    // Recover the packet length, from temporary location.
    const size_t new_size = ByteReader<uint16_t>::ReadBigEndian(&data[2]) + kRtpHeaderSize;
    
    // std::cout<<"new_size="<<new_size<<" "<<"(IP_PACKET_SIZE - kRtpHeaderSize)"<<IP_PACKET_SIZE - kRtpHeaderSize<<std::endl;

    if (new_size > size_t{IP_PACKET_SIZE - kRtpHeaderSize}) {
      RTC_LOG(LS_WARNING) << "The recovered packet had a length larger than a typical IP packet, and is thus dropped.";
      return false;
    }
    size_t old_size = (*cur_rec_pkt_it)->pkt->data.size();
    (*cur_rec_pkt_it)->pkt->data.SetSize(new_size);
    data = (*cur_rec_pkt_it)->pkt->data.MutableData();
    if (new_size > old_size) {
      memset(data + old_size, 0, new_size - old_size);
    }

    // Set the SN field.
    ByteWriter<uint16_t>::WriteBigEndian(&data[2], (*cur_rec_pkt_it)->seq_num);
    // Set the SSRC field.
    ByteWriter<uint32_t>::WriteBigEndian(&data[8], (*cur_rec_pkt_it)->ssrc);

    cur_rec_pkt_it++;
  }
  return true;
}

void RSForwardErrorCorrection::GFMulRTPHeaders(const Packet& src, Packet* dst, uint8_t GFValue) {
  uint8_t* dst_data = dst->data.MutableData();
  const uint8_t* src_data = src.data.cdata();
  // XOR the first 2 bytes of the header: V, P, X, CC, M, PT fields.
  dst_data[0] = dst_data[0]^galois_single_multiply(src_data[0],GFValue,8);
  dst_data[1] = dst_data[1]^galois_single_multiply(src_data[1],GFValue,8);

  // XOR the length recovery field.
  uint8_t src_payload_length_network_order[2];
  ByteWriter<uint16_t>::WriteBigEndian(src_payload_length_network_order, src.data.size() - kRtpHeaderSize);

  // std::cout<<"Write pkt length:"<<src.data.size() - kRtpHeaderSize<<std::endl;

  dst_data[2] = dst_data[2]^galois_single_multiply(src_payload_length_network_order[0],GFValue,8);
  dst_data[3] = dst_data[3]^galois_single_multiply(src_payload_length_network_order[1],GFValue,8);

  // XOR the 5th to 8th bytes of the header: the timestamp field.
  dst_data[4] = dst_data[4]^galois_single_multiply(src_data[4],GFValue,8);
  dst_data[5] = dst_data[5]^galois_single_multiply(src_data[5],GFValue,8);
  dst_data[6] = dst_data[6]^galois_single_multiply(src_data[6],GFValue,8);
  dst_data[7] = dst_data[7]^galois_single_multiply(src_data[7],GFValue,8);

  // Skip the 9th to 12th bytes of the header.
}

void RSForwardErrorCorrection::GFMulRTPPayloads(const Packet& src,
                                         size_t payload_length,
                                         size_t dst_offset,
                                         Packet* dst,
                                         uint8_t GFValue) {

  // std::cout<<"Run GFMulRTPPayloads with:"<<std::endl;
  // std::cout<<"\t\tpayload_length="<<payload_length<<std::endl;
  // std::cout<<"\t\tdst_offset="<<dst_offset<<std::endl;
  // std::cout<<"\t\tGFValue="<<(int)GFValue<<std::endl;

  // XOR the payload.
  RTC_DCHECK_LE(kRtpHeaderSize + payload_length, src.data.size());
  RTC_DCHECK_LE(dst_offset + payload_length, dst->data.capacity());
  if (dst_offset + payload_length > dst->data.size()) {
    size_t old_size = dst->data.size();
    size_t new_size = dst_offset + payload_length;
    dst->data.SetSize(new_size);
    memset(dst->data.MutableData() + old_size, 0, new_size - old_size);
  }
  uint8_t* dst_data = dst->data.MutableData();
  const uint8_t* src_data = src.data.cdata();
  for (size_t i = 0; i < payload_length; ++i) {
    dst_data[dst_offset + i] = dst_data[dst_offset + i]^galois_single_multiply(src_data[kRtpHeaderSize + i],GFValue,8);
  }
}

void RSForwardErrorCorrection::GFMulFECHeaders(const Packet& src, Packet* dst, uint8_t GFValue) {
  uint8_t* dst_data = dst->data.MutableData();
  const uint8_t* src_data = src.data.cdata();
  // XOR the first 2 bytes of the header: V, P, X, CC, M, PT fields.
  dst_data[0] = dst_data[0]^galois_single_multiply(src_data[0],GFValue,8);
  dst_data[1] = dst_data[1]^galois_single_multiply(src_data[1],GFValue,8);

  // XOR the length recovery field.
  dst_data[2] = dst_data[2]^galois_single_multiply(src_data[2],GFValue,8);
  dst_data[3] = dst_data[3]^galois_single_multiply(src_data[3],GFValue,8);

  // XOR the 5th to 8th bytes of the header: the timestamp field.
  dst_data[4] = dst_data[4]^galois_single_multiply(src_data[4],GFValue,8);
  dst_data[5] = dst_data[5]^galois_single_multiply(src_data[5],GFValue,8);
  dst_data[6] = dst_data[6]^galois_single_multiply(src_data[6],GFValue,8);
  dst_data[7] = dst_data[7]^galois_single_multiply(src_data[7],GFValue,8);

  // Skip the 9th to 12th bytes of the header.
}

void RSForwardErrorCorrection::GFMulFECPayloads(const Packet& src,
                                         size_t payload_length,
                                         size_t src_offset,
                                         Packet* dst,
                                         uint8_t GFValue) {

  // std::cout<<"Run GFMulFECPayloads with:"<<std::endl;
  // std::cout<<"\t\tpayload_length="<<payload_length<<std::endl;
  // std::cout<<"\t\tsrc_offset="<<src_offset<<std::endl;
  // std::cout<<"\t\tGFValue="<<(int)GFValue<<std::endl;

  // XOR the payload.
  RTC_DCHECK_LE(src_offset + payload_length, src.data.size());
  RTC_DCHECK_LE(kRtpHeaderSize + payload_length, dst->data.capacity());
  if (kRtpHeaderSize + payload_length > dst->data.size()) {
    size_t old_size = dst->data.size();
    size_t new_size = kRtpHeaderSize + payload_length;
    dst->data.SetSize(new_size);
    memset(dst->data.MutableData() + old_size, 0, new_size - old_size);
  }
  uint8_t* dst_data = dst->data.MutableData();
  const uint8_t* src_data = src.data.cdata();
  for (size_t i = 0; i < payload_length; ++i) {
    dst_data[kRtpHeaderSize + i] = dst_data[kRtpHeaderSize + i]^galois_single_multiply(src_data[src_offset + i],GFValue,8);
  }
}

bool RSForwardErrorCorrection::RecoverPacket(const std::vector<std::shared_ptr<ReceivedFecPacket>>& fec_packets, std::vector<std::shared_ptr<ConsiderLossPacket>>& maybe_loss_packets, std::vector<std::unique_ptr<RecoveredPacket>>& recovered_packets) {

  if (fec_packets.size() == 0) return false;
  auto fec_packet_it = fec_packets.cbegin();
  std::shared_ptr<ReceivedFecPacket> cur_fec_packet = *fec_packet_it;
  uint16_t min_seq_num = cur_fec_packet->protected_streams[0].seq_num_base;

  // std::cout<<"mask and arrive"<<std::endl;
  // std::cout<<cur_fec_packet->protected_streams[0].mask<<std::endl;
  // std::cout<<cur_fec_packet->protected_streams[0].arrive<<std::endl;

  std::bitset<kConsiderDecodeMaxN> consider = std::bitset<kConsiderDecodeMaxN>(cur_fec_packet->protected_streams[0].mask.to_ullong());
  std::bitset<kConsiderDecodeMaxN> arrive = std::bitset<kConsiderDecodeMaxN>(cur_fec_packet->protected_streams[0].arrive.to_ullong());
  fec_packet_it++;

  // std::cout<<"start now_consider & now_arrive"<<std::endl;
  // for (int ii=0;ii<kConsiderDecodeMaxN;ii++){
  //   std::cout<<consider[ii];
  // }
  // std::cout<<std::endl;
  // for (int ii=0;ii<kConsiderDecodeMaxN;ii++){
  //   std::cout<<arrive[ii];
  // }
  // std::cout<<std::endl;

  while (fec_packet_it != fec_packets.end()) {
    cur_fec_packet = *fec_packet_it;
    uint16_t cur_seq_num = cur_fec_packet->protected_streams[0].seq_num_base;
    uint16_t seq_num_diff = MinDiff(min_seq_num, cur_seq_num);
    std::bitset<kConsiderDecodeMaxN> now_mask = std::bitset<kConsiderDecodeMaxN>(cur_fec_packet->protected_streams[0].mask.to_ullong());
    std::bitset<kConsiderDecodeMaxN> now_arrive = std::bitset<kConsiderDecodeMaxN>(cur_fec_packet->protected_streams[0].arrive.to_ullong());

    // std::cout<<"Current mask!"<<std::endl;
    // std::cout<<now_mask<<std::endl;
    // std::cout<<now_arrive<<std::endl;

    if (seq_num_diff > kConsiderDecodeMaxN-32) {
      RTC_LOG(LS_WARNING) << "Two intersecting FEC Packets' seq_num_base should not have over "<< kConsiderDecodeMaxN-32 <<" difference!";
      return false;
    }
    
    if (seq_num_diff == 0) {
      consider = consider | now_mask;
      arrive = arrive | now_arrive;
    }
    else if (IsNewerSequenceNumber(min_seq_num, cur_seq_num)) {
      // it means cur_seq_num is smaller, we should set min_seq_num = cur_seq_num
      min_seq_num = cur_seq_num;
      consider = (consider << seq_num_diff) | now_mask;
      arrive = (arrive << seq_num_diff) | now_arrive;
    }
    else if (IsNewerSequenceNumber(cur_seq_num, min_seq_num)) {
      // it means min_seq_num is the smaller one, we should not change it!
      consider = consider | (now_mask << seq_num_diff);
      arrive = arrive | (now_arrive << seq_num_diff);
    }
    fec_packet_it++;
  }

  // std::cout<<"now_consider & now_arrive(bit)"<<std::endl;
  // for (int ii=0;ii<kConsiderDecodeMaxN;ii++){
  //   std::cout<<consider[ii];
  // }
  // std::cout<<std::endl;
  // for (int ii=0;ii<kConsiderDecodeMaxN;ii++){
  //   std::cout<<arrive[ii];
  // }
  // std::cout<<std::endl;

  int curK = fec_packets.size();
  std::bitset<kConsiderDecodeMaxN>* k_mask_list = (std::bitset<kConsiderDecodeMaxN>*) malloc(sizeof(std::bitset<kConsiderDecodeMaxN>)*curK);
  int* baseNum_list = (int*) malloc(sizeof(int)*curK);
  int fec_pkt_i=0;
  int curN=0;
  std::vector<int> seq_num_To_id;
  fec_packet_it = fec_packets.cbegin();
  while (fec_packet_it != fec_packets.end()) {
    cur_fec_packet = *fec_packet_it;
    uint16_t cur_seq_num = cur_fec_packet->protected_streams[0].seq_num_base;
    uint16_t seq_num_diff = MinDiff(min_seq_num, cur_seq_num);
    k_mask_list[fec_pkt_i] = (std::bitset<kConsiderDecodeMaxN>(cur_fec_packet->protected_streams[0].mask.to_ullong())) << seq_num_diff;
    baseNum_list[fec_pkt_i] = cur_fec_packet->protected_streams[0].cur_base_num;
    fec_packet_it++;
    fec_pkt_i++;
  }

  // std::cout<<"curBaseNumList"<<std::endl;
  // for (int ii=0;ii<curK;ii++) {
  //   std::cout<<baseNum_list[ii]<<" ";
  // }
  // std::cout<<std::endl;

  for (size_t consider_i=0; consider_i<kConsiderDecodeMaxN; consider_i++) {
    if (consider[consider_i]==1) {
      seq_num_To_id.push_back(curN);
      curN++;
    }
    else {
      seq_num_To_id.push_back(-1);
    }
  }

  // K*N table! Remember to free this space!
  uint8_t* decoding_table = internal::GenerateDecodingMatrix(curN, curK, baseNum_list, consider, arrive, k_mask_list);
  if (decoding_table == NULL) {
    if (k_mask_list!=NULL) free(k_mask_list);
    if (baseNum_list!=NULL) free(baseNum_list);
    return false;
  }

  if (!StartPacketRecovery(fec_packets, recovered_packets)) {
    if (k_mask_list!=NULL) free(k_mask_list);
    if (baseNum_list!=NULL) free(baseNum_list);
    if (decoding_table!=NULL) free(decoding_table);
    return false;
  }

  auto rec_packet_it = recovered_packets.cbegin();
  int table_i = 0;
  while (rec_packet_it != recovered_packets.end()) {
    std::bitset<kConsiderDecodeMaxN> if_done = std::bitset<kConsiderDecodeMaxN>(0);
    (*rec_packet_it)->seq_num = maybe_loss_packets[table_i]->seq_num;
    (*rec_packet_it)->ssrc = maybe_loss_packets[table_i]->ssrc;
    int rec_pkt_i = 0;
    fec_packet_it = fec_packets.cbegin();
    while (fec_packet_it != fec_packets.end()) {
      cur_fec_packet = (*fec_packet_it);
      for (const auto& protected_packet : cur_fec_packet->protected_packets) {
        if (protected_packet->pkt == nullptr) continue;
        uint16_t seq_num_diff = MinDiff(min_seq_num, protected_packet->seq_num);
        if (consider[seq_num_diff]==1 && arrive[seq_num_diff]==1 && if_done[seq_num_diff]==0) {
          
          // std::cout<<"GFMul Regular Packet"<<std::endl;
          // std::cout<<"current if_done:"<<std::endl;
          // std::cout<<if_done<<std::endl;
          // std::cout<<"now GFMul:"<<seq_num_diff<<std::endl;
          // std::cout<<"the front 30 bytes is:"<<std::endl;
          // for (int ii=0;ii<30;ii++) {
          //   std::cout<<(int)protected_packet->pkt->data[ii]<<" ";
          // }
          // std::cout<<std::endl;

          GFMulRTPHeaders(*protected_packet->pkt, (*rec_packet_it)->pkt.get(), decoding_table[table_i*curN+seq_num_To_id[seq_num_diff]]);
          GFMulRTPPayloads(*protected_packet->pkt, protected_packet->pkt->data.size() - kRtpHeaderSize,
                        kRtpHeaderSize, (*rec_packet_it)->pkt.get(), decoding_table[table_i*curN+seq_num_To_id[seq_num_diff]]);
          if_done[seq_num_diff]=1;
        }
      }
      uint16_t seq_num_diff = MinDiff(min_seq_num, maybe_loss_packets[rec_pkt_i]->seq_num);
      
      // std::cout<<"GFMul FEC Packet"<<std::endl;
      // std::cout<<"now GFMul:"<<seq_num_diff<<std::endl;
      // std::cout<<"the front 30 bytes is:"<<std::endl;
      // for (int ii=0;ii<30;ii++) {
      //   std::cout<<(int)cur_fec_packet->pkt->data[ii]<<" ";
      // }
      // std::cout<<std::endl;

      GFMulFECHeaders(*cur_fec_packet->pkt, (*rec_packet_it)->pkt.get(), decoding_table[table_i*curN+seq_num_To_id[seq_num_diff]]);
      GFMulFECPayloads(*cur_fec_packet->pkt, cur_fec_packet->pkt->data.size() - cur_fec_packet->fec_header_size,
                    cur_fec_packet->fec_header_size, (*rec_packet_it)->pkt.get(), decoding_table[table_i*curN+seq_num_To_id[seq_num_diff]]);
      fec_packet_it++;
      rec_pkt_i++;
    }
    rec_packet_it++;
    table_i++;
  }

  // std::cout<<"Before FinishPacketRecovery:"<<std::endl;
  // for (int ii=0;ii<curK;ii++) {
  //   std::cout<<"The front 30 bytes of the "<< ii <<" recover pkt is:"<<std::endl;
  //   for (int jj=0;jj<30;jj++) {
  //     std::cout<<(int)recovered_packets[ii]->pkt->data[jj]<<" ";
  //   }
  //   std::cout<<std::endl;
  // }

  if (!FinishPacketRecovery(fec_packets, recovered_packets)) {
    if (k_mask_list!=NULL) free(k_mask_list);
    if (baseNum_list!=NULL) free(baseNum_list);
    if (decoding_table!=NULL) free(decoding_table);
    return false;
  }

  // std::cout<<"After FinishPacketRecovery:"<<std::endl;
  // for (int ii=0;ii<curK;ii++) {
  //   std::cout<<"The front 30 bytes of the "<< ii <<" recover pkt is:"<<std::endl;
  //   for (int jj=0;jj<30;jj++) {
  //     std::cout<<(int)recovered_packets[ii]->pkt->data[jj]<<" ";
  //   }
  //   std::cout<<std::endl;
  // }

  if (k_mask_list!=NULL) free(k_mask_list);
  if (baseNum_list!=NULL) free(baseNum_list);
  if (decoding_table!=NULL) free(decoding_table);
  return true;
}

int RSForwardErrorCorrection::AttemptRecovery(RecoveredPacketList* recovered_packets) {
  
  // std::cout<<"Run AttemptRecovery!!" << std::endl;
  // std::cout<<"Current recovered_packets.size():"<<recovered_packets->size()<<std::endl;
  // std::cout<<"\t seq_num:";
  // auto recv_pkt_it=recovered_packets->cbegin();
  // while (recv_pkt_it != recovered_packets->end()) {
  //   std::cout<<std::to_string((*recv_pkt_it)->seq_num)<<" ";
  //   recv_pkt_it++;
  // }
  // std::cout<<std::endl;
  
  size_t num_recovered_packets = 0;

  // Timestamp rec_ensure = clock_->CurrentTime();

  // Ensure that all FEC packets are valid!
  auto fec_packet_it = received_fec_packets_.cbegin();
  while (fec_packet_it != received_fec_packets_.end()) {
    bool fec_check_flag = true;
    auto fec_cons_loss_pkt_it = (*fec_packet_it)->maybe_loss_packets.cbegin();
    while (fec_cons_loss_pkt_it != (*fec_packet_it)->maybe_loss_packets.end()) {
      if (!(*fec_cons_loss_pkt_it).lock()) {
        fec_check_flag = false;
        break;
      }
      fec_cons_loss_pkt_it++;
    }
    size_t num_fec_pkt_loss = NumCoveredPacketsMissing(**fec_packet_it);
    if (fec_check_flag == false || num_fec_pkt_loss==0 || IsOldFecPacket(**fec_packet_it, recovered_packets)) {
      //Write file!
      auto now = std::chrono::system_clock::now();
      auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
      std::ofstream fec(inputV::Params::output+"forward_error_correction.csv",std::ios::app);
      std::string protect_seq_num="";
      bool first=true;
      for (const auto& protected_packet : (**fec_packet_it).protected_packets) {
        if (first){
          protect_seq_num=protect_seq_num+std::to_string(protected_packet->seq_num);
          first=false;
        }
        else{
          protect_seq_num=protect_seq_num+"_"+std::to_string(protected_packet->seq_num);
        }
      }
      
      TimeDelta fec_drop_time= clock_->CurrentTime() - clock_->CurrentTime();
      if (fec_receive_time_[(**fec_packet_it).seq_num] != std::nullopt) {
        fec_drop_time = clock_->CurrentTime() - *fec_receive_time_[(**fec_packet_it).seq_num];
        fec_receive_time_.erase((**fec_packet_it).seq_num);
      }
      else {
        std::cout<<"Maybe wrong! fec_receive_time_[(**fec_packet_it).seq_num] is not found!"<<std::endl;
      }

      if (fec_check_flag == false) {
        fec<<milliseconds_since_epoch<<","<<(**fec_packet_it).seq_num<<","<<"fail_ProtectPktTimeout"<<","<<","<<protect_seq_num<<",,,"<<fec_drop_time.ns()<<"\n";
      }
      else if (num_fec_pkt_loss==0) {
        fec<<milliseconds_since_epoch<<","<<(**fec_packet_it).seq_num<<","<<"fail_NoLoss"<<","<<","<<protect_seq_num<<",,,"<<fec_drop_time.ns()<<"\n";
      }
      else {
        fec<<milliseconds_since_epoch<<","<<(**fec_packet_it).seq_num<<","<<"fail_OldFEC"<<","<<","<<protect_seq_num<<",,,"<<fec_drop_time.ns()<<"\n";
      }

      fec.close();

      // std::cout<<"We drop fec packet seq_num:"<<(**fec_packet_it).seq_num<<" because of:";
      // if (fec_check_flag == false) {
      //   std::cout<<"consider packet drop!"<<std::endl;
      // }
      // else if (num_fec_pkt_loss==0) {
      //   std::cout<<"num_fec_pkt_loss==0"<<std::endl;
      // }
      // else {
      //   std::cout<<"IsOldFecPacket(**fec_packet_it, recovered_packets)"<<std::endl;
      // }

      fec_packet_it = received_fec_packets_.erase(fec_packet_it);
    }
    else {
      (*fec_packet_it)->maybe_loss_packets.sort(SortablePacket::LessThanWeak());
      fec_packet_it++;
    }
  }

  // std::cout<<"We ensure all FECPackets are valid!" << std::endl;
  // std::cout<<"Valid FEC Packet size:"<<received_fec_packets_.size()<<std::endl;
  // std::cout<<"FEC Packet details as follow:"<<std::endl;
  // fec_packet_it = received_fec_packets_.cbegin();
  // while (fec_packet_it != received_fec_packets_.end()) {
  //   std::cout<<"\tseq_num:"<<(*fec_packet_it)->seq_num;

  //   std::cout<<"\tmaybe loss:";
  //   auto fec_cons_loss_pkt_it = (*fec_packet_it)->maybe_loss_packets.cbegin();
  //   while (fec_cons_loss_pkt_it != (*fec_packet_it)->maybe_loss_packets.end()) {
  //     std::cout<<(*fec_cons_loss_pkt_it).lock()->seq_num<<" ";
  //     fec_cons_loss_pkt_it++;
  //   }

  //   std::cout<<"\tprotect:";
  //   auto fec_protect_pkt_it = (*fec_packet_it)->protected_packets.cbegin();
  //   while (fec_protect_pkt_it != (*fec_packet_it)->protected_packets.end()) {
  //     std::cout<<(*fec_protect_pkt_it)->seq_num<<" ";
  //     fec_protect_pkt_it++;
  //   }

  //   std::cout<<std::endl;
  //   fec_packet_it++;
  // }

  // std::cout<<"Current consider loss packet size:"<<consider_loss_packets_.size()<<std::endl;
  // std::cout<<"Consider loss packets details as follow:"<<std::endl;
  // auto pclp_it = consider_loss_packets_.cbegin();
  // while (pclp_it != consider_loss_packets_.end()) {
  //   std::cout<<"\tseq_num:"<<(*pclp_it)->seq_num;
  //   std::cout<<"\tprotected by:";
  //   DeleteUnavailableWeakPtr(&(*pclp_it)->reference_fec_packets);
  //   auto pclp_fec_pkt_it = (*pclp_it)->reference_fec_packets.cbegin();
  //   while (pclp_fec_pkt_it != (*pclp_it)->reference_fec_packets.end()) {
  //     std::cout<<(*pclp_fec_pkt_it).lock()->seq_num<<" ";
  //     pclp_fec_pkt_it++;
  //   }
  //   std::cout<<std::endl;
  //   pclp_it++;
  // }

  auto consider_loss_packet_it = consider_loss_packets_.cbegin();
  while (consider_loss_packet_it != consider_loss_packets_.end()) {
    std::shared_ptr<ConsiderLossPacket> consider_packet = *consider_loss_packet_it;
    DeleteUnavailableWeakPtr(&consider_packet->reference_fec_packets);
    // std::cout<<"\t\t current seq_num"<<consider_packet->seq_num<<"  ref_fec_pktsize:"<<consider_packet->reference_fec_packets.size()<<std::endl;
    if (consider_packet->reference_fec_packets.size() == 0) {
      consider_loss_packet_it = consider_loss_packets_.erase(consider_loss_packet_it);
    }
    else {
      consider_loss_packet_it++;
    }
  }

  // TimeDelta time_ensure = clock_->CurrentTime() - rec_ensure;
  // std::cout<<"Time for ensure fec packets valid:"<<time_ensure.ns()<<std::endl;

  bool search_flag;

  // Through DFS!
  // consider_loss_packet_it = consider_loss_packets_.cbegin();
  // std::vector<uint16_t> to_be_decoded_fec_seq_num;
  // std::vector<std::shared_ptr<ReceivedFecPacket>> to_be_decoded_fec_packet;
  // std::vector<std::shared_ptr<ConsiderLossPacket>> to_be_recover_loss_packet;
  // while (consider_loss_packet_it != consider_loss_packets_.end()) {
  //   std::shared_ptr<ConsiderLossPacket> consider_packet = *consider_loss_packet_it;
  //   to_be_decoded_fec_packet.clear();
  //   to_be_recover_loss_packet.clear();
  //   to_be_decoded_fec_seq_num.clear();
  //   if (DfsConsiderLossPacket(to_be_decoded_fec_packet, to_be_recover_loss_packet, to_be_decoded_fec_seq_num, consider_packet, consider_packet->seq_num)) break;
  //   else consider_loss_packet_it++;
  // }
  // if (consider_loss_packet_it != consider_loss_packets_.end()) {
  //   search_flag=true;
  // }
  // else {
  //   search_flag=false;
  // }

  Timestamp rec_search = clock_->CurrentTime();

  // Through new Algorithm!
  auto cur_fec_packet_it = received_fec_packets_.rbegin();
  std::vector<std::shared_ptr<ReceivedFecPacket>> to_be_decoded_fec_packet;
  std::vector<std::shared_ptr<ConsiderLossPacket>> to_be_recover_loss_packet;
  std::list<std::shared_ptr<ConsiderLossPacket>> CMLP;
  while (cur_fec_packet_it != received_fec_packets_.rend()) {
    
    // std::cout<<"====================================================================="<<std::endl;

    to_be_decoded_fec_packet.clear();
    to_be_recover_loss_packet.clear();
    CMLP.clear();
    auto cur_fec_packet_itb = cur_fec_packet_it.base();
    cur_fec_packet_itb--;
    if (SearchFECPacket(to_be_decoded_fec_packet, to_be_recover_loss_packet, CMLP, cur_fec_packet_itb)) break;
    else cur_fec_packet_it++;
  }
  if (cur_fec_packet_it != received_fec_packets_.rend()) {
    search_flag=true;
  }
  else {
    search_flag=false;
  }

  TimeDelta time_search = clock_->CurrentTime() - rec_search;
  // std::cout<<"Time for search decode opportunities:"<<time_search.ns()<<std::endl;

  // std::cout<<"We finish dfs search!" << std::endl;

  if (search_flag) {

    // std::cout<<"We find a decodable loss packet!" <<std::endl;

    // Sort the fecPackets based on to_be_decoded_fec_seq_num. Only use through DFS!
    // size_t fec_pkt_n = to_be_decoded_fec_seq_num.size();
    // for (size_t i = 0; i < fec_pkt_n; i++) {
    //   for (size_t j = 0; j< fec_pkt_n-i-1; j++) {
    //     if (to_be_decoded_fec_seq_num[j] > to_be_decoded_fec_seq_num[j+1]) {
    //       std::swap(to_be_decoded_fec_seq_num[j], to_be_decoded_fec_seq_num[j+1]);
    //       std::swap(to_be_decoded_fec_packet[j], to_be_decoded_fec_packet[j+1]);
    //       std::swap(to_be_recover_loss_packet[j], to_be_recover_loss_packet[j+1]);
    //     }
    //   }
    // }

    // std::cout<<"FEC Packet("<<to_be_decoded_fec_packet.size()<<"):";
    // for (size_t ii=0;ii<to_be_decoded_fec_packet.size();ii++) {
    //   std::cout<<to_be_decoded_fec_packet[ii]->seq_num<<" ";
    // }
    // std::cout<<std::endl;

    // std::cout<<"FEC Packet SeqNum("<<to_be_decoded_fec_seq_num.size()<<"):";
    // for (size_t ii=0;ii<to_be_decoded_fec_seq_num.size();ii++) {
    //   std::cout<<to_be_decoded_fec_seq_num[ii]<<" ";
    // }
    // std::cout<<std::endl;

    // std::cout<<"Consider Loss Packet("<<to_be_recover_loss_packet.size()<<"):";
    // for (size_t ii=0;ii<to_be_recover_loss_packet.size();ii++) {
    //   std::cout<<to_be_recover_loss_packet[ii]->seq_num<<" ";
    // }
    // std::cout<<std::endl;

    // Record recover time!
    Timestamp rec_now = clock_->CurrentTime();

    // Now these fec_packets can be decoded!
    std::vector<std::unique_ptr<RecoveredPacket>> temp_recover_packets;
    if (!RecoverPacket(to_be_decoded_fec_packet, to_be_recover_loss_packet, temp_recover_packets)) {
      
      // std::cout<<"RecoverPacket fail!"<<std::endl;
      
      // Can't recover using this packet, drop them!
      SortablePacket::LessThan less_than;
      std::sort(to_be_decoded_fec_packet.begin(),to_be_decoded_fec_packet.end(),SortablePacket::LessThan());
      auto cur_fec_pkt_it = to_be_decoded_fec_packet.cbegin();
      auto rec_fec_pkt_it = received_fec_packets_.cbegin();

      while (cur_fec_pkt_it != to_be_decoded_fec_packet.end() && rec_fec_pkt_it != received_fec_packets_.end()) {
        
        //Write file!
        auto now = std::chrono::system_clock::now();
        auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        std::ofstream fec(inputV::Params::output+"forward_error_correction.csv",std::ios::app);
        std::string protect_seq_num="";
        bool first=true;
        for (const auto& protected_packet : (**cur_fec_pkt_it).protected_packets) {
          if (first){
            protect_seq_num=protect_seq_num+std::to_string(protected_packet->seq_num);
            first=false;
          }
          else{
            protect_seq_num=protect_seq_num+"_"+std::to_string(protected_packet->seq_num);
          }
        }
        
        TimeDelta fec_drop_time= clock_->CurrentTime() - clock_->CurrentTime();
        if (fec_receive_time_[(**fec_packet_it).seq_num] != std::nullopt) {
          fec_drop_time = clock_->CurrentTime() - *fec_receive_time_[(**fec_packet_it).seq_num];
          fec_receive_time_.erase((**fec_packet_it).seq_num);
        }
        else {
          std::cout<<"Maybe wrong! fec_receive_time_[(**fec_packet_it).seq_num] is not found!"<<std::endl;
        }

        fec<<milliseconds_since_epoch<<","<<(**cur_fec_pkt_it).seq_num<<","<<"fail_RecoverFail"<<","<<","<<protect_seq_num<<",,,"<<fec_drop_time.ns()<<"\n";
        fec.close();
        
        while (rec_fec_pkt_it!=received_fec_packets_.end() && less_than(*rec_fec_pkt_it, *cur_fec_pkt_it)) rec_fec_pkt_it++;
        if (rec_fec_pkt_it!=received_fec_packets_.end() && (*cur_fec_pkt_it)->seq_num == (*rec_fec_pkt_it)->seq_num) {
          rec_fec_pkt_it = received_fec_packets_.erase(rec_fec_pkt_it);
        }
        cur_fec_pkt_it++;
      }

      auto cur_recover_loss_packet_it = to_be_recover_loss_packet.cbegin();

      while (cur_recover_loss_packet_it != to_be_recover_loss_packet.end()) {
        (*cur_recover_loss_packet_it)->if_used = false;
        cur_recover_loss_packet_it++;
      }

      return -1;
    }

    TimeDelta time_decode = clock_->CurrentTime() - rec_now;

    // std::cout<<"Recover Success!!"<<std::endl;

    num_recovered_packets+=temp_recover_packets.size();
    
    auto tp_rec_pkt_it = temp_recover_packets.begin();
    while (tp_rec_pkt_it != temp_recover_packets.end()) {
      auto* recovered_packet_ptr = (*tp_rec_pkt_it).get();
      recovered_packets->push_back(std::move((*tp_rec_pkt_it)));
      recovered_packets->sort(SortablePacket::LessThan());
      UpdateCoveringFecPackets(*recovered_packet_ptr);
      UpdateMaybeLossPackets(*recovered_packet_ptr);
      DiscardOldRecoveredPackets(recovered_packets);
      tp_rec_pkt_it++;
    }

    //Write file!
    auto now = std::chrono::system_clock::now();
    auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    std::ofstream fec(inputV::Params::output+"forward_error_correction.csv",std::ios::app);
    std::string protect_seq_num="";
    std::string fec_seq_num="";
    std::string fec_drop_time_str = "";
    std::string rec_seq_num="";
    bool if_add=true;
    auto fec_packet_it = to_be_decoded_fec_packet.cbegin();
    while (fec_packet_it != to_be_decoded_fec_packet.end()){
      if (if_add) {
        if_add=false;
        fec_seq_num=fec_seq_num+std::to_string((*fec_packet_it)->seq_num);
        TimeDelta fec_drop_time = clock_->CurrentTime() - *fec_receive_time_[(*fec_packet_it)->seq_num];
        fec_receive_time_.erase((*fec_packet_it)->seq_num);
        fec_drop_time_str = fec_drop_time_str+std::to_string(fec_drop_time.ns());
      }
      else {
        protect_seq_num = protect_seq_num+"|"; 
        fec_seq_num=fec_seq_num+"_"+std::to_string((*fec_packet_it)->seq_num);
        TimeDelta fec_drop_time = clock_->CurrentTime() - *fec_receive_time_[(*fec_packet_it)->seq_num];
        fec_receive_time_.erase((*fec_packet_it)->seq_num);
        fec_drop_time_str = fec_drop_time_str+ "_"+ std::to_string(fec_drop_time.ns());
      }
      bool first=true;
      for (const auto& protected_packet : (**fec_packet_it).protected_packets) {
        if (first){
          protect_seq_num=protect_seq_num+std::to_string(protected_packet->seq_num);
          first=false;
        }
        else{
          protect_seq_num=protect_seq_num+"_"+std::to_string(protected_packet->seq_num);
        }
      }
      fec_packet_it++;
    }
    if_add=true;
    auto rec_packet_it = to_be_recover_loss_packet.cbegin();
    while (rec_packet_it != to_be_recover_loss_packet.end()) {
      if (if_add) {
        if_add=false;
        rec_seq_num = rec_seq_num + std::to_string((*rec_packet_it)->seq_num);
      }
      else {
        rec_seq_num = rec_seq_num + "_" + std::to_string((*rec_packet_it)->seq_num);
      }
      rec_packet_it++;
    }
    
    fec<<milliseconds_since_epoch<<","<<fec_seq_num<<","<<"success"<<","<<rec_seq_num<<","<<protect_seq_num<<","<<time_decode.ns()<<","<<time_search.ns()<<","<<fec_drop_time_str<<"\n";
    fec.close();

    SortablePacket::LessThan less_than;
    std::sort(to_be_decoded_fec_packet.begin(),to_be_decoded_fec_packet.end(),SortablePacket::LessThan());
    auto cur_fec_pkt_it = to_be_decoded_fec_packet.cbegin();
    auto rec_fec_pkt_it = received_fec_packets_.cbegin();

    while (cur_fec_pkt_it != to_be_decoded_fec_packet.end() && rec_fec_pkt_it != received_fec_packets_.end()) {
      while (rec_fec_pkt_it!=received_fec_packets_.end() && less_than(*rec_fec_pkt_it, *cur_fec_pkt_it)) rec_fec_pkt_it++;
      if (rec_fec_pkt_it!=received_fec_packets_.end() && (*cur_fec_pkt_it)->seq_num == (*rec_fec_pkt_it)->seq_num) {
        rec_fec_pkt_it = received_fec_packets_.erase(rec_fec_pkt_it);
      }
      cur_fec_pkt_it++;
    }
  }

  return num_recovered_packets;
}

bool RSForwardErrorCorrection::SearchFECPacket(std::vector<std::shared_ptr<ReceivedFecPacket>>& select_fec_packet, std::vector<std::shared_ptr<ConsiderLossPacket>>& select_cons_loss_packet, std::list<std::shared_ptr<ConsiderLossPacket>>& CMLP, std::list<std::shared_ptr<ReceivedFecPacket>>::iterator cur_fec_packet_it) {
  SortablePacket::LessThan less_than;
  while (cur_fec_packet_it != received_fec_packets_.end()) {
    select_fec_packet.push_back(*cur_fec_packet_it);
    auto cur_cons_pkt_it = (*cur_fec_packet_it)->maybe_loss_packets.cbegin();
    auto select_cons_loss_packet_it = select_cons_loss_packet.cbegin();
    while (cur_cons_pkt_it != (*cur_fec_packet_it)->maybe_loss_packets.end() && select_cons_loss_packet_it != select_cons_loss_packet.end()) {
      while (select_cons_loss_packet_it != select_cons_loss_packet.end() && less_than(*select_cons_loss_packet_it,(*cur_cons_pkt_it).lock())) {
        select_cons_loss_packet_it++;
      }
      if (select_cons_loss_packet_it == select_cons_loss_packet.end()) break;
      if ((*select_cons_loss_packet_it)->seq_num != (*cur_cons_pkt_it).lock()->seq_num) {
        return false;
      }
      else {
        cur_cons_pkt_it++;
        select_cons_loss_packet_it++;
      }
    }
    if (select_cons_loss_packet_it != select_cons_loss_packet.end()) {
      return false;
    }
    while (cur_cons_pkt_it != (*cur_fec_packet_it)->maybe_loss_packets.end()) {
      select_cons_loss_packet.push_back((*cur_cons_pkt_it).lock());
      CMLP.push_back((*cur_cons_pkt_it).lock());
      cur_cons_pkt_it++;
    }

    RTC_DCHECK(!CMLP.empty());
    cur_cons_pkt_it = (*cur_fec_packet_it)->maybe_loss_packets.cbegin();
    auto CMLP_it = CMLP.cbegin();
    while (cur_cons_pkt_it != (*cur_fec_packet_it)->maybe_loss_packets.end() && less_than((*cur_cons_pkt_it).lock(),*CMLP_it)) {
      cur_cons_pkt_it++;
    }
    if (cur_cons_pkt_it != (*cur_fec_packet_it)->maybe_loss_packets.end() && (*cur_cons_pkt_it).lock()->seq_num == (*CMLP_it)->seq_num) {
      CMLP.pop_front();
    }
    else {
      return false;
    }

    // std::cout<<"============"<<std::endl;
    // std::cout<<"Current fec_packet:"<<std::endl;
    // auto select_fec_packet_it = select_fec_packet.cbegin();
    // while (select_fec_packet_it != select_fec_packet.end()) {
    //   std::cout<<(*select_fec_packet_it)->seq_num<<" ";
    //   select_fec_packet_it++;
    // }
    // std::cout<<std::endl;
    // std::cout<<"Current consider_loss_packet:"<<std::endl;
    // select_cons_loss_packet_it = select_cons_loss_packet.cbegin();
    // while (select_cons_loss_packet_it != select_cons_loss_packet.end()) {
    //   std::cout<<(*select_cons_loss_packet_it)->seq_num<<" ";
    //   select_cons_loss_packet_it++;
    // }
    // std::cout<<std::endl;
    // std::cout<<"Current CMLP:"<<std::endl;
    // CMLP_it = CMLP.cbegin();
    // while (CMLP_it != CMLP.end()) {
    //   std::cout<<(*CMLP_it)->seq_num<<" ";
    //   CMLP_it++;
    // }
    // std::cout<<std::endl;
    
    if (CMLP.empty()) {
      break;
    }
    else {
      cur_fec_packet_it++;
    }
  }

  if (CMLP.empty() && select_fec_packet.size()!=0 && select_fec_packet.size() == select_cons_loss_packet.size()) {
    return true;
  }
  else {
    return false;
  }
}

bool RSForwardErrorCorrection::DfsFECPacket(std::vector<std::shared_ptr<ReceivedFecPacket>>& select_fec_packet, std::vector<std::shared_ptr<ConsiderLossPacket>>& select_cons_loss_packet, std::vector<uint16_t>& select_fec_pkt_seq_num, std::shared_ptr<ReceivedFecPacket> cur_fec_packet, uint16_t search_loss_seq_num) {
  
  // std::cout<<"\t[DfsFECPacket]SearcnSN:"<<search_loss_seq_num<< "  cur_seq_num="<<  cur_fec_packet->seq_num<<"  select_fec_packet.size()="<<select_fec_packet.size()<<"  select_cons_loss_packet.size()"<<select_cons_loss_packet.size()<<std::endl; 
  
  if (cur_fec_packet->if_used) return false;
  cur_fec_packet->if_used = true;
  DeleteUnavailableWeakPtr(&cur_fec_packet->maybe_loss_packets);
  auto maybe_loss_pkt_it = cur_fec_packet->maybe_loss_packets.cbegin();
  uint16_t select_fec_packet_num = select_fec_packet.size();
  uint16_t select_cons_loss_packet_num = select_cons_loss_packet.size(); 
  bool dfs_flag=true;
  while (maybe_loss_pkt_it != cur_fec_packet->maybe_loss_packets.end()) {
    if (!DfsConsiderLossPacket(select_fec_packet, select_cons_loss_packet, select_fec_pkt_seq_num, (*maybe_loss_pkt_it).lock(), search_loss_seq_num)) {
      dfs_flag=false;
      break;
    }
    maybe_loss_pkt_it++;
  }
  if (dfs_flag) {
    select_fec_packet.push_back(cur_fec_packet);
    return true;
  }

  while (select_fec_packet.size()>select_fec_packet_num) {
    std::shared_ptr<ReceivedFecPacket> per_select_fec_pkt = select_fec_packet.back();
    per_select_fec_pkt->if_used = false;
    select_fec_packet.pop_back();
  }
  while (select_cons_loss_packet.size()>select_cons_loss_packet_num) {
    std::shared_ptr<ConsiderLossPacket> per_select_cons_pkt = select_cons_loss_packet.back();
    per_select_cons_pkt->if_used = false;
    select_cons_loss_packet.pop_back();
    select_fec_pkt_seq_num.pop_back();
  }
  cur_fec_packet->if_used = false;
  return false;
}

bool RSForwardErrorCorrection::DfsConsiderLossPacket(std::vector<std::shared_ptr<ReceivedFecPacket>>& select_fec_packet, std::vector<std::shared_ptr<ConsiderLossPacket>>& select_cons_loss_packet, std::vector<uint16_t>& select_fec_pkt_seq_num, std::shared_ptr<ConsiderLossPacket> cur_cons_loss_packet, uint16_t search_loss_seq_num) {
  
  // std::cout<<"\t[DfsConsiderLossPacket]SearcnSN:"<<search_loss_seq_num<<  "  cur_seq_num="<<  cur_cons_loss_packet->seq_num<<"  select_fec_packet.size()="<<select_fec_packet.size()<<"  select_cons_loss_packet.size()"<<select_cons_loss_packet.size()<<std::endl;
  
  if (cur_cons_loss_packet->if_used) return true;
  cur_cons_loss_packet->if_used = true;
  DeleteUnavailableWeakPtr(&cur_cons_loss_packet->reference_fec_packets);
  auto ref_fec_pkt_it = cur_cons_loss_packet->reference_fec_packets.cbegin();
  uint16_t select_fec_packet_num = select_fec_packet.size();
  uint16_t select_cons_loss_packet_num = select_cons_loss_packet.size(); 
  while (ref_fec_pkt_it != cur_cons_loss_packet->reference_fec_packets.end()) {
    bool fec_search_flag = true;
    uint16_t fec_seq_num_base = (*ref_fec_pkt_it).lock()->protected_streams[0].seq_num_base;
    if (IsNewerSequenceNumber(fec_seq_num_base, search_loss_seq_num)) {
      fec_search_flag=false;
    }
    uint16_t seq_num_diff = MinDiff(fec_seq_num_base, search_loss_seq_num);
    if ((*ref_fec_pkt_it).lock()->protected_streams[0].mask[seq_num_diff] == 0) {
      fec_search_flag=false;
    }

    if (fec_search_flag && DfsFECPacket(select_fec_packet, select_cons_loss_packet, select_fec_pkt_seq_num, (*ref_fec_pkt_it).lock(), search_loss_seq_num)) {
      select_cons_loss_packet.push_back(cur_cons_loss_packet);
      select_fec_pkt_seq_num.push_back(cur_cons_loss_packet->seq_num);
      
      // std::cout<<"\t\tselect_cons_loss_packet.size()="<<select_cons_loss_packet.size()<<std::endl;
      // std::cout<<"\t\tSearchSN:"<<search_loss_seq_num<<"  We dfs fecPackets "<<(*ref_fec_pkt_it).lock()->seq_num<<" to decode "<<cur_cons_loss_packet->seq_num<<std::endl;

      return true;
    }
    ref_fec_pkt_it++;
  }
  while (select_fec_packet.size()>select_fec_packet_num) {
    std::shared_ptr<ReceivedFecPacket> per_select_fec_pkt = select_fec_packet.back();
    per_select_fec_pkt->if_used = false;
    select_fec_packet.pop_back();
  }
  while (select_cons_loss_packet.size()>select_cons_loss_packet_num) {
    std::shared_ptr<ConsiderLossPacket> per_select_cons_pkt = select_cons_loss_packet.back();
    per_select_cons_pkt->if_used = false;
    select_cons_loss_packet.pop_back();
    select_fec_pkt_seq_num.pop_back();
  }
  cur_cons_loss_packet->if_used = false;
  return false;
}

int RSForwardErrorCorrection::NumCoveredPacketsMissing(
    const ReceivedFecPacket& fec_packet) {
  int packets_missing = 0;
  for (const auto& protected_packet : fec_packet.protected_packets) {
    if (protected_packet->pkt == nullptr) {
      ++packets_missing;
      if (packets_missing > 1) {
        break;  // We can't recover more than one packet.
      }
    }
  }
  return packets_missing;
}

void RSForwardErrorCorrection::DiscardOldRecoveredPackets(
    RecoveredPacketList* recovered_packets) {
  const size_t max_media_packets = fec_header_reader_->MaxMediaPackets();
  while (recovered_packets->size() > max_media_packets) {
    recovered_packets->pop_front();
  }
  RTC_DCHECK_LE(recovered_packets->size(), max_media_packets);
}

bool RSForwardErrorCorrection::IsOldFecPacket(
    const ReceivedFecPacket& fec_packet,
    const RecoveredPacketList* recovered_packets) {
  if (recovered_packets->empty()) {
    return false;
  }

  const uint16_t back_recovered_seq_num = recovered_packets->back()->seq_num;
  const uint16_t last_protected_seq_num =
      fec_packet.protected_packets.back()->seq_num;

  // FEC packet is old if its last protected sequence number is much
  // older than the latest protected sequence number received.
  return (MinDiff(back_recovered_seq_num, last_protected_seq_num) >
          kOldSequenceThreshold);
}

uint16_t RSForwardErrorCorrection::ParseSequenceNumber(const uint8_t* packet) {
  return (packet[2] << 8) + packet[3];
}

uint32_t RSForwardErrorCorrection::ParseSsrc(const uint8_t* packet) {
  return (packet[8] << 24) + (packet[9] << 16) + (packet[10] << 8) + packet[11];
}

RSForwardErrorCorrection::DecodeFecResult RSForwardErrorCorrection::DecodeRSFec(
        const ReceivedPacket& received_packet, RecoveredPacketList* recovered_packets) {
  RTC_DCHECK(recovered_packets);
  const size_t max_media_packets = fec_header_reader_->MaxMediaPackets();
  if (recovered_packets->size() == max_media_packets) {
    const RecoveredPacket* back_recovered_packet = recovered_packets->back().get();

    if (received_packet.ssrc == back_recovered_packet->ssrc) {
      const unsigned int seq_num_diff = MinDiff(received_packet.seq_num, back_recovered_packet->seq_num);
      if (seq_num_diff > max_media_packets) {
        // A big gap in sequence numbers. The old recovered packets
        // are now useless, so it's safe to do a reset.
        RTC_LOG(LS_INFO) << "Big gap in media/ULPFEC sequence numbers. No need "
                            "to keep the old packets in the FEC buffers, thus "
                            "resetting them.";
        ResetState(recovered_packets);
      }
    }
  }

  InsertPacket(received_packet, recovered_packets);

  // if (received_packet.is_fec) {
  //   std::cout<<"=======================We Successful receive a RSFec Packet! Seq_num:"<<received_packet.seq_num<<std::endl;
  // }
  
  DecodeFecResult decode_result;
  while (true) {
    int ret = AttemptRecovery(recovered_packets);
    if (ret == 0) break;
    if (ret > 0) decode_result.num_recovered_packets += ret;
  }
  return decode_result;
}

size_t RSForwardErrorCorrection::MaxPacketOverhead() const {
  return fec_header_writer_->MaxPacketOverhead();
}

//
// Implementation for RSFecHeaderReader
// 

RSFecHeaderReader::RSFecHeaderReader(size_t max_media_packets,
                                     size_t max_fec_packets,
                                     size_t max_consider_loss_packets)
    : max_media_packets_(max_media_packets),
      max_fec_packets_(max_fec_packets),
      max_consider_loss_packets_(max_consider_loss_packets) {}

RSFecHeaderReader::~RSFecHeaderReader() = default;

size_t RSFecHeaderReader::MaxMediaPackets() const {
  return max_media_packets_;
}

size_t RSFecHeaderReader::MaxFecPackets() const {
  return max_fec_packets_;
}

size_t RSFecHeaderReader::MaxConsiderLossPackets() const {
  return max_consider_loss_packets_;
}

//
// Implementation for RSFecHeaderWriter
// 

RSFecHeaderWriter::RSFecHeaderWriter(size_t max_media_packets,
                                     size_t max_fec_packets,
                                     size_t max_consider_loss_packets,
                                     size_t max_packet_overhead)
    : max_media_packets_(max_media_packets),
      max_fec_packets_(max_fec_packets),
      max_consider_loss_packets_(max_consider_loss_packets),
      max_packet_overhead_(max_packet_overhead) {}

RSFecHeaderWriter::~RSFecHeaderWriter() = default;

size_t RSFecHeaderWriter::MaxMediaPackets() const {
    return max_media_packets_;
}

size_t RSFecHeaderWriter::MaxFecPackets() const {
    return max_fec_packets_;
}

size_t RSFecHeaderWriter::MaxConsiderLossPackets() const {
  return max_consider_loss_packets_;
}


size_t RSFecHeaderWriter::MaxPacketOverhead() const {
    return max_packet_overhead_;
}

}  // namespace webrtc