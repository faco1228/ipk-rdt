/**
 * @file sender.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef SENDER_HPP
#define SENDER_HPP

#include "config.hpp"
#include "socket.hpp"
#include "sender_window.hpp"
#include "packet.hpp"

#include <cstdint>
#include <chrono>
#include <utility>
#include <vector>

class Sender {
public:
    /**
     * @brief Construct a new Sender object
     * 
     * @param config 
     */
    explicit Sender(const Config& config);

    /**
     * @brief 
     * 
     */
    void run();

private:
    /**
     * @brief 
     * 
     * @param start 
     * @return int 
     */
    static int elapsed_ms(const std::chrono::steady_clock::time_point& start);
    
    /**
     * @brief 
     * 
     * @param packet 
     * @return std::vector<std::pair<uint32_t, uint32_t>> 
     */
    std::vector<std::pair<uint32_t, uint32_t>> parse_sack(const Packet& packet) const;
   
    /**
     * @brief 
     * 
     */
    void handshake();

    /**
     * @brief 
     * 
     */
    void send_data();

    /**
     * @brief 
     * 
     */
    void teardown();

    const Config& config_;
    UdpSocket socket_;
    SenderWindow window_;
    uint16_t connection_id_;
    uint32_t next_seq_;
};

#endif // SENDER_HPP