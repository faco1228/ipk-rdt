/**
 * @file sender_window.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "sender_window.hpp"

#include <algorithm>
#include <chrono> 
#include <vector>

/**
 * @brief Construct a new Sender Window:: Sender Window object
 * 
 * @param timeout 
 */
SenderWindow::SenderWindow(int timeout) {
    initial_rto_ms_ = (timeout * 1000) / 4; // convert seconds to miliseconds
    max_rto_ms_ = (timeout * 1000); 
}

/**
 * @brief 
 * 
 * @param seq 
 * @param data 
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

/**
 * @brief 
 * 
 * @param ack_num 
 * @param sack_blocks 
 */
void SenderWindow::ack_received(uint32_t ack_num, const std::vector<std::pair<uint32_t, uint32_t>>& sack_blocks) {
    // erase all segments, where: seq_num + data.size() <= ack_num
    segments_.erase(
        std::remove_if(segments_.begin(), segments_.end(),
            [ack_num](const Segment& seg) {
                return seg.seq_num + static_cast<uint32_t>(seg.data.size()) <= ack_num;
            }),
        segments_.end()
    );

    // select SACK segments as acked
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

/**
 * @brief 
 * 
 * @return std::vector<Segment> 
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

/**
 * @brief 
 * 
 * @param seq 
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

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool SenderWindow::has_capacity() const {
    return segments_.size() < MAX_WINDOW;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool SenderWindow::all_acked() const {
    return segments_.empty();
}

/**
 * @brief 
 * 
 * @return int 
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
        int remain = segm.rto_ms - elapsed;

        if (remain < min_remain) {
            min_remain = remain;
        }
    }

    return std::max(0, min_remain);
}