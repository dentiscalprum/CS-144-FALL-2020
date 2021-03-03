#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <iostream>
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
    , _cur_retransmission_timeout(_initial_retransmission_timeout)
    {}

uint64_t TCPSender::bytes_in_flight() const
{
    return _next_seqno - _unack_seq_left;
}

void TCPSender::send_segment(const TCPSegment &segment, uint64_t absolute_seq) {
    _segments_out.push(segment);
    if(_unack_segment.empty()) _retransmission_timer = 0;
    if(!_unack_segment.count(absolute_seq)) _unack_segment[absolute_seq] = segment;
}

void TCPSender::fill_window()
{
    TCPSegment segment;
    auto &header = segment.header();

    if(_next_seqno == 0) {
        header.syn = true;
        header.seqno = _isn;

        _next_seqno = 1;
        send_segment(segment, 0);
    } else {
        while(true) {
            uint16_t window_size = _window_size;
            if(window_size == 0) {
                window_size = 1;
            }

            uint16_t absolute_seq = _next_seqno;
            header.seqno = wrap(_next_seqno, _isn);

            size_t len_data = min(window_size - (_next_seqno - _unack_seq_left), TCPConfig::MAX_PAYLOAD_SIZE);
            auto data = _stream.read(len_data);

            bool have_segment = false;
            if(data.size() != 0) {
                _next_seqno += data.size();
                segment.payload() = Buffer(std::move(data));
                have_segment = true;
            }

            if(_stream.eof() && window_size - (_next_seqno - _unack_seq_left) > 0 &&
                        _next_seqno < _stream.bytes_written() + 2) {
                header.fin = true;
                _next_seqno++;
                have_segment = true;
            }

            if(have_segment) {
                cout << segment.header().to_string();
                send_segment(segment, absolute_seq);
                if(_window_size == 0) break;
            } else break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    uint64_t absoluteAckno = unwrap(ackno, _isn, _unack_seq_left);
    if(absoluteAckno > _next_seqno || absoluteAckno < _unack_seq_left) {
        return;
    }
    
    for(auto iter = _unack_segment.begin(); iter != _unack_segment.end(); ) {
        if(iter->first + iter->second.length_in_sequence_space() <= absoluteAckno) {
            _unack_segment.erase(iter++);
        } else break;
    }

    if(absoluteAckno > _unack_seq_left) {
        _unack_seq_left = absoluteAckno;
        _consecutive_retransmissions = 0;
        _cur_retransmission_timeout = _initial_retransmission_timeout;
        _retransmission_timer = 0;
    }
    _window_size = window_size;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    if(_unack_segment.empty()) {
        return;
    } else if(_retransmission_timer + ms_since_last_tick < _cur_retransmission_timeout) {
        _retransmission_timer += ms_since_last_tick;
        return;
    }

    if(_window_size != 0) {
        _consecutive_retransmissions++;
        _cur_retransmission_timeout *= 2;
    }
    _retransmission_timer = 0;

    auto begin = _unack_segment.begin();
    send_segment(begin->second, begin->first);
}

unsigned int TCPSender::consecutive_retransmissions() const
{
    return _consecutive_retransmissions;
}

void TCPSender::send_empty_segment()
{
    TCPSegment empty_segment;
    empty_segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(empty_segment);
}
