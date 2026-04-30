/**
 * @file receiver.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief server-side FSM implementation: handshake, data reception and teardown
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. elapsed_ms() measures time since a given steady_clock point
 *   2. send_ack() builds an ACK with cumulative ack_num + optional SACK payload
 *   3. handshake() polls for SYN, stores client_addr_ and sends SYN_ACK
 *   4. receive_data() processes DATA/FIN/SYN packets, flushes in-order data
 *      to output and sends ACK+SACK, handles FIN arriving before all DATA
 *   5. run() binds the socket and sequences handshake -> receive_data
 */

#include "receiver.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <cstring>
#include <arpa/inet.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns milliseconds elapsed since start
 */
int Receiver::elapsed_ms(const std::chrono::steady_clock::time_point& start) {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief builds and sends an ACK packet to the client
 */
void Receiver::send_ack() {
    std::vector<std::pair<uint32_t, uint32_t>> sack_blocks = buffer_.get_sack_blocks();
    std::vector<uint8_t> payload;
    uint8_t flags;

    if (sack_blocks.empty()) {
        flags = 0;
    }
    else {
        flags = FLAG_SACK;
        for (const auto& block : sack_blocks) {
            uint32_t start = htonl(block.first);
            uint32_t end = htonl(block.second);
            uint8_t tmp[4];

            std::memcpy(tmp, &start, 4);
            payload.insert(payload.end(), tmp, tmp + 4);
            std::memcpy(tmp, &end, 4);
            payload.insert(payload.end(), tmp, tmp + 4);
        }
    }

    Packet packet(PacketType::ACK, flags, connection_id_, 0, buffer_.get_ack_num(), payload);
    socket_.send_to(packet.encode(), client_addr_, client_addr_len_);
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief waits for SYN, records the client address and sends SYN_ACK
 */
void Receiver::handshake() {
    int global_ms = config_.timeout * 1000;
    int max_rto_ms = global_ms;
    int rto_ms = global_ms / 4;

    std::chrono::steady_clock::time_point global_start = std::chrono::steady_clock::now();

    struct pollfd pfd{};
    pfd.fd = socket_.fd();
    pfd.events = POLLIN;

    while (true) {
        if (elapsed_ms(global_start) >= global_ms) {
            throw std::runtime_error("Handshake timeout");
        }

        int wait = std::min(rto_ms, global_ms - elapsed_ms(global_start));
        int rc = poll(&pfd, 1, wait);

        if (rc < 0) {
            throw std::runtime_error(std::string("poll: ") + strerror(errno));
        }

        if (rc > 0 && (pfd.revents & POLLIN)) {
            std::vector<uint8_t> buffer;
            sockaddr_storage src{};
            socklen_t src_len = sizeof(src);
            socket_.receive(buffer, src, src_len);

            try {
                Packet packet = Packet::decode(buffer);

                if (packet.type() != PacketType::SYN) { continue; }

                connection_id_ = packet.connection_id();
                client_addr_ = src;
                client_addr_len_ = src_len;

                Packet syn_ack(PacketType::SYN_ACK, 0, connection_id_, 0, 1);
                socket_.send_to(syn_ack.encode(), client_addr_, client_addr_len_);
                return;
            }
            catch (...) { continue; }
        }
        else {
            rto_ms = std::min(rto_ms * 2, max_rto_ms);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief receives DATA and FIN packets and writes output
 */
void Receiver::receive_data() {
    std::ostream* output = &std::cout;
    std::ofstream file;

    if (!config_.file_out.empty()) {
        file.open(config_.file_out, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open output file: " + config_.file_out);
        }
        output = &file;
    }

    int global_ms = config_.timeout * 1000;
    bool fin_received = false;
    uint32_t fin_seq = 0;
    std::chrono::steady_clock::time_point progress = std::chrono::steady_clock::now();

    struct pollfd pfd{};
    pfd.fd = socket_.fd();
    pfd.events = POLLIN;

    while (true) {
        if (elapsed_ms(progress) >= global_ms) {
            throw std::runtime_error("No progress timeout");
        }

        int wait = global_ms - elapsed_ms(progress);
        int rc = poll(&pfd, 1, wait);

        if (rc < 0) {
            throw std::runtime_error(std::string("poll: ") + strerror(errno));
        }

        if (rc > 0 && (pfd.revents & POLLIN)) {
            std::vector<uint8_t> buffer;
            sockaddr_storage src{};
            socklen_t src_len = sizeof(src);
            socket_.receive(buffer, src, src_len);

            try {
                Packet packet = Packet::decode(buffer);

                if (packet.connection_id() != connection_id_) { continue; }

                if (packet.type() == PacketType::SYN) {
                    // duplicate SYN — retransmit SYN_ACK
                    Packet syn_ack(PacketType::SYN_ACK, 0, connection_id_, 0, 1);
                    socket_.send_to(syn_ack.encode(), client_addr_, client_addr_len_);
                    continue;
                }

                if (packet.type() == PacketType::DATA) {
                    uint32_t old_ack = buffer_.get_ack_num();
                    buffer_.insert(packet.seq_num(), packet.payload());
                    std::vector<uint8_t> data = buffer_.flush();

                    if (!data.empty()) {
                        output->write(reinterpret_cast<const char*>(data.data()), data.size());
                    }

                    send_ack();

                    if (buffer_.get_ack_num() > old_ack) {
                        progress = std::chrono::steady_clock::now();
                    }

                    // FIN was received earlier, check if the gap is now filled
                    if (fin_received && buffer_.get_ack_num() == fin_seq) {
                        Packet fin_ack(PacketType::FIN_ACK, 0, connection_id_, 0, fin_seq + 1);
                        socket_.send_to(fin_ack.encode(), client_addr_, client_addr_len_);
                        return;
                    }
                }

                if (packet.type() == PacketType::FIN) {
                    fin_seq = packet.seq_num();
                    fin_received = true;

                    if (buffer_.get_ack_num() == fin_seq) {
                        Packet fin_ack(PacketType::FIN_ACK, 0, connection_id_, 0, fin_seq + 1);
                        socket_.send_to(fin_ack.encode(), client_addr_, client_addr_len_);
                        return;
                    }
                }

                if (packet.type() == PacketType::ACK) { continue; }
            }
            catch (...) { continue; }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief constructs a Receiver bound to the given configuration
 */
Receiver::Receiver(const Config& config)
    : config_(config), socket_(), buffer_(), connection_id_(0), client_addr_{}, client_addr_len_(0)
{}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief binds the socket and runs handshake -> receive_data
 */
void Receiver::run() {
    socket_.bind(config_.address, std::to_string(config_.port));

    handshake();
    receive_data();
}