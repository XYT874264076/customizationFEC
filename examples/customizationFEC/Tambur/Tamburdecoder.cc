#include "examples/customizationFEC/Tambur/Tamburdecoder.hh"

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <optional>
#include <utility>
#include <cassert>

using namespace std;

VideoFrame::VideoFrame(const uint32_t frame_id,
                       const FrameType frame_type,
                       const uint16_t frag_cnt,
                       uint32_t rtp_timestamp,
                       uint8_t payload_type)
    : first_receive_time_(0), last_receive_time_(0), id_(frame_id), type_(frame_type), rtp_timestamp_(rtp_timestamp), payload_type_(payload_type),
      frags_(frag_cnt), null_frags_(frag_cnt), frame_size_(0)
{
    assert(frag_cnt > 0 && "frame cannot have zero fragments");
}



bool VideoFrame::has_frag(const uint16_t frag_id) const
{
    return frags_.at(frag_id).has_value();
}

Datagram& VideoFrame::get_frag(const uint16_t frag_id)
{
    return frags_.at(frag_id).value();
}

const Datagram& VideoFrame::get_frag(const uint16_t frag_id) const
{
    return frags_.at(frag_id).value();
}

bool VideoFrame::complete() const
{
    return (num_fec_frames_ > 0) and (num_fec_frames_ == fec_frames_.size());
}

std::optional<size_t> VideoFrame::frame_size() const
{
    if (not complete()) {
        return nullopt;
    }
    std::optional<size_t> frame_size = 0;
    for (auto const &[key, val] : fec_frames_)
    {
        frame_size.value() += val.size();
    }
    return frame_size;
}

std::string VideoFrame::get_frame()
{
    if (frame_val_.size() > 0)
    {
        return frame_val_;
    }
    
    size_t the_frame_size = frame_size().value();
    frame_val_.reserve(the_frame_size);
    size_t last_key = 0;
    for (auto const &[key, val] : fec_frames_)
    {
        assert(key > last_key && "FEC frames must be in order");
        last_key = key;
        frame_val_.append(val);
    }
    return frame_val_;
}

void VideoFrame::validate_datagram(const Datagram& datagram) const
{
    assert(datagram.frame_id == id_ && "unable to insert an incompatible datagram");
    assert(datagram.frame_type == type_ && "unable to insert an incompatible datagram");
    assert(datagram.frag_id < frags_.size() && "unable to insert an incompatible datagram");
    assert(datagram.frag_cnt == frags_.size() && "unable to insert an incompatible datagram");
}

std::optional<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> VideoFrame::first_packet() const
{
    if (packets_.empty()) {
        return std::nullopt;
    }
    
    // Find the packet with the smallest sequence number
    auto it = std::min_element(packets_.begin(), packets_.end(),
        [](const auto& a, const auto& b) {
            return a->sequence_number < b->sequence_number;
        });
    
    return *it;
}

std::optional<std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet>> VideoFrame::last_packet() const
{
    if (packets_.empty()) {
        return std::nullopt;
    }
    
    // Find the packet with the largest sequence number
    auto it = std::max_element(packets_.begin(), packets_.end(),
        [](const auto& a, const auto& b) {
            return a->sequence_number < b->sequence_number;
        });
    
    return *it;
}

void VideoFrame::insert_frag(const Datagram& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet)
{
    validate_datagram(datagram);

    // insert only if the datagram does not exist yet
    if (not frags_[datagram.frag_id]) {
        frame_size_ += datagram.payload.size();
        null_frags_--;
        frags_[datagram.frag_id] = datagram;
        
        // Store packet information and update receive time
        if (packet) {
            packets_.push_back(packet);
            
            // Get current time as receive time
            int64_t current_time = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
            
            // Update first_receive_time if this is the first packet
            if (first_receive_time_ == 0) {
                first_receive_time_ = current_time;
            }
            
            // Always update last_receive_time to the latest packet
            last_receive_time_ = current_time;
        }
    }
}

void VideoFrame::insert_frag(Datagram&& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet)
{
    validate_datagram(datagram);

    // insert only if the datagram does not exist yet
    if (not frags_[datagram.frag_id]) {
        frame_size_ += datagram.payload.size();
        null_frags_--;
        frags_[datagram.frag_id] = std::move(datagram);
        
        // Store packet information
        if (packet) {
            packets_.push_back(packet);

            // Get current time as receive time
            int64_t current_time = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
            
            // Update first_receive_time if this is the first packet
            if (first_receive_time_ == 0) {
                first_receive_time_ = current_time;
            }
            
            // Always update last_receive_time to the latest packet
            last_receive_time_ = current_time;
        }
    }
}

void VideoFrame::add_fec_frame(uint16_t fec_frame_num, FrameType frameType,
                               uint8_t num_fec_frames, string& frame)
{
    if (not fec_frames_.count(fec_frame_num))
    {
        fec_frames_[fec_frame_num] = std::move(frame);
    }
    type_ = frameType;
    num_fec_frames_ = num_fec_frames;
}

bool VideoFrame::has_fec_frame(uint16_t fec_frame_num) const
{
    return fec_frames_.count(fec_frame_num) > 0;
}



TamburDecoder::TamburDecoder(FECMultiReceiver* fECMultiReceiver)
    : fec_multi_receiver_(fECMultiReceiver) {

    }

bool TamburDecoder::add_datagram_common(const Datagram& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet)
{
    if (not packet) {
        return false;
    }
    
    const auto frame_id = datagram.frame_id;
    const auto frame_type = datagram.frame_type;
    const auto frag_cnt = datagram.frag_cnt;

    // printf("\t\t\t === Current frame_id: %u, next_frame_: %u \n", frame_id, next_frame_);

    // Ignore any datagrams from old frames
    if (frame_id < next_frame_) {
        return false;
    }

    if (not frame_buf_.count(frame_id)) {
        pad_dummy_frames(frame_id);
        // Initialize a Frame instance for frame 'frame_id' with packet information
        frame_buf_.emplace(std::piecewise_construct,
                           std::forward_as_tuple(frame_id),
                           std::forward_as_tuple(frame_id, frame_type, frag_cnt, packet->timestamp, packet->payload_type));
    }
    return true;
}

bool TamburDecoder::add_datagram(const Datagram& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet, uint8_t retrans)
{
    if (not packet) {
        return false;
    }

    if (not add_datagram_common(datagram, packet)) {
        return false;
    }
    
    if (fec_multi_receiver_) {
        fec_multi_receiver_->receive_pkt(datagram.payload, retrans);
    }
    
    // Insert the datagram into the VideoFrame with packet information
    frame_buf_.at(datagram.frame_id).insert_frag(datagram, packet);
    
    // Verify FEC and check for complete frames
    update_fec_recovered_frames();
    
    return true;
}

bool TamburDecoder::add_datagram(Datagram&& datagram, std::shared_ptr<const webrtc::video_coding::PacketBuffer::Packet> packet, uint8_t retrans)
{
    if (not packet) {
        return false;
    }

    if (not add_datagram_common(datagram, packet)) {
        return false;
    }
    
    if (fec_multi_receiver_) {
        fec_multi_receiver_->receive_pkt(datagram.payload, retrans);
    }
    
    // Insert the datagram into the VideoFrame with packet information
    frame_buf_.at(datagram.frame_id).insert_frag(std::move(datagram), packet);
    
    // Verify FEC and check for complete frames
    update_fec_recovered_frames();
    
    return true;
}

void TamburDecoder::update_fec_recovered_frames()
{
    if (not fec_multi_receiver_) {
        return;
    }
    
    std::vector<std::pair<uint16_t, uint32_t>> fec_video_recovered_frames =
        fec_multi_receiver_->recovered_video_frames();
    
    for (auto el : fec_video_recovered_frames)
    {
        auto fec_frame = el.first;
        auto video_frame = el.second;
        
        if (video_frame < next_frame_ or 
            (fec_frame < fec_multi_receiver_->get_num_frames()))
        {
            continue;
        }
        
        auto video_frame_info = fec_multi_receiver_->video_frame_info(fec_frame);
        if (not frame_buf_.count(video_frame)) {
            pad_dummy_frames(video_frame + 1);
        }
        
        VideoFrame& frame = frame_buf_.at(video_frame);
        if (not frame.has_fec_frame(fec_frame))
        {
            // Check if the FEC frame is still recoverable (not expired in StreamingCode)
            auto is_recovered = fec_multi_receiver_->is_frame_recovered(fec_frame);
            if (!is_recovered.has_value() || !is_recovered.value()) {
                continue;
            }
            std::string s = fec_multi_receiver_->recovered_frame(fec_frame);
            frame.add_fec_frame(fec_frame, FrameType{std::get<1>(video_frame_info)},
                                std::get<2>(video_frame_info), s);
        }
    }
}

bool TamburDecoder::has_next_frame() const
{
    // Check if the next frame to expect is complete
    auto it = frame_buf_.find(next_frame_);
    if (it != frame_buf_.end() and it->second.complete()) {
        return true;
    }

    // Seek forward if a key frame in the future is already complete
    for (auto it = frame_buf_.rbegin(); it != frame_buf_.rend(); it++) {
        const auto frame_id = it->first;
        const auto& frame = it->second;

        // Found a complete key frame ahead of next_frame_
        if (frame.type() == FrameType::KEY and frame.complete()) {
            assert(frame_id > next_frame_);
            return true;
        }
    }

    return false;
}

std::optional<std::string> TamburDecoder::get_next_frame()
{
    if (not has_next_frame()) {
        return nullopt;
    }

    VideoFrame& frame = frame_buf_.at(next_frame_);
    if (not frame.complete()) {
        return nullopt;
    }

    string frame_data = frame.get_frame();
    
    // Update statistics
    total_frames_assembled_++;
    
    // Move to next frame
    advance_next_frame();
    
    return frame_data;
}

std::optional<TamburDecoder::TimestampedFrame> TamburDecoder::get_next_timestamped_frame()
{
    if (not has_next_frame()) {
        return nullopt;
    }

    // If next_frame_ is not complete (lost and unrecoverable), skip forward
    // to the nearest complete key frame.
    {
        auto it = frame_buf_.find(next_frame_);
        if (it == frame_buf_.end() or not it->second.complete()) {
            // Find the nearest complete key frame ahead of next_frame_
            uint32_t skip_to = 0;
            bool found = false;
            for (auto rit = frame_buf_.begin(); rit != frame_buf_.end(); ++rit) {
                if (rit->first > next_frame_ &&
                    rit->second.type() == FrameType::KEY &&
                    rit->second.complete()) {
                    skip_to = rit->first;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return nullopt;
            }
            printf("\t\t\t skip lost frames: next_frame_=%u -> %u\n", next_frame_, skip_to);
            // Advance next_frame_ to the key frame, discarding lost frames
            while (next_frame_ < skip_to) {
                advance_next_frame();
            }
        }
    }

    VideoFrame& frame = frame_buf_.at(next_frame_);
    if (not frame.complete()) {
        return nullopt;
    }

    TimestampedFrame result;
    result.frame_data = frame.get_frame();
    result.rtp_timestamp = frame.rtp_timestamp();
    
    // Fill frame info
    result.frame_info.frame_id = frame.id();
    result.frame_info.frame_type = frame.type();
    result.frame_info.frame_size = frame.frame_size().value();
    result.frame_info.is_key_frame = (frame.type() == FrameType::KEY);
    result.frame_info.rtp_timestamp = frame.rtp_timestamp();
    result.frame_info.payload_type = frame.payload_type();
    result.frame_info.min_receive_time = frame.first_receive_time();
    result.frame_info.max_receive_time = frame.last_receive_time();
    
    // Fill packet information
    result.packets = frame.packets();
    result.first_packet = frame.first_packet();
    result.last_packet = frame.last_packet();
    
    // Update statistics
    total_frames_assembled_++;
    
    // Move to next frame
    advance_next_frame();
    
    return result;
}

bool TamburDecoder::fec_feedback_ready() const
{
    return fec_multi_receiver_ ? fec_multi_receiver_->feedback_ready() : false;
}

FECMsg TamburDecoder::get_fec_feedback()
{
    if (fec_multi_receiver_) {
        return FECMsg{fec_multi_receiver_->get_feedback()};
    }
    return FECMsg{};
}

void TamburDecoder::pad_dummy_frames(uint32_t frame_id)
{
    for (uint32_t id = next_frame_; id < frame_id; id++)
    {
        if (not frame_buf_.count(id)) {
            frame_buf_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(id),
                             std::forward_as_tuple(id, FrameType::DUMMY, 65535, 0));
        }
    }
}

void TamburDecoder::advance_next_frame()
{
    next_frame_++;
    clean_up_to(next_frame_);
}

void TamburDecoder::clean_up_to(uint32_t frontier)
{
    std::vector<uint32_t> to_clean;
    for (auto it = frame_buf_.begin(); it != frame_buf_.end(); ++it)
    {
        if (it->first < frontier)
        {
            to_clean.emplace_back(it->first);
        }
    }
    
    for (auto frame_id : to_clean)
    {
        if (processed_frames_.count(frame_id))
        {
            processed_frames_.erase(frame_id);
        }
        frame_buf_.erase(frame_id);
    }
}

bool TamburDecoder::is_frame_complete(uint32_t frame_id) const
{
    auto it = frame_buf_.find(frame_id);
    return it != frame_buf_.end() and it->second.complete();
}

void TamburDecoder::cleanup_old_frames(uint32_t current_frame_id)
{
    clean_up_to(current_frame_id);
}
