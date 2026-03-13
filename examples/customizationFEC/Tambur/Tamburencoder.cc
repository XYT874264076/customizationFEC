
#include "examples/customizationFEC/Tambur/Tamburencoder.hh"

TamburEncoder::TamburEncoder(FECSender* fECSender)
    : fECSender_(fECSender) {
        frame_id_ = 0;
    }

std::vector<Datagram> TamburEncoder::encode(std::vector<uint8_t> data, 
                                 size_t frame_size,
                                 FrameType frame_type,
                                 uint16_t rel_num_frames) {

    std::deque<std::pair<uint16_t, FECDatagram>> temp_pkts;
    std::vector<Datagram> datagrams;
    uint16_t frag = 0;

    uint8_t* data_ptr = data.data();

    for (uint64_t j = 0; j < rel_num_frames; j++) {
        uint16_t sz = uint16_t(frame_size / rel_num_frames) +
                      uint16_t(j < (frame_size % rel_num_frames));
        const auto pkts = fECSender_->next_frame(sz, data_ptr,
                                                 frame_id_, (uint8_t)frame_type, (uint8_t)rel_num_frames);
        
        //Write file!!
        if (pkts.size() > 0){
            uint32_t frame_num = pkts.at(0).frame_num;
            auto nowtime = std::chrono::system_clock::now();
            auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(nowtime.time_since_epoch()).count();
            std::fstream TamburEncoder(inputV::Params::output+"TamburEncoder.csv",std::ios::app);
            TamburEncoder << milliseconds_since_epoch << "," << frame_num << "," << pkts.size() << std::endl;
            TamburEncoder.close();
        }
        
        data_ptr += sz;
        for (auto pkt : pkts)
        {
          temp_pkts.emplace_back(std::make_pair(frag, std::move(pkt)));
          frag++;
        }
    }

    const uint16_t frag_cnt = temp_pkts.size();
    for (uint16_t pos = 0; pos < frag_cnt; pos++)
      {
        std::string packet = temp_pkts.front().second.serialize_to_string();
        auto frag_id = temp_pkts.front().first;
        temp_pkts.pop_front();
        assert(packet.size() <= Datagram::max_payload);
        // enqueue a datagram
        datagrams.emplace_back(Datagram{frame_id_, frame_type, frag_id,
                                        frag, std::string_view{packet}});
      }
    
    frame_id_ ++;

    return datagrams;
}