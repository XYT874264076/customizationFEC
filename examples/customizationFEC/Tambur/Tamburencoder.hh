
#ifndef ENCODER_HH
#define ENCODER_HH

#include <vector>

#include "examples/customizationFEC/Tambur/src/fec/fec_sender.hh"
#include "examples/customizationFEC/Tambur/protocol.hh"

class TamburEncoder
{
  public:
    TamburEncoder(FECSender* fECSender = nullptr);
    ~TamburEncoder() = default;

    std::vector<Datagram> encode(std::vector<uint8_t> data, 
                                 size_t frame_size,
                                 FrameType frame_type,
                                 uint16_t rel_num_frames);
  private:
    FECSender * fECSender_;
    uint32_t frame_id_ = 0;

};

#endif //ENCODER_HH