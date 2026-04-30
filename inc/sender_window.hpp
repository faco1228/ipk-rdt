/**
 * @file sender_window.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief sliding window with per-segment RTO timers for the sender
 * @version 0.1
 * @date 2026-04-28
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. Segment holds one unacknowledged data chunk with its RTO timer
 *   2. SenderWindow tracks up to MAX_WINDOW segments in flight
 *   3. ack_received() removes cumulative-ACKed segments and marks SACK-covered ones
 *   4. get_timed_out() returns segments whose RTO has expired for retransmission
 *   5. mark_retransmitted() doubles the RTO (exponential backoff) and resets send_time
 */

#ifndef SENDER_WINDOW_HPP
#define SENDER_WINDOW_HPP

#include <vector>
#include <cstdint>
#include <utility>
#include <chrono>

/**
 * @brief one segment tracked by SenderWindow
 */
struct Segment {
    uint32_t seq_num;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point send_time;
    int rto_ms; // current retransmission timeout (ms)
    bool acked; // true if covered by a SACK block (skip RTO)
};

/**
 * @brief sliding window of unacknowledged segments for the sender
 *
 * holds at most MAX_WINDOW segments; each has its own RTO timer
 */
class SenderWindow {
public:
    /**
     * @brief initialises the window with RTO derived from the config timeout
     *
     * initial RTO = timeout / 4 (ms), max RTO = timeout (ms)
     *
     * @param timeout global timeout in seconds from Config
     */
    explicit SenderWindow(int timeout);

    /**
     * @brief adds a new segment to the window and starts its RTO timer
     *
     * @param seq sequence number of the segment
     * @param data payload bytes
     */
    void push(uint32_t seq, const std::vector<uint8_t>& data);

    /**
     * @brief processes received ACK and optional SACK blocks
     *
     * removes all segments fully covered by ack_num (cumulative ACK),
     * then checks segments covered by any SACK block as acked
     *
     * @param ack_num cumulative acknowledgement number
     * @param sack_blocks list of [start, end) SACK ranges
     */
    void ack_received(uint32_t ack_num, const std::vector<std::pair<uint32_t, uint32_t>>& sack_blocks);

    /**
     * @brief returns all segments whose RTO has expired
     *
     * only returns segments that are not already SACK-covered
     *
     * @return list of timed-out segments to retransmit
     */
    std::vector<Segment> get_timed_out();

    /**
     * @brief doubles the RTO for a segment and resets its send_time
     *
     * @param seq sequence number of the retransmitted segment
     */
    void mark_retransmitted(uint32_t seq);

    /**
     * @brief returns true if the window has room for another segment
     */
    bool has_capacity() const;

    /**
     * @brief returns true if all sent segments have been acknowledged
     */
    bool all_acked() const;

    /**
     * @brief returns milliseconds until the next RTO expiration, for poll()
     *
     * @return remaining time in ms, or max_rto_ms_ if window is empty
     */
    int next_timeout() const;

private:
    std::vector<Segment> segments_;
    int initial_rto_ms_;
    int max_rto_ms_;
    static constexpr int MAX_WINDOW = 32;
};

#endif // SENDER_WINDOW_HPP