#include "tcp_connection.hh"

#include <iostream>
/*
 * 收到一个TCPSegment时
 * 1、connection从因特网收到一个segment
 * 2、将给segment给TCPReceiver发送, 最后交给inbound stream.
 * 3、提取segment的ackno和window_size交给sender. 好让sender解除阻塞状态继续发送
 *
 * sender不断读取stream信息, 结合receiver的ack, window_size发送出去
 * inbound是receiver读取的, 该流eof意味着收到过一个fin包, 且之前内容全部收到.
 * outbound是sender读取的, 该流eof意味着应用程序发送完毕, 且之前内容全部已发送
 */

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

// 未重组的比特数目
size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return timer.now_time_; }


// parm connect: 表示本次调用是否是因为主动连接, 想发送syn包
void TCPConnection::FillWindow(bool connect) {
    if (connect) {
        connect_ = true;
    }
    if (connect_) {
        _sender.fill_window();
        assert(_sender.syn());
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    bool sender_fin = !_sender.stream_in().eof();
    timer.ResetAndStart(10 * _cfg.rt_timeout);
    auto &header = seg.header();
    if (header.rst) {
        // active状态设置为false
        ReceiveRSTSegment();
        return;
    }
    _receiver.segment_received(seg);
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }
    // important: 只有存在序列号才能fill_window.(否则会提前发送syn.)
    // 也就是说, 收到对方的ack空包不应该发syn, 收到syn才能发syn
    if (header.syn) {
        connect_ = true;
    }
    FillWindow(false);
    // 收到keep alive包.
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
    // 确认任何有序列号的包.
    if (seg.length_in_sequence_space() && _sender.segments_out().empty() && segments_out().empty()) {
        _sender.send_empty_segment();
    }
    SendAll();
    // Todo:
    bool receiver_fin = _receiver.stream_out().input_ended();
    if (receiver_fin && sender_fin) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const { return active_; }

size_t TCPConnection::write(const string &data) {
    auto writed = _sender.stream_in().write(data);
    FillWindow(false);
    SendAll();
    return writed;
}

void TCPConnection::SendAll() {
    auto &segout = _sender.segments_out();
    auto ackno = _receiver.ackno();
    auto wz = _receiver.window_size();
    while (!segout.empty()) {
        auto &segment = segout.front();
        if (ackno.has_value()) {
            segment.header().ack = true;
            segment.header().ackno = ackno.value();
        }
        uint16_t mx_wz = numeric_limits<uint16_t>::max();
        if (wz > mx_wz) {
            segment.header().win = mx_wz;
        } else {
            segment.header().win = wz;
        }
        _segments_out.push(segment);
        segout.pop();
    }
}

void TCPConnection::SendRSTSegment() {
    active_ = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _sender.send_empty_segment(false, false, true);
    SendAll();
}

void TCPConnection::ReceiveRSTSegment() {
    active_ = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

// 值得一提的是, _receiver.stream_out().eof()包含了_receiver.unassembled_bytes() == 0.
// 因为当inbound stream eof时意味着收到fin包, 那么必定把所有序列号给了重组器, 所有字节一定被重组了。

// 而_sender.stream_in().input_ended() && (_sender.bytes_in_flight() == 0)包含了_sender.stream_in().empty();
// 因为当outbound stream input_ended说明应用程序提交了所有字节给stream. 而_sender发送的所有字节都被确认了.
// 这里有一个细节是, end_input_stream函数会调用fill_window取出stream的字节, 所以一定被包含.

// 为了兼容各种实现, 以下写法是最稳妥的
bool TCPConnection::Prereq() const {
    auto p1 = _receiver.stream_out().eof() && _receiver.unassembled_bytes() == 0;
    if (!p1) {
        return false;
    }
    auto p2 = _sender.stream_in().eof() && (_sender.bytes_in_flight() == 0);
    if (!p2) {
        return false;
    }
    auto p3 = _sender.pre_ackno_ == _sender.next_seqno_absolute();
    if (!p3) {
        return false;
    }
    return true;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active_) {
        return;
    }
    timer.tick(ms_since_last_tick);
    // 1、告诉_sender时间的流逝以判断是否需要重传
    // 2、如果重传次数过多, 终止连接
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        SendRSTSegment();
        return;
    }
    // 重要: 先判断是否重传过多. 没有才继续重传.
    _sender.tick(ms_since_last_tick);
    SendAll();
    // Todo: 3、如有必要, 请干净地结束连接
    if (Prereq()) {
        if (!_linger_after_streams_finish) {
            active_ = false;
        } else if (timer.Timeout()) {
            active_ = false;
        }
    }
}

// 关闭out端的stream(允许in端)
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    FillWindow(false);
    SendAll();
}

void TCPConnection::connect() {
    // 发送syn
    FillWindow(true);
    SendAll();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            SendRSTSegment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}