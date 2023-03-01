#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : vec_(capacity), has_(capacity), _output(capacity), _capacity(capacity) {}

void StreamReassembler::push_byteStream() {
    string input;
    while (has_[vec_index_]) {
        unassembled_bytes_--;
        input += vec_[vec_index_];
        has_[vec_index_] = false;
        nxt_index_++;
        vec_index_ = (vec_index_ + 1) % _capacity;
    }
    _output.write(input);
    if (eof_ && empty()){
        _output.end_input();
    }
}
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
// Todo: 考虑截断过最后一个eof串的情况, 此时不应该将eof_设置为true
// 因此这里的做法是, 若本次为eof串, 且一直放置到末尾, eof = true. 若eof串没放到末尾, eof = false
// 而且考虑到只要第一次将eof串放到末尾, 后续也都是末尾位置. 所以不会出现false覆盖true的情况
// Todo: 如果是eof串, index正确但是为空串怎么办?
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if (eof && data.empty()) {
        assert("是eof串, index正确但是为空串");
    }
    if (eof) {
        eof_=true;
    }
    if (data.empty()) {
        push_byteStream();
        return;
    }
    // 最大接收字节数量
    size_t mx_in =  _capacity - _output.buffer_size();
    size_t l = index;
    size_t r = index + data.size() -1;
    l = max(l, nxt_index_);
    r = min(r, nxt_index_ + mx_in - 1);
    l -= index;
    r -= index;
    if (r < data.size() - 1 && eof) {
        eof_ = false;
    }
    for (size_t i = l; i <= r && mx_in; i++) {
        size_t cha = (index + i) - (nxt_index_);
        size_t id = (vec_index_ + cha) % _capacity;
        if (!has_[id]) {
            unassembled_bytes_++;
            has_[id] = true;
            vec_[id] = data[i];
            mx_in--;
        }
    }
    push_byteStream();
}

// 存储但尚未重组的子串中的字节数
size_t StreamReassembler::unassembled_bytes() const { return unassembled_bytes_; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
