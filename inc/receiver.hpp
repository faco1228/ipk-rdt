/**
 * @file receiver.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-29
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef RECEIVER_HPP
#define RECEIVER_HPP

#include "config.hpp"
#include "socket.hpp"
#include "recv_buffer.hpp"
#include "packet.hpp"

#include <chrono>

class Receiver {
public:
    /**
     * @brief Construct a new Receiver object
     * 
     * @param config 
     */
    explicit Receiver(const Config& config);

    /**
     * @brief 
     * 
     */
    void run();

private:
    /**
     * @brief 
     * 
     * @param ... 
     * @return int 
     */
    static int elapsed_ms(const std::chrono::steady_clock::time_point& start);
    
    /**
     * @brief 
     * 
     */
    void send_ack();

    /**
     * @brief 
     * 
     */
    void handshake();

    /**
     * @brief 
     * 
     */
    void receive_data();

    const Config& config_;
    UdpSocket socket_;
    RecvBuffer buffer_;
    uint16_t connection_id_;
    sockaddr_storage client_addr_;
    socklen_t client_addr_len_;
};

#endif // RECEIVER_HPP