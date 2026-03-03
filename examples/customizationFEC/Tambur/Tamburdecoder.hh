#ifndef DECODER_HH
#define DECODER_HH

#include <map>
#include <vector>
#include <optional>
#include <string>
#include <memory>

#include "examples/customizationFEC/Tambur/src/fec/fec_multi_receiver.hh"
#include "examples/customizationFEC/Tambur/protocol.hh"
#include "modules/video_coding/packet_buffer.h"

class VideoFrame
{
public:
    VideoFrame(uint32_t frame_id, FrameType frame_type, uint16_t frag_cnt, uint32_t rtp_timestamp = 0, uint8_t payload_type = 0);
    
    // Check if the frame has a specific fragment
    bool has_frag(uint16_t frag_id) const;
    
    // Get fragment by ID
    Datagram& get_frag(uint16_t frag_id);
    const Datagram& get_frag(uint16_t frag_id) const;
    
    // Get complete frame data
    std::string get_frame();
    
    // Insert fragments with packet information
    void insert_frag(const Datagram& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet);
    void insert_frag(Datagram&& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet);
    
    // Check if frame is complete
    bool complete() const;
    
    // Get frame size if complete
    std::optional<size_t> frame_size() const;
    
    // Accessors
    uint32_t id() const { return id_; }
    FrameType type() const { return type_; }
    uint32_t rtp_timestamp() const { return rtp_timestamp_; }
    uint8_t payload_type() const { return payload_type_; }
    
    // Get packet information
    const std::vector<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>>& packets() const { return packets_; }
    std::optional<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> first_packet() const;
    std::optional<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> last_packet() const;
    
    // Receive time tracking
    int64_t first_receive_time() const { return first_receive_time_; }
    int64_t last_receive_time() const { return last_receive_time_; }

    // FEC related methods
    bool has_fec_frame(uint16_t fec_frame_num) const;
    void add_fec_frame(uint16_t fec_frame_num, FrameType frame_type,
                      uint8_t num_fec_frames, std::string& frame);
    
    // Type management
    bool type_is_set() const { return type_ != FrameType::DUMMY; }

    // Fragment access
    std::vector<std::optional<Datagram>>& frags() { return frags_; }
    const std::vector<std::optional<Datagram>>& frags() const { return frags_; }
    
    unsigned int null_frags() const { return null_frags_; }

private:
    void validate_datagram(const Datagram& datagram) const;
    
    int64_t first_receive_time_;  // first packet receive time
    int64_t last_receive_time_;   // last packet receive time

    uint32_t id_;           // frame ID
    FrameType type_;        // frame type
    uint32_t rtp_timestamp_; // RTP timestamp
    uint8_t payload_type_;  // RTP payload type
    
    std::vector<std::optional<Datagram>> frags_;  // fragments of this frame
    unsigned int null_frags_;                     // number of uninitialized fragments
    size_t frame_size_{0};                        // frame size so far
    
    // Packet storage
    std::vector<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> packets_; // stored packets
    
    uint8_t num_fec_frames_{0};                   // number of FEC frames
    std::map<uint16_t, std::string> fec_frames_; // FEC recovered frames
    std::string frame_val_;                      // cached complete frame data
};

class TamburDecoder
{
  public:
    TamburDecoder(FECMultiReceiver* fECMultiReceiver = nullptr);
    ~TamburDecoder() = default;
    
    // Add a received datagram with packet information
    bool add_datagram(const Datagram & datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet, uint8_t retrans);
    bool add_datagram(Datagram && datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet, uint8_t retrans);

    // Check if next frame is complete
    bool has_next_frame() const;

    // Get the next complete frame data
    std::optional<std::string> get_next_frame();

    // Get frame information for WebRTC integration
    struct FrameInfo {
        uint32_t frame_id;
        FrameType frame_type;
        size_t frame_size;
        bool is_key_frame;
        uint32_t rtp_timestamp; // Add RTP timestamp
        uint8_t payload_type;    // Add payload type from RTP packet
        int64_t min_receive_time; // minimum packet receive time
        int64_t max_receive_time; // maximum packet receive time
    };

    // Get next frame with timestamp and packet information
    struct TimestampedFrame {
        std::string frame_data;
        uint32_t rtp_timestamp;
        FrameInfo frame_info;
        std::vector<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> packets; // stored packets
        std::optional<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> first_packet; // first packet of frame
        std::optional<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> last_packet;  // last packet of frame
    };
    
    std::optional<TimestampedFrame> get_next_timestamped_frame();

    // FEC related functions
    bool fec_feedback_ready() const;
    FECMsg get_fec_feedback();

    // Cleanup
    void cleanup_old_frames(uint32_t current_frame_id);

    // Statistics
    uint32_t get_next_frame_id() const { return next_frame_; }
    size_t get_total_frames_assembled() const { return total_frames_assembled_; }

  private:

    // Common code for adding datagrams
    bool add_datagram_common(const Datagram& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet);

    // Update FEC recovered frames
    void update_fec_recovered_frames();

    // Check if a frame is complete
    bool is_frame_complete(uint32_t frame_id) const;

    // Pad dummy frames for continuity
    void pad_dummy_frames(uint32_t frame_id);

    // Advance to next frame
    void advance_next_frame();

    // Clean up old frames
    void clean_up_to(uint32_t frontier);

    FECMultiReceiver* fec_multi_receiver_;
    uint32_t next_frame_{0};
    size_t total_frames_assembled_{0};

    // Frame buffer: frame_id -> VideoFrame
    std::map<uint32_t, VideoFrame> frame_buf_;
    
    // Track which frames have been processed
    std::map<uint32_t, bool> processed_frames_;
};

#endif // DECODER_HH