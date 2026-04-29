/**
 * @file recv_buffer.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "recv_buffer.hpp"

/**
 * @brief 
 * 
 * @param seq 
 * @param data 
 */
void RecvBuffer::insert(uint32_t seq, const std::vector<uint8_t>& data) {
    if (seq < exp_next_) {
        return;
    }

    if (buffer_.count(seq) > 0) {
        return;
    }

    buffer_[seq] = data;
}

/**
 * @brief 
 * 
 * @return std::vector<uint8_t> 
 */
std::vector<uint8_t> RecvBuffer::flush() {
    std::vector<uint8_t> result{};

    while (!buffer_.empty()) {
        auto first = buffer_.begin();
        if (first->first != exp_next_) {
            break;
        }

        result.insert(result.end(), first->second.begin(), first->second.end());
        exp_next_ += static_cast<uint32_t>(first->second.size());
        buffer_.erase(first);
    }

    return result;
}

/**
 * @brief 
 * 
 * @return uint32_t 
 */
uint32_t RecvBuffer::get_ack_num() const {
    return exp_next_;
}

/**
 * @brief 
 * 
 * @return std::vector<std::pair<uint32_t, uint32_t>> 
 */
std::vector<std::pair<uint32_t, uint32_t>> RecvBuffer::get_sack_blocks() const {
    std::vector<std::pair<uint32_t, uint32_t>> result{};

    for (const auto& entry : buffer_) {
        result.push_back({entry.first, entry.first + static_cast<uint32_t>(entry.second.size())});
        
        if(result.size() == 3) {
            break;
        }
    }
    
    return result;
}