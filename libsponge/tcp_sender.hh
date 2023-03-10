#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <list>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  public:
    class Timer {
      public:
        unsigned int expire_time_{0};
        unsigned int now_time_{0};
        bool stoped_{true};
        void Start(unsigned int rto_) {
            if (!stoped_) {
                return;
            }
            stoped_ = false;
            expire_time_ = rto_;
            now_time_ = 0;
        }
        void ResetAndStart(unsigned int rto_) {
            stoped_ = false;
            expire_time_ = rto_;
            now_time_ = 0;
        }
        void Stop() {
            stoped_ = true;
        }
        [[nodiscard]] bool Timeout() const {
            if (stoped_) {
                return false;
            }
            return now_time_ >= expire_time_;
        }
        void tick(const size_t ms_since_last_tick) {
            now_time_ += ms_since_last_tick;
        }

    };
    WrappingInt32 _isn;
    // 最新收到接收方的确认号
    uint64_t pre_ackno_{0};
  private:
    //! our initial sequence number, the number for our SYN.


    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    // 下一个需要发送字节的 absolute sequence number
    uint64_t _next_seqno{0};
    // My var ---------------------------------------
    Timer timer_;
    unsigned int RTO_;
    unsigned int retransmissions_{0};
    bool syn_{false};
    bool fin_{false};
    // 多少字节发送但还没有被确认
    uint32_t bytes_in_flight_{0};

    uint64_t window_size_{1};
    std::list<TCPSegment> li_;

    // My Function ----------------------------------
    // 会引发超时计数器的发送
    void SendTCPSegment(const TCPSegment& segment, bool isFirst);
    void AckTCPSegment();
    uint64_t ToAbsoluteSeq(uint32_t seq);
    uint64_t ToAbsoluteSeq(WrappingInt32 seq);
    [[nodiscard]] uint32_t GetMaxSend() const ;
  public:
    bool syn() { return syn_; }
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    // 发送一个空负载段(用于创建空的ack段)
    void send_empty_segment(bool syn = false, bool fin = false, bool rst = false);

    //! \brief create and send segments to fill as much of the window as possible
    // 创建并发送片段来填充尽可能多的窗口
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    // 通知TCPSender时间的流逝
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    // 发送但尚未确认的段占用了多少序列号?
    // 注释计数在“序列空间”中，即SYN和FIN每个计数为一个字节(参见TCPSegment::length_in_sequence_space()))
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    // 连续重传的个数
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    // TCPSender为传输加入队列的tcpsegment。
    // 这些必须从队列中取出并由TCPConnection发送，TCPConnection在发送之前需要填充由TCPReceiver设置的字段(ackno和窗口大小)。
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH