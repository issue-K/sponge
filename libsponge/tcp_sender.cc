#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
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
    , timer_(Timer())
    , RTO_(_initial_retransmission_timeout)
    , li_(std::list<TCPSegment>()) {}

size_t TCPSender::bytes_in_flight() const { return bytes_in_flight_; }

void TCPSender::SendTCPSegment(const TCPSegment& segment, bool isFirst) {
    if (isFirst) {
        bytes_in_flight_ += segment.length_in_sequence_space();
        li_.emplace_back(segment);
    }
    _segments_out.emplace(segment);
    // 如果超时计数器没有打开, 将其打开
    timer_.Start(RTO_);
}

void TCPSender::AckTCPSegment() {
    assert(!li_.empty());
    li_.erase(li_.begin());
}

uint64_t TCPSender::ToAbsoluteSeq(uint32_t seq) {
    return ToAbsoluteSeq(WrappingInt32(seq));
}
uint64_t TCPSender::ToAbsoluteSeq(WrappingInt32 seq) {
    return unwrap(seq, _isn, _next_seqno);
}
// 获得能发送的最大字节数 = 窗口大小 - 发送但未被确认的字节大小
uint32_t TCPSender::GetMaxSend() const {
    // 它基于一个这样的逻辑
    // 1、如果bytes_in_flight_ == 0, 说明li_为空, 那么需要发送一个字节出去(以获得window size)
    // 2、否则, li_不为空, 不需要发送多余的字节.
    if (!window_size_) {
        return !bytes_in_flight_;
    }
    // Todo: solve this(换成if return)
    assert(window_size_ >= bytes_in_flight_);
    return window_size_ - bytes_in_flight();
}

void TCPSender::fill_window() {
    auto siz = GetMaxSend();
    while (siz) {
        TCPSegment segment;
        segment.header().seqno = wrap(_next_seqno, _isn);
        // 1、若为第一次发包, 带上syn标志
        if (!syn_) {
            syn_ = true;
            segment.header().syn = true;
            siz--;
        }
        auto mx = min(TCPConfig::MAX_PAYLOAD_SIZE, size_t(siz));
        string payload = _stream.read(mx);
        assert(mx >= payload.size());
        siz -= payload.size();
        // 2、判断是否能带上fin_标志
        if (!fin_ && _stream.eof() && siz) {
            fin_ = true;
            segment.header().fin = true;
            siz--;
        }
        segment.payload() = Buffer(std::move(payload));
        if (segment.length_in_sequence_space() == 0) {
            break;
        }
        _next_seqno += segment.length_in_sequence_space();
        SendTCPSegment(segment, true);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
// 收到一个ack段, 传递新的ackno和window size. 于是需要删除被确认的任何段
// Todo: 若absolute_ackno不是段的末尾(也就是确认了不完整的段), 会怎么样?
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = ToAbsoluteSeq(ackno);

    if (absolute_ackno > _next_seqno || absolute_ackno < pre_ackno_) {
        return;
    }
    while (!li_.empty()) {
        auto& segment = *li_.begin();
        auto las = ToAbsoluteSeq(segment.header().seqno + segment.length_in_sequence_space() -1);
        // 该段已完全发送完毕
        if (absolute_ackno > las) {
            AckTCPSegment();
        } else {
            break;
        }
    }
    // 5、确认了所有的segment, 停止超时计数器
    if (li_.empty()) {
        timer_.Stop();
    }
    // 7、收到一个比之前的确认号更大的绝对序列号
    if (pre_ackno_ < absolute_ackno) {
        bytes_in_flight_ -= (absolute_ackno - pre_ackno_);
        pre_ackno_ = absolute_ackno;
        RTO_ = _initial_retransmission_timeout;
        if (!li_.empty()) {
            timer_.ResetAndStart(RTO_);
        }
        retransmissions_ = 0;
    }
    window_size_ = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer_.tick(ms_since_last_tick);
    if (timer_.Timeout()) {
        assert(!li_.empty());
        // 重传最早的数据包
        SendTCPSegment(*li_.begin(), false);
        if (window_size_ != 0) {
            retransmissions_++;
            RTO_ <<= 1;
        }
        timer_.ResetAndStart(RTO_);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return retransmissions_;
}

// 生成并发送一个tcp segment. 该tcp segment在序列空间中长度为0.
// 当想要发送一个空的ACK段时, 这是很有用的
void TCPSender::send_empty_segment(bool syn, bool fin, bool rst) {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    segment.header().syn = syn;
    segment.header().fin = fin;
    segment.header().rst = rst;
    _segments_out.emplace(segment);
}