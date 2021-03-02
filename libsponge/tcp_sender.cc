#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
#include <string>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , cur_retransmission_timeout_(_initial_retransmission_timeout)
    {}

uint64_t TCPSender::bytes_in_flight() const
{
    return _next_seqno - unack_seq_left_;
}

void TCPSender::sendSegment(const TCPSegment &segment, uint64_t absolute_seq) {
    _segments_out.push(segment);
    if(unack_segment_.empty()) retransmission_timer_ = 0;
    if(!unack_segment_.count(absolute_seq)) unack_segment_[absolute_seq] = segment;
}

void TCPSender::fill_window()
{
    uint64_t absolute_seq;
    TCPSegment segment;
    auto &header = segment.header();


    switch(state_) {
    case CLOSED:
        header.syn = true;
        header.seqno = _isn;
        _next_seqno = 1;
        absolute_seq = 0;
        state_ = SYN_SENT;
        sendSegment(segment, absolute_seq);
        break;
    case SYN_ACKED:
        while(true) {
            uint16_t window_size = window_size_;
            if(window_size == 0) {
                window_size = 1;
            }

            absolute_seq = _next_seqno;
            header.seqno = wrap(_next_seqno, _isn);

            size_t len_data = min(window_size - (_next_seqno - unack_seq_left_), 
                        TCPConfig::MAX_PAYLOAD_SIZE);
            auto data = _stream.read(len_data);

            bool haveSegment = false;
            if(data.size() != 0) {
                _next_seqno += data.size();
                segment.payload() = Buffer(std::move(data));
                haveSegment = true;
            }
            if(_stream.eof() && window_size - (_next_seqno - unack_seq_left_) > 0 &&
                    state_ != FIN_SENT) {
                header.fin = true;
                state_ = FIN_SENT;
                _next_seqno++;
                haveSegment = true;
            } 

            if(haveSegment == true) {
                sendSegment(segment, absolute_seq);
            } else {
                break;
            }
        }
    default:
        break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    if(ackno == _isn + 1 && state_ == SYN_SENT) {
        state_ = SYN_ACKED;
    }
    uint64_t absoluteAckno = unwrap(ackno, _isn, unack_seq_left_);
    if(absoluteAckno > _next_seqno) {
        return;
    }
    
    for(auto iter = unack_segment_.begin(); iter != unack_segment_.end();) {
        if(iter->first < absoluteAckno && iter->first + iter->second.length_in_sequence_space() <= absoluteAckno) {
            unack_segment_.erase(iter++);
        } else {
            break;
        }
    }
    if(absoluteAckno > unack_seq_left_) {
        unack_seq_left_ = absoluteAckno;
        consecutive_retransmissions_ = 0;
        cur_retransmission_timeout_ = _initial_retransmission_timeout;
        retransmission_timer_ = 0;
    }
    window_size_ = window_size;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    if(retransmission_timer_ + ms_since_last_tick < cur_retransmission_timeout_ || 
            unack_segment_.empty()) {
        retransmission_timer_ += ms_since_last_tick;
        return;
    }

    if(window_size_ != 0) {
        consecutive_retransmissions_++;
        cur_retransmission_timeout_ *= 2;
    }
    retransmission_timer_ = 0;

    auto begin = unack_segment_.begin();
    sendSegment(begin->second, begin->first);
}

unsigned int TCPSender::consecutive_retransmissions() const
{
    return consecutive_retransmissions_;
}

void TCPSender::send_empty_segment()
{
    TCPSegment empty_segment;
    empty_segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(empty_segment);
}
