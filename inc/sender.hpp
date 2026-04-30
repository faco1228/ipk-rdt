/**
 * @file sender.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief client-side FSM: handshake, data transfer and teardown
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. run() resolves the server address, generates a random connection_id
 *      and calls handshake() -> send_data() -> teardown() in sequence
 *   2. handshake() performs the 3-way SYN/SYN_ACK/ACK exchange with retransmits
 *   3. send_data() reads input in MAX_PAYLOAD chunks, manages the sliding window
 *      and retransmits on RTO expiry
 *   4. teardown() sends FIN and waits for FIN_ACK with exponential backoff
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

/**
 * @brief client FSM that sends a file / stdin over the reliable UDP protocol
 */
class Sender {
public:
    /**
     * @brief constructs a Sender bound to the given configuration
     *
     * @param config fully parsed program configuration
     */
    explicit Sender(const Config& config);

    /**
     * @brief resolves server address and runs handshake -> send_data -> teardown
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
     * @brief parses SACK blocks from an ACK packet payload
     *
     * reads up to 3 blocks of 8 bytes each (start + end in network byte order)
     *
     * @param packet ACK packet that may carry SACK payload
     * @return list of [start, end) ranges, empty if no SACK flag
     */
    std::vector<std::pair<uint32_t, uint32_t>> parse_sack(const Packet& packet) const;

    /**
     * @brief performs the 3-way handshake (SYN -> SYN_ACK -> ACK)
     *
     * retransmits SYN with exponential backoff until SYN_ACK is received
     * or the global timeout expires
     *
     * @throws std::runtime_error on timeout or socket error
     */
    void handshake();

    /**
     * @brief transfers all input data using the sliding window
     *
     * reads from file or stdin, sends DATA packets, processes ACK+SACK,
     * retransmits on RTO expiry, tracks a global progress timer
     *
     * @throws std::runtime_error if no progress within timeout or socket error
     */
    void send_data();

    /**
     * @brief sends FIN and waits for FIN_ACK (teardown)
     *
     * retransmits FIN with exponential backoff until FIN_ACK is received
     * or the global timeout expires
     *
     * @throws std::runtime_error on timeout or socket error
     */
    void teardown();

    const Config& config_;
    UdpSocket socket_;
    SenderWindow window_;
    uint16_t connection_id_; // randomly chosen at the start of run()
    uint32_t next_seq_; // shared between send_data() and teardown()
};

#endif // SENDER_HPP