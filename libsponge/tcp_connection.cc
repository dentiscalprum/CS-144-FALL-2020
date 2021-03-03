#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return {}; }

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
    }

    if(seg.length_in_sequence_space()) {
        _receiver.segment_received(seg);
    }

    if(header.ack) {
        _sender.ack_received(header.ackno, header.win);
        
        if(header.syn) {
            _state = State::ESTABLISHED;
            _active = true;
        } else if(_state == State::SYN_RCVD) {
            _state = State::ESTABLISHED;
        }
    } else if(header.syn) {
        _state = State::SYN_RCVD;
        _active = true;
    }

    if(_state != State::CLOSED) {
        _sender.fill_window();
        if(send_out_segment() > 0) return;
    }

    if(seg.length_in_sequence_space()) {
        TCPSegment ack_seg;
        auto &ack_header = ack_seg.header();
        ack_header.ack = true;
        ack_header.ackno = *_receiver.ackno();
        ack_header.win = static_cast<uint16_t>(_receiver.window_size());
        _segments_out.push(ack_seg);
    }

    if(judge_fin_acked()) {
        switch(_state) {
        case State::FIN_WAIT1:
        case State::FIN_WAIT2:
        case State::CLOSING:
        case State::TIME_WAIT:
            _timer = 0;
            _state = State::TIME_WAIT;
            break;
        case State::LAST_ACK:
            _active = false;
            _state = State::CLOSED;
            break;
        default:
            break;
        }
    } else {
        if(header.fin == true) {
            if(_state == State::FIN_WAIT1) {
                _state = State::CLOSING;
            } else {
                _state = State::CLOSE_WAIT;
                _timer = 0;
                _linger_after_streams_finish = false;
            }
        }
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data)
{
    auto write_bytes = _sender.stream_in().write(data);

    _sender.fill_window();
    send_out_segment();
    return write_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick)
{
    _time_since_last_segment_received += ms_since_last_tick;

    if(_state == State::TIME_WAIT) {
        if(_timer + ms_since_last_tick < 10 * _cfg.rt_timeout) {
            _timer += ms_since_last_tick;
        } else {
            _state = State::CLOSED;
            _active = false;
            return;
        }
    }  

    _sender.tick(ms_since_last_tick);
    send_out_segment();

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

    _sender.fill_window();
    send_out_segment();

    switch(_state) {
    case State::ESTABLISHED:
        _state = State::FIN_WAIT1;
        break;
    case State::CLOSE_WAIT:
        _state = State::LAST_ACK;
        break;
    default:
        break;
    }
}

void TCPConnection::connect()
{
    _sender.fill_window();
    send_out_segment();
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

int TCPConnection::send_out_segment()
{
    int num = 0;
    const auto opt_ackno = _receiver.ackno();
    const uint16_t win = static_cast<uint16_t>(_receiver.window_size());

    while(!_sender.segments_out().empty()) {
        num++;
        auto segment = _sender.segments_out().front();
        _sender.segments_out().pop();

        auto &header = segment.header();
        if(opt_ackno) {
            header.ack = true;
            header.ackno = *opt_ackno;
        }
        header.win = win;
        _segments_out.push(std::move(segment));
    }
    return num;
}

bool TCPConnection::judge_fin_acked()
{
   return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 
                && _sender.bytes_in_flight() == 0; 
}

void TCPConnection::reset_connection()
{
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
    _linger_after_streams_finish = false;
}