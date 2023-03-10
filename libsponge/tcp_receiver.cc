#include "tcp_receiver.hh"
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;


// important: 由于段中的syn和fin拥有独立的序列号
// 而之前StreamReassembler::push_substring传递的是绝对序列号, 以0开始.
// 那么现在, 需要告诉StreamReassembler::push_substring收到syn和fin信息)
// syn主动调用_reassembler.syn(), fin由eof参数传递过去.
// 重点在于维护_reassembler.GetFirstUnassembled(): 第一个未重组字节索引
// 1、当收到syn包, 令_reassembler.GetFirstUnassembled() = 1
// 2、当收到fin包, 把信号传递给_reassembler. _reassembler判断, 若当前所有字节推送给字节流后, 再令_reassembler.GetFirstUnassembled()++;
void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 若本包是SYN包, segno要自增.(syn序列号本身没有任何字节)
    auto seqno = seg.header().seqno;
    if (seg.header().syn) {
        isn_ = seg.header().seqno;
        _reassembler.syn();
        seqno = seqno + 1;
    }
    if (!isn_.has_value()) {
        return;
    }
    string str = seg.payload().copy();
    auto isn = isn_.value();
    // 这里seqno必须是数据开始的部分. 而且即使负载为0, 仍要push_substring. 因为fin标记需要下推
    uint64_t index = unwrap(seqno, isn, _reassembler.GetFirstUnassembled());
    _reassembler.push_substring(str, index, seg.header().fin);
}


// 其中包含接收者不知道的第一个字节的序列号。这是窗口的左边缘: 接收器想要接收的第一个字节。
// 如果还没有设置ISN，则返回一个空的optional对象。
std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!isn_.has_value()) {
        return {};
    }
    return wrap(_reassembler.GetFirstUnassembled(), isn_.value());
}

// 返回“first unassembled”索引(对应ackno的索引)和“first unacceptable”索引之间的距离。
size_t TCPReceiver::window_size() const {
    return _reassembler.GetFirstUnacceptable() - _reassembler.GetFirstUnassembled();
    //return _capacity - _reassembler.stream_out().buffer_size();
}