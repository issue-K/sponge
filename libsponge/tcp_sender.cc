//#include "tcp_sender.hh"
//
//#include "tcp_config.hh"
//
//#include <random>
//#include <iostream>
//
//// Dummy implementation of a TCP sender
//
//// For Lab 3, please replace with a real implementation that passes the
//// automated checks run by `make check_lab3`.
//
//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}
//
//using namespace std;
//
////! \param[in] capacity the capacity of the outgoing byte stream
////! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
////! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
//TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
//    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
//    , _initial_retransmission_timeout{retx_timeout}
//    , _stream(capacity)
//    , timer_(Timer())
//    , RTO_(_initial_retransmission_timeout)
//    , li_(std::list<TCPSegment>()) {}
//
//size_t TCPSender::bytes_in_flight() const { return bytes_in_flight_; }
//
//void TCPSender::SendTCPSegment(const TCPSegment& segment, bool isFirst) {
//    if (isFirst) {
//        bytes_in_flight_ += segment.length_in_sequence_space();
//        li_.emplace_back(segment);
//    }
//    _segments_out.emplace(segment);
//    // 如果超时计数器没有打开, 将其打开
//    timer_.Start(RTO_);
//}
//
//void TCPSender::AckTCPSegment() {
//    assert(!li_.empty());
//    li_.erase(li_.begin());
//}
//
//uint64_t TCPSender::ToAbsoluteSeq(uint32_t seq) {
//    return ToAbsoluteSeq(WrappingInt32(seq));
//}
//uint64_t TCPSender::ToAbsoluteSeq(WrappingInt32 seq) {
//    return unwrap(seq, _isn, _next_seqno);
//}
//// 获得能发送的最大字节数 = 窗口大小 - 发送但未被确认的字节大小
//uint32_t TCPSender::GetMaxSend() const {
//    // 它基于一个这样的逻辑
//    // 1、如果bytes_in_flight_ == 0, 说明li_为空, 那么需要发送一个字节出去(以获得window size)
//    // 2、否则, li_不为空, 不需要发送多余的字节.
//    if (!window_size_) {
//        return !bytes_in_flight_;
//    }
//    assert(window_size_ >= bytes_in_flight_);
//    // Todo: solve this
//    return window_size_ - bytes_in_flight();
//}
//
//// 从ByteStream获取负载, 并以Tcp Segment的形式发送尽可能多的字节
//// 只要有新的字节需要读取, 且窗口有可用空间
//void TCPSender::fill_window() {
//    std::cout << window_size_ << " " << bytes_in_flight_ << "\n";
//    auto siz = GetMaxSend();
//    while (siz && !fin_) {
//        TCPSegment segment;
//        segment.header().seqno = wrap(_next_seqno, _isn);
//        // 1、若为第一次发包, 带上syn标志
//        if (!syn_) {
//            syn_ = true;
//            _next_seqno++;
//            segment.header().syn = true;
//            siz--;
//        }
//        auto mx = min(TCPConfig::MAX_PAYLOAD_SIZE, size_t(siz));
//        string payload = _stream.read(mx);
//        assert(mx >= payload.size());
//        siz -= payload.size();
//        _next_seqno += payload.size();
//        // 2、判断是否能带上fin_标志
//        if (_stream.eof() && siz) {
//            fin_ = true;
//            _next_seqno++;
//            segment.header().fin = true;
//            siz--;
//        }
//        // 3、负载为空, 考虑退出循环. (需要考虑本包是否有syn/fin标志, 有的话发出去)
//        if (payload.empty()) {
//            // 负载为空, 考虑是否要带上fin标志
//            if (segment.length_in_sequence_space()) {
//                SendTCPSegment(segment, true);
//            }
//            break;
//        }
//        segment.payload() = Buffer(std::move(payload));
//        SendTCPSegment(segment, true);
//    }
//}
//
////! \param ackno The remote receiver's ackno (acknowledgment number)
////! \param window_size The remote receiver's advertised window size
//// 收到一个ack段, 传递新的ackno和window size. 于是需要删除被确认的任何段
//void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
//    uint64_t absolute_ackno = ToAbsoluteSeq(ackno);
//    // 确保该ack是正确的
//    // assert(absolute_ackno <= _next_seqno);
//
//    if (absolute_ackno > _next_seqno) {
//        return;
//    }
//    while (!li_.empty()) {
//        auto& segment = *li_.begin();
//        auto las = segment.header().seqno + segment.length_in_sequence_space();
//        // 该段已完全发送完毕
//        if (absolute_ackno >= ToAbsoluteSeq(las)) {
//            AckTCPSegment();
//        } else {
//            break;
//        }
//    }
//    // 5、确认了所有的segment, 停止超时计数器
//    if (li_.empty()) {
//        timer_.Stop();
//    }
//    // 7、收到一个比之前的确认号更大的绝对序列号
//    if (pre_ackno_ < absolute_ackno) {
//        bytes_in_flight_ -= (absolute_ackno - pre_ackno_);
//        pre_ackno_ = absolute_ackno;
//        RTO_ = _initial_retransmission_timeout;
//        if (!li_.empty()) {
//            timer_.ResetAndStart(RTO_);
//        }
//        retransmissions_ = 0;
//    }
//    window_size_ = window_size;
//    fill_window();
//}
//
////! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//void TCPSender::tick(const size_t ms_since_last_tick) {
//    timer_.tick(ms_since_last_tick);
//    if (timer_.Timeout()) {
//        assert(!li_.empty());
//        // 重传最早的数据包
//        SendTCPSegment(*li_.begin(), false);
//        if (window_size_ != 0) {
//            retransmissions_++;
//            RTO_ <<= 1;
//        }
//        timer_.ResetAndStart(RTO_);
//    }
//}
//
//unsigned int TCPSender::consecutive_retransmissions() const {
//    return retransmissions_;
//}
//
//// 生成并发送一个tcp segment. 该tcp segment在序列空间中长度为0.
//// 当想要发送一个空的ACK段时, 这是很有用的
//void TCPSender::send_empty_segment(bool syn, bool fin, bool rst) {
//    TCPSegment segment;
//    segment.header().seqno = next_seqno();
//    segment.header().syn = syn;
//    segment.header().syn = syn;
//    segment.header().fin = fin;
//    segment.header().rst = rst;
//    _segments_out.emplace(segment);
//}


#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

using namespace std;

void Timer::start(unsigned int timeout) {
    _active = true;
    _expired = false;
    _current_time = 0;
    _timeout = timeout;
}

void Timer::update(unsigned int time_elapsed) {
    if (!_active) {
        throw runtime_error("Trying to update an inactive timer. Please start it first.");
    }
    _current_time += time_elapsed;
    if (_current_time >= _timeout)
        _expired = true;
}

void Timer::reset() {
    _active = false;
    _expired = false;
    _timeout = 0;
    _current_time = 0;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retrans_timeout(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t res = 0;
    for (auto &seg : _retrans_buf) {
        res += seg.length_in_sequence_space();
    }
    return res;
}

void TCPSender::fill_window() {
    size_t remaining_winsize = (_window_size != 0 ? _window_size : 1);
    size_t out_size = bytes_in_flight();
    if (remaining_winsize < out_size)
        return;
    remaining_winsize -= out_size;

    while (true) {
        size_t seg_size = remaining_winsize;
        if (seg_size == 0)
            break;

        TCPSegment seg;
        TCPHeader &header = seg.header();

        // first, put the SYN flag into the seg if nothing has been sent
        if (!_syn_sent) {
            seg_size -= 1;
            header.syn = true;
            _syn_sent = true;
        }

        // then, stuff as much data as possible into the seg
        header.seqno = wrap(_next_seqno, _isn);
        string seg_data = _stream.read(min(seg_size, TCPConfig::MAX_PAYLOAD_SIZE));
        seg_size -= seg_data.size();
        seg.payload() = Buffer(move(seg_data));

        // finally, put the FIN flag if the input stream has ended and there's still space
        if (!_fin_sent && _stream.eof() && seg_size > 0) {
            seg_size -= 1;
            header.fin = true;
            _fin_sent = true;
        }

        seg_size = seg.length_in_sequence_space();
        // if the segment's actual size is 0, it shouldn't been sent
        if (seg_size == 0)
            break;

        _segments_out.emplace(seg);
        _retrans_buf.emplace_back(seg);
        _next_seqno += seg_size;
        remaining_winsize -= seg_size;

        if (!_timer.active())
            _timer.start(_retrans_timeout);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    // use next seqno as checkpoint
    uint64_t ack_seqno = unwrap(ackno, _isn, _next_seqno);
    // it's impossible that ackno > _next_seqno, because that byte hasn't been sent yet!
    if (ack_seqno > _next_seqno) {
        return;
    }

    // remove completely ack-ed segments from the retransmission buffer
    // because the segment in retrans buffer is ordered by seqno,
    // it's ok to break once current seg can't be erased (subsequent seg has larger seqno)
    for (auto it = _retrans_buf.begin(); it != _retrans_buf.end();) {
        if (unwrap((*it).header().seqno, _isn, _next_seqno) + (*it).length_in_sequence_space() <= ack_seqno) {
            it = _retrans_buf.erase(it);
            _retrans_timeout = _initial_retransmission_timeout;
            _timer.start(_retrans_timeout);
            _consec_retrans_count = 0;
        } else
            break;
    }
    // stop the timer if retransmission buffer is clear
    if (_retrans_buf.empty())
        _timer.reset();

    // refill the window
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.active())
        _timer.update(ms_since_last_tick);
    if (_timer.expired()) {
        _segments_out.emplace(_retrans_buf.front());
        if (_window_size > 0) {
            _consec_retrans_count++;
            _retrans_timeout *= 2;
        }
        _timer.start(_retrans_timeout);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consec_retrans_count; }

void TCPSender::send_empty_segment(bool syn, bool fin, bool rst) {
    if (syn)
        _syn_sent = true;
    if (fin)
        _fin_sent = true;

    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    seg.header().syn = syn;
    seg.header().fin = fin;
    seg.header().rst = rst;
    _next_seqno += seg.length_in_sequence_space();
    _segments_out.push(seg);
}