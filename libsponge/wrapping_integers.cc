#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

#define MAX (1UL << 32)

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// 将以零开始的绝对序列号和ISN, 转换为WrappingInt32(序列号)
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // DUMMY_CODE(n, isn);
    uint32_t seq = (n + isn.raw_value()) % MAX;
    return WrappingInt32{seq};
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
// 给定序列号n, 初始序列号isn.
// unwrap(WrappingInt32(UINT32_MAX - 1), WrappingInt32(0), 3 * (1ul << 32)), 3 * (1ul << 32) - 2);
uint64_t abs(uint64_t a, uint64_t b) {
    if (a > b) {
        return a - b;
    }
    return b - a;
}

uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // DUMMY_CODE(n, isn, checkpoint);
    uint64_t cha = std::uint64_t(MAX + n.raw_value() - isn.raw_value()) % MAX;
    if (checkpoint <= cha) {
        return cha;
    }
    // 找到第一个小于checkpoint的和第一个大于等于的
    uint64_t down = (checkpoint - cha) / MAX * MAX + cha;
    uint64_t up = down + MAX;
    if (checkpoint - down < up - checkpoint) {
        return down;
    } else {
        return up;
    }
}
