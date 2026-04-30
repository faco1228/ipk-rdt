/**
 * @file sender_window.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief sliding window implementation with per-segment RTO timers
 * @version 0.1
 * @date 2026-04-28
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. push() adds a segment with send_time = now() and initial_rto_ms_
 *   2. ack_received() erases cumulative-ACKed segments via remove_if/erase,
 *      then marks SACK-covered segments as acked
 *   3. get_timed_out() iterates segments and collects those whose elapsed
 *      time since send_time exceeds rto_ms
 *   4. mark_retransmitted() doubles rto_ms (capped at max_rto_ms_) and
 *      resets send_time to now()
 *   5. next_timeout() computes the minimum remaining time across all active
 *      segments for use as the poll() timeout
 */

#include "sender_window.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief initialises the window with RTO derived from the config timeout
 */
SenderWindow::SenderWindow(int timeout) {
    initial_rto_ms_ = (timeout * 1000) / 4;
    max_rto_ms_ = timeout * 1000;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief adds a new segment to the window and starts its RTO timer
 */
void SenderWindow::push(uint32_t seq, const std::vector<uint8_t>& data) {
    Segment segm;
    segm.seq_num = seq;
    segm.data = data;
    segm.send_time = std::chrono::steady_clock::now();
    segm.rto_ms = initial_rto_ms_;
    segm.acked = false;
    segments_.push_back(segm);
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief processes a received ACK and optional SACK blocks
 */
void SenderWindow::ack_received(uint32_t ack_num, const std::vector<std::pair<uint32_t, uint32_t>>& sack_blocks) {
    // erase all segments fully covered by the cumulative ACK
    segments_.erase(
        std::remove_if(segments_.begin(), segments_.end(),
            [ack_num](const Segment& seg) {
                return seg.seq_num + static_cast<uint32_t>(seg.data.size()) <= ack_num;
            }),
        segments_.end()
    );

    // mark segments covered by any SACK block so they are not retransmitted
    for (const auto& block : sack_blocks) {
        uint32_t start = block.first;
        uint32_t end = block.second;
        for (auto& segm : segments_) {
            if (segm.seq_num >= start &&
                segm.seq_num + static_cast<uint32_t>(segm.data.size()) <= end) {
                segm.acked = true;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns all segments whose RTO has expired
 */
std::vector<Segment> SenderWindow::get_timed_out() {
    std::vector<Segment> result;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    for (const Segment& segm : segments_) {
        if (segm.acked) {
            continue;
        }
        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - segm.send_time).count();
        if (elapsed >= segm.rto_ms) {
            result.push_back(segm);
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief doubles the RTO for a segment and resets its send_time
 */
void SenderWindow::mark_retransmitted(uint32_t seq) {
    for (Segment& segm : segments_) {
        if (segm.seq_num == seq) {
            segm.rto_ms = std::min(segm.rto_ms * 2, max_rto_ms_);
            segm.send_time = std::chrono::steady_clock::now();
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns true if the window has room for another segment
 */
bool SenderWindow::has_capacity() const {
    return segments_.size() < MAX_WINDOW;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns true if all sent segments have been acknowledged
 */
bool SenderWindow::all_acked() const {
    return segments_.empty();
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns milliseconds until the next RTO expiry, for poll()
 */
int SenderWindow::next_timeout() const {
    if (segments_.empty()) {
        return max_rto_ms_;
    }

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    int min_remain = max_rto_ms_;

    for (const Segment& segm : segments_) {
        if (segm.acked) {
            continue;
        }
        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - segm.send_time).count();
        int remain = segm.rto_ms - static_cast<int>(elapsed);
        if (remain < min_remain) {
            min_remain = remain;
        }
    }

    return std::max(0, min_remain);
}