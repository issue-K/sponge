#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : vec_(vector<char>(capacity + 1)), cap_(capacity), mod_(capacity + 1), write_at_(0),
    read_at_(0), write_all_(0), read_all_(0), end_input_(false) {
    vec_.reserve(mod_);
}

auto ByteStream::IsFull() const -> bool {
    return (write_at_ + 1 ) % mod_ == read_at_;
}

auto ByteStream::IsEmpty() const -> bool {
    return read_at_ == write_at_;
}

size_t ByteStream::write(const string &data) {
    // DUMMY_CODE(data);
    size_t pre = write_at_;
    for (auto v : data) {
        if (IsFull()) {
            break;
        }
        vec_[write_at_] = v;
        write_at_ = (write_at_ + 1) % mod_;
    }
    size_t wa = (write_at_ + mod_ - pre) % mod_;
    write_all_ += wa;
    return wa;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // DUMMY_CODE(len);
    size_t l = min(len, buffer_size());
    string ans;
    size_t tread_at_ = read_at_;
    for (size_t i = 0; i < l; i++) {
        ans += vec_[tread_at_];
        tread_at_ = (tread_at_ + 1) % mod_;
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    // DUMMY_CODE(len);
    size_t l = min(len, buffer_size());
    read_at_ = (read_at_ + l) % mod_;
    read_all_ += l;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // DUMMY_CODE(len);
    size_t l = min(len, buffer_size());
    string ans;
    for (size_t i = 0; i < l; i++) {
        ans += vec_[read_at_];
        read_at_ = (read_at_ + 1) % mod_;
    }
    read_all_ += l;
    return ans;
}

void ByteStream::end_input() { end_input_ = true; }

bool ByteStream::input_ended() const { return end_input_; }

size_t ByteStream::buffer_size() const {  return (write_at_ + mod_ - read_at_) % mod_; }

bool ByteStream::buffer_empty() const { return IsEmpty(); }

bool ByteStream::eof() const { return end_input_ && IsEmpty(); }

size_t ByteStream::bytes_written() const { return write_all_; }

size_t ByteStream::bytes_read() const { return read_all_; }

size_t ByteStream::remaining_capacity() const { return cap_ - buffer_size(); }
