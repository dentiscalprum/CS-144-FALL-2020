#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg)
{
    if(already_sync_ == false && seg.header().syn == false) return;

    WrappingInt32 startSeqno{0};
    if(seg.header().syn) {
        already_sync_ = true;
        isn_ = seg.header().seqno;
        checkpoint_ = 0;
        startSeqno = seg.header().seqno + 1;
    } else {
        startSeqno = seg.header().seqno;
    }

    uint64_t absoluteSeqno = unwrap(startSeqno, isn_, checkpoint_);
    checkpoint_ = absoluteSeqno;

    uint64_t streamIndex = absoluteSeqno - 1;
    bool eof = false;
    if(seg.header().fin) {
        eof = true;
        eof_ = true;
    }
    
    uint64_t max_seq = _reassembler.stream_out().bytes_read() + _capacity + 1;
    if(seg.header().fin) {
        max_seq += 1;
    }

    if(absoluteSeqno + seg.length_in_sequence_space() > max_seq) {
        return;
    }
    _reassembler.push_substring(seg.payload().copy(), streamIndex, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const
{
    std::optional<WrappingInt32> ackOpt;
    if(already_sync_ == false) return ackOpt;
    uint64_t absoluteAckno = _reassembler.stream_out().bytes_written() + 1;
    if(_reassembler.stream_out().input_ended()) {
        absoluteAckno += 1;
    }
    return wrap(absoluteAckno, isn_);
}

size_t TCPReceiver::window_size() const
{
    uint64_t window_size = _reassembler.stream_out().remaining_capacity();
    if(_reassembler.stream_out().input_ended()) {
        window_size -= 1;
    }
    return window_size;
}
