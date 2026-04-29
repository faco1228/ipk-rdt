/**
 * @file recv_buffer.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef RECV_BUFFER_HPP
#define RECV_BUFFER_HPP

#include <vector>
#include <cstdint>
#include <utility>
#include <map>

class RecvBuffer {
public:
    /**
     * @brief 
     * insert segment into buffer
     * @param seq 
     * @param data 
     */
    void insert(uint32_t seq, const std::vector<uint8_t>& data);

    /**
     * @brief 
     * return every in-order data, from exp_next_
     * @return std::vector<uint8_t> 
     */
    std::vector<uint8_t> flush();

    /**
     * @brief Get the ack num object
     * 
     * @return uint32_t 
     */
    uint32_t get_ack_num() const;

    /**
     * @brief Get the sack blocks object
     * 
     * @return std::vector<std::pair<uint32_t, uint32_t>> 
     */
    std::vector<std::pair<uint32_t, uint32_t>> get_sack_blocks() const;

private:
    std::map<uint32_t, std::vector<uint8_t>> buffer_; // out-of-order segments
    uint32_t exp_next_ = 1; // next expected byte
};

#endif // RECV_BUFFER_HPP