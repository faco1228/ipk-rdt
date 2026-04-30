/**
 * @file receiver.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief server-side FSM: handshake, data reception and teardown
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. run() binds the socket and calls handshake() -> receive_data() in sequence
 *   2. handshake() waits for SYN, saves the client address and sends SYN_ACK
 *   3. receive_data() processes DATA and FIN packets, writes output, sends ACK+SACK
 *   4. send_ack() builds an ACK packet with cumulative ack_num and optional SACK payload
 */

#ifndef RECEIVER_HPP
#define RECEIVER_HPP

#include "config.hpp"
#include "socket.hpp"
#include "recv_buffer.hpp"
#include "packet.hpp"

#include <chrono>

/**
 * @brief server FSM that receives a file or stream over the reliable UDP protocol
 */
class Receiver {
public:
    /**
     * @brief constructs a Receiver bound to the given configuration
     *
     * @param config fully parsed program configuration
     */
    explicit Receiver(const Config& config);

    /**
     * @brief binds the socket and runs handshake -> receive_data
     *
     * @throws std::runtime_error on timeout, socket error or file open failure
     */
    void run();

private:
    /**
     * @brief returns milliseconds elapsed since start
     *
     * @param start reference time point
     * @return elapsed time in ms
     */
    static int elapsed_ms(const std::chrono::steady_clock::time_point& start);

    /**
     * @brief builds and sends an ACK packet to the client
     *
     * includes SACK payload if there are out of order segments in the buffer
     * each SACK block is serialised as 8 bytes (htonl(start) + htonl(end))
     */
    void send_ack();

    /**
     * @brief waits for SYN, records the client address and sends SYN_ACK
     *
     * @throws std::runtime_error on timeout or socket error
     */
    void handshake();

    /**
     * @brief receives DATA and FIN packets and writes output
     *
     * inserts segments into RecvBuffer, flushes in order data to output,
     * sends ACK+SACK after each DATA, handles FIN arriving before all DATA,
     * retransmits SYN_ACK on duplicate SYN
     *
     * @throws std::runtime_error if no progress within timeout or socket error
     */
    void receive_data();

    const Config& config_;
    UdpSocket socket_;
    RecvBuffer buffer_;
    uint16_t connection_id_;
    sockaddr_storage client_addr_; // filled during handshake, used by send_ack()
    socklen_t client_addr_len_;
};

#endif // RECEIVER_HPP