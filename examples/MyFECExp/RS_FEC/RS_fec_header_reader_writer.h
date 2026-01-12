/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef EXAMPLES_MYFECEXP_RSFEC_RS_FEC_HEADER_READER_WRITER_H_
#define EXAMPLES_MYFECEXP_RSFEC_RS_FEC_HEADER_READER_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include "examples/MyFECExp/RS_FEC/RS_forward_error_correction.h"

namespace webrtc {

// FEC Level 0 Header, 10 bytes.
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |E|L|P|X|  CC   |M| PT recovery |            SN base            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          TS recovery                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |        length recovery        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// FEC Level 1 Header, 8 bytes.
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |       Protection Length       |             mask              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |             mask              |          magic number         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class RSfecHeaderReader : public RSFecHeaderReader {
 public:
  RSfecHeaderReader();
  ~RSfecHeaderReader() override;

  bool ReadFecHeader(RSForwardErrorCorrection::ReceivedFecPacket* fec_packet) const override;
};

class RSfecHeaderWriter : public RSFecHeaderWriter {
 public:
  RSfecHeaderWriter();
  ~RSfecHeaderWriter() override;

  size_t MinPacketMaskSize(const uint8_t* packet_mask,
                           size_t packet_mask_size) const override;

  size_t FecHeaderSize() const override;

  void FinalizeFecHeader(
      rtc::ArrayView<const ProtectedStream> protected_streams,
      RSForwardErrorCorrection::Packet& fec_packet) const override;
};

}  // namespace webrtc

#endif  // EXAMPLES_MYFECEXP_RSFEC_RS_FEC_HEADER_READER_WRITER_H_
