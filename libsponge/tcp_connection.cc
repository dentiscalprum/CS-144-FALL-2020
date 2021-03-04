#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const
{
    return _receiver.window_size();
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const
{
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg)
{
    _time_since_last_segment_received = 0;
    const auto &header = seg.header();

    if(header.rst) {
        reset_connection();
        return;
    } else if(!header.syn && !_receiver.ackno().has_value()) {
        return;
    }

    _receiver.segment_received(seg);

    if(header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }

    if(seg.length_in_sequence_space()) {
        send_out_segment(AT_LEAST_ONCE);
    } else {
        send_out_segment(TRY_MY_BEST);
    }

    if(judge_fin_acked()) {
        if(_active_close) {
            _linger_timer = 0;
            _linger_after_streams_finish = true;
        } else {
            _active = false;
        }
    } else {
        if(header.fin && _active_close == false) {
            _linger_after_streams_finish = false;
        }
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data)
{
    auto write_bytes = _sender.stream_in().write(data);
    send_out_segment(TRY_MY_BEST);
    return write_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{
    _time_since_last_segment_received += ms_since_last_tick;

    if(_linger_after_streams_finish && judge_fin_acked()) {
        if(_linger_timer + ms_since_last_tick < 10 * _cfg.rt_timeout) {
            _linger_timer += ms_since_last_tick;
        } else {
            _linger_after_streams_finish = false;
            _active = false;
            return;
        }
    }  

    _sender.tick(ms_since_last_tick);
    send_out_segment(TICK);

    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _segments_out = queue<TCPSegment>();
        _sender.send_empty_segment();
        auto segment = _sender.segments_out().front();
        _sender.segments_out().pop();

        segment.header().rst = true;
        _segments_out.push(std::move(segment));

        reset_connection();
    }
}

void TCPConnection::end_input_stream()
{
    _sender.stream_in().end_input();
    
    if(!_receiver.stream_out().input_ended()) {
        _active_close = true;
    }

    send_out_segment(TRY_MY_BEST);
}

void TCPConnection::connect()
{
    send_out_segment(TRY_MY_BEST);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_out_segment(SendsegmentMode mode)
{
    if(mode != TICK) {
        _sender.fill_window();
        if(mode == AT_LEAST_ONCE && _sender.segments_out().empty())  _sender.send_empty_segment();
    }

    const auto opt_ackno = _receiver.ackno();
    const uint16_t win = static_cast<uint16_t>(_receiver.window_size());
    while(!_sender.segments_out().empty()) {
        auto segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        
        auto &header = segment.header();
        if(opt_ackno.has_value()) {
            header.ack = true;
            header.ackno = *opt_ackno;
        }
        header.win = win;
        _segments_out.push(std::move(segment));
    }
}

void TCPConnection::reset_connection()
{
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    _linger_after_streams_finish = false;
}

bool TCPConnection::judge_fin_acked() {
    auto &sender_stream = _sender.stream_in();
    return sender_stream.eof() && _sender.next_seqno_absolute() == sender_stream.bytes_written() + 2
        && _sender.bytes_in_flight() == 0;
}