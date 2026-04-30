/**
 * @file recv_buffer.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief reorder buffer for the receiver with cumulative ACK and SACK generation
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. insert() stores incoming segments in a sorted map, ignoring duplicates
 *      and already delivered data (seq < exp_next_)
 *   2. flush() delivers contiguous in-order data and advances exp_next_
 *   3. get_ack_num() returns exp_next_ as the cumulative ACK value
 *   4. get_sack_blocks() returns up to 3 out-of-order ranges for the ACK packet
 */

#ifndef RECV_BUFFER_HPP
#define RECV_BUFFER_HPP

#include <vector>
#include <cstdint>
#include <utility>
#include <map>

/**
 * @brief reorder buffer for the receiver side
 *
 * uses std::map for automatic order by sequence number
 */
class RecvBuffer {
public:
    /**
     * @brief inserts a segment into the reorder buffer
     *
     * ignores segments with seq < exp_next_ (delivered)
     * and duplicates
     *
     * @param seq sequence number of the segment
     * @param data payload bytes
     */
    void insert(uint32_t seq, const std::vector<uint8_t>& data);

    /**
     * @brief delivers all contiguous in-order data and advances exp_next_
     *
     * @return concatenated in-order bytes ready to write to output
     */
    std::vector<uint8_t> flush();

    /**
     * @brief returns the next expected byte (cumulative ACK value)
     *
     * @return exp_next_
     */
    uint32_t get_ack_num() const;

    /**
     * @brief returns up to 3 SACK blocks for out-of-order segments
     *
     * each block is [start, end) covering one buffered segment
     *
     * @return list of at most 3 SACK ranges
     */
    std::vector<std::pair<uint32_t, uint32_t>> get_sack_blocks() const;

private:
    std::map<uint32_t, std::vector<uint8_t>> buffer_; // out-of-order segments keyed by seq
    uint32_t exp_next_ = 1; // next expected byte (SYN consumed byte 0)
};

#endif // RECV_BUFFER_HPP