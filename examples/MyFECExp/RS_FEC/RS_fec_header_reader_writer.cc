/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/MyFECExp/RS_FEC/RS_fec_header_reader_writer.h"

#include <string.h>
#include <iostream>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "examples/MyFECExp/RS_FEC/RS_forward_error_correction_internal.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Maximum number of media packets that can be protected in one batch.
constexpr size_t kMaxMediaPackets = 32;

// Maximum number of media packets tracked by FEC decoder.
// Maintain a sufficiently larger tracking window than `kMaxMediaPackets`
// to account for packet reordering in pacer/ network.
constexpr size_t kMaxTrackedMediaPackets = 4 * kMaxMediaPackets;

// Maximum number of FEC packets stored inside ForwardErrorCorrection.
constexpr size_t kMaxFecPackets = 32;

constexpr size_t kMaxConsiderLossPackets = kMaxMediaPackets*4;

// FEC Level 0 header size in bytes.
constexpr size_t kFecLevel0HeaderSize = 10;

// FEC Level 1 (ULP) header size in bytes.
constexpr size_t kFecLevel1HeaderSize = 2 + kRSfecPacketMaskSize + kRSfecMagicNumSize;

constexpr size_t kPacketMaskOffset = kFecLevel0HeaderSize + 2;

size_t RSfecHeaderSize() {
  return kFecLevel0HeaderSize + kFecLevel1HeaderSize;
}

}  // namespace

//
// Implementation for RSfecHeaderReader
//

RSfecHeaderReader::RSfecHeaderReader()
    : RSFecHeaderReader(kMaxTrackedMediaPackets, kMaxFecPackets, kMaxConsiderLossPackets) {}

RSfecHeaderReader::~RSfecHeaderReader() = default;

bool RSfecHeaderReader::ReadFecHeader(RSForwardErrorCorrection::ReceivedFecPacket* fec_packet) const {
  uint8_t* data = fec_packet->pkt->data.MutableData();
  if (fec_packet->pkt->data.size() < kPacketMaskOffset) {
    return false;  // Truncated packet.
  }

  size_t packet_mask_size = kRSfecPacketMaskSize;
  fec_packet->fec_header_size = RSfecHeaderSize();
  uint16_t seq_num_base = ByteReader<uint16_t>::ReadBigEndian(&data[2]);
  uint16_t cur_base_num = ByteReader<uint16_t>::ReadBigEndian(&data[16]);
  uint32_t mask_uint = ByteReader<uint32_t>::ReadBigEndian(&data[12]);
  std::bitset<32> mask = std::bitset<32>(mask_uint);

  // std::cout<<"\t\t===Read mask:"<<std::endl;
  // std::cout<<mask<<std::endl;

  std::bitset<32> arrive = std::bitset<32>(0);
  fec_packet->protected_streams = {{.ssrc = fec_packet->ssrc,  // Due to RED.
                                    .seq_num_base = seq_num_base,
                                    .cur_base_num = cur_base_num,
                                    .packet_mask_offset = kPacketMaskOffset,
                                    .mask = mask,
                                    .arrive = arrive,
                                    .packet_mask_size = packet_mask_size}};
  fec_packet->protection_length = ByteReader<uint16_t>::ReadBigEndian(&data[10]);

  // Store length recovery field in temporary location in header.
  // This makes the header "compatible" with the corresponding
  // FlexFEC location of the length recovery field, thus simplifying
  // the XORing operations.
  memcpy(&data[2], &data[8], 2);
 
  return true;
}

//
// Implementation for RSfecHeaderWriter!
//

RSfecHeaderWriter::RSfecHeaderWriter()
    : RSFecHeaderWriter(kMaxMediaPackets,
                      kMaxFecPackets,
                      kMaxConsiderLossPackets,
                      kFecLevel0HeaderSize + kFecLevel1HeaderSize) {}

RSfecHeaderWriter::~RSfecHeaderWriter() = default;

// TODO(brandtr): Consider updating this implementation (which actually
// returns a bound on the sequence number spread), if logic is added to
// UlpfecHeaderWriter::FinalizeFecHeader to truncate packet masks which end
// in a string of zeroes. (Similar to how it is done in the FlexFEC case.)
size_t RSfecHeaderWriter::MinPacketMaskSize(const uint8_t* packet_mask,
                                             size_t packet_mask_size) const {
  //TODO:Maybe I can cancel this Function!
  return kRSfecPacketMaskSize;
}

size_t RSfecHeaderWriter::FecHeaderSize() const {
  return RSfecHeaderSize();
}

void RSfecHeaderWriter::FinalizeFecHeader(
    rtc::ArrayView<const ProtectedStream> protected_streams,
    RSForwardErrorCorrection::Packet& fec_packet) const {
  RTC_CHECK_EQ(protected_streams.size(), 1);
  uint16_t seq_num_base = protected_streams[0].seq_num_base;
  uint16_t cur_base_num = protected_streams[0].cur_base_num;
  std::bitset<32> mask = protected_streams[0].mask;

  // std::cout<<"\t\t===Write mask:"<<std::endl;
  // std::cout<<mask<<std::endl;

  uint8_t* data = fec_packet.data.MutableData();
  // We can't set E bit to zero!
  // L bit is no longer used in RS-FEC. 
  // data[0] &= 0x7f;
  
  // Copy length recovery field from temporary location.
  memcpy(&data[8], &data[2], 2);
  // Write sequence number base.
  ByteWriter<uint16_t>::WriteBigEndian(&data[2], seq_num_base);
  // Protection length is set to entire packet. (This is not
  // required in general.)
  const size_t fec_header_size = FecHeaderSize();
  ByteWriter<uint16_t>::WriteBigEndian(&data[10], fec_packet.data.size() - fec_header_size);
  // Write Magic Number
  ByteWriter<uint16_t>::WriteBigEndian(&data[16], cur_base_num);
  // Write the packet mask.
  ByteWriter<uint32_t>::WriteBigEndian(&data[12], mask.to_ulong());
}

}  // namespace webrtc
