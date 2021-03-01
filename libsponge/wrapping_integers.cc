#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t mask = (1ul << 32) - 1;
    uint64_t n_mod = ((n & mask) + isn.raw_value()) & mask;
    return WrappingInt32(static_cast<uint32_t>(n_mod));
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t result = 0;
    if(n.raw_value() < isn.raw_value()) {
        result = n.raw_value() + (1ul << 32) - isn.raw_value();
    } else {
        result = n.raw_value() - isn.raw_value();
    }


    uint64_t mask = (1ul << 32) - 1;
    result &= mask;
    result += (checkpoint & ~mask);

    if(result < checkpoint && checkpoint - result > (1ul << 31)) {
        result += (1ul << 32);
    } else if(checkpoint < result && result - checkpoint > (1ul << 31)) {
        if(result - (1ul << 32) < result) {
            result -= (1ul << 32);
        }
    }
    return result;
}
