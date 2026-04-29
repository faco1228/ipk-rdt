/**
 * @file sender_window.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef SENDER_WINDOW_HPP
#define SENDER_WINDOW_HPP

#include <vector>
#include <cstdint>
#include <utility>
#include <chrono>

struct Segment {
    uint32_t seq_num;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point send_time;
    int rto_ms; // retransmission timeout
    bool acked; // SACK check
};

class SenderWindow {
public:
    /**
     * @brief Construct a new Sender Window object
     * 
     * @param timeout 
     */
    explicit SenderWindow(int timeout);

    /**
     * @brief 
     * 
     * @param seq 
     * @param data 
     */
    void push(uint32_t seq, const std::vector<uint8_t>& data);

    /**
     * @brief 
     * 
     * @param ack_num 
     * @param sack_blocks 
     */
    void ack_received(uint32_t ack_num, const std::vector<std::pair<uint32_t, uint32_t>>& sack_blocks);

    /**
     * @brief Get the timeout object
     * 
     * @return std::vector<Segment> 
     */
    std::vector<Segment> get_timed_out();

    /**
     * @brief 
     * doubles RTO for Segment
     * @param seq 
     */
    void mark_retransmitted(uint32_t seq);

    /**
     * @brief 
     * 
     * @return true 
     * @return false 
     */
    bool has_capacity() const;

    /**
     * @brief 
     * 
     * @return true 
     * @return false 
     */
    bool all_acked() const;

    /**
     * @brief 
     * for poll()
     * @return int 
     */
    int next_timeout() const;

private:
    std::vector<Segment> segments_;
    int initial_rto_ms_;
    int max_rto_ms_;
    static constexpr int MAX_WINDOW = 32;
};

#endif // SENDER_WINDOW_HPP