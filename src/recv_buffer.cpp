/**
 * @file recv_buffer.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief reorder buffer implementation for the receiver
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. insert() drops duplicates and already-delivered segments, stores the rest
 *   2. flush() walks the map from the front and collects segments that start
 *      exactly at "exp_next_" stops on the first gap
 *   3. get_ack_num() returns exp_next_
 *   4. get_sack_blocks() builds [start, end) pairs from the map, up to 3 blocks
 */

#include "recv_buffer.hpp"

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief inserts a segment into the reorder buffer
 */
void RecvBuffer::insert(uint32_t seq, const std::vector<uint8_t>& data) {
    if (seq < exp_next_) {
        return; // already delivered
    }
    if (buffer_.count(seq) > 0) {
        return; // duplicate
    }
    buffer_[seq] = data;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief delivers all contiguous in-order data and advances exp_next_
 */
std::vector<uint8_t> RecvBuffer::flush() {
    std::vector<uint8_t> result;

    while (!buffer_.empty()) {
        auto first = buffer_.begin();
        if (first->first != exp_next_) {
            break; // gap, process stops
        }
        result.insert(result.end(), first->second.begin(), first->second.end());
        exp_next_ += static_cast<uint32_t>(first->second.size());
        buffer_.erase(first);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns the next expected byte (cumulative ACK value)
 */
uint32_t RecvBuffer::get_ack_num() const {
    return exp_next_;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns up to 3 SACK blocks for out-of-order segments
 */
std::vector<std::pair<uint32_t, uint32_t>> RecvBuffer::get_sack_blocks() const {
    std::vector<std::pair<uint32_t, uint32_t>> result;

    for (const auto& entry : buffer_) {
        result.push_back({entry.first, entry.first + static_cast<uint32_t>(entry.second.size())});
        if (result.size() == 3) {
            break;
        }
    }

    return result;
}