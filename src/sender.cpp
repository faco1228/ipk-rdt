/**
 * @file sender.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief client-side FSM implementation: handshake, data transfer and teardown
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. elapsed_ms() measures time since a given steady_clock point
 *   2. parse_sack() reads up to 3 SACK blocks from an ACK packet payload
 *   3. handshake() sends SYN, waits for SYN_ACK, retransmits with backoff
 *   4. send_data() fills the sliding window from input, processes ACK+SACK,
 *      retransmits timed-out segments and tracks a global progress timer
 *   5. teardown() sends FIN, waits for FIN_ACK, retransmits with backoff
 *   6. run() resolves the server, generates connection_id and sequences the phases
 */

#include "sender.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <poll.h>
#include <stdexcept>
#include <random>
#include <string>
#include <cstring>
#include <arpa/inet.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief returns milliseconds elapsed since start
 */
int Sender::elapsed_ms(const std::chrono::steady_clock::time_point& start) {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief parses SACK blocks from an ACK packet payload
 */
std::vector<std::pair<uint32_t, uint32_t>> Sender::parse_sack(const Packet& packet) const {
    if (!packet.has_sack()) {
        return {};
    }

    const std::vector<uint8_t>& payload = packet.payload();
    std::vector<std::pair<uint32_t, uint32_t>> blocks;
    size_t offset = 0;

    while (offset + 8 <= payload.size() && blocks.size() < 3) {
        uint32_t start = 0;
        uint32_t end = 0;

        std::memcpy(&start, payload.data() + offset, 4);
        std::memcpy(&end, payload.data() + offset + 4, 4);
        blocks.push_back({ntohl(start), ntohl(end)});
        offset += 8;
    }
    return blocks;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief performs the 3-way handshake (SYN -> SYN_ACK -> ACK)
 */
void Sender::handshake() {
    int global_ms = config_.timeout * 1000;
    int max_rto_ms = global_ms;
    int rto_ms = global_ms / 4;

    Packet syn(PacketType::SYN, 0, connection_id_, 0, 0);
    socket_.send(syn.encode());

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

                if (packet.type() != PacketType::SYN_ACK) { continue; }
                if (packet.connection_id() != connection_id_) { continue; }
                if (packet.ack_num() != 1) { continue; }

                Packet ack(PacketType::ACK, 0, connection_id_, 0, 1);
                socket_.send(ack.encode());
                return;
            }
            catch (...) { continue; }
        }
        else {
            // timeout
            socket_.send(syn.encode());
            rto_ms = std::min(rto_ms * 2, max_rto_ms);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief transfers all input data using the sliding window
 */
void Sender::send_data() {
    std::istream* input = &std::cin;
    std::ifstream file;

    if (!config_.file_in.empty()) {
        file.open(config_.file_in, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open input file: " + config_.file_in);
        }
        input = &file;
    }

    int global_ms = config_.timeout * 1000;
    bool eof = false;
    uint32_t last_ack = 1;
    std::chrono::steady_clock::time_point progress = std::chrono::steady_clock::now();

    struct pollfd pfd{};
    pfd.fd = socket_.fd();
    pfd.events = POLLIN;

    while (true) {
        // fill the window with new segments from input
        while (!eof && window_.has_capacity()) {
            std::vector<uint8_t> chunk(MAX_PAYLOAD);
            input->read(reinterpret_cast<char*>(chunk.data()), MAX_PAYLOAD);
            std::streamsize n = input->gcount();

            if (n == 0) {
                eof = true;
                break;
            }

            chunk.resize(n);
            Packet data(PacketType::DATA, 0, connection_id_, next_seq_, 0, chunk);
            socket_.send(data.encode());
            window_.push(next_seq_, chunk);
            next_seq_ += static_cast<uint32_t>(n);
        }

        if (eof && window_.all_acked()) {
            break;
        }
        if (elapsed_ms(progress) >= global_ms) {
            throw std::runtime_error("No progress timeout");
        }

        int rc = poll(&pfd, 1, window_.next_timeout());
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

                if (packet.type() != PacketType::ACK) { continue; }
                if (packet.connection_id() != connection_id_) { continue; }

                auto sack = parse_sack(packet);
                window_.ack_received(packet.ack_num(), sack);

                if (packet.ack_num() > last_ack) {
                    last_ack = packet.ack_num();
                    progress = std::chrono::steady_clock::now();
                }
            }
            catch (...) { continue; }
        }
        else {
            // RTO expired
            std::vector<Segment> timed_out = window_.get_timed_out();
            for (const Segment& seg : timed_out) {
                Packet data(PacketType::DATA, 0, connection_id_, seg.seq_num, 0, seg.data);
                socket_.send(data.encode());
                window_.mark_retransmitted(seg.seq_num);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief sends FIN and waits for FIN_ACK (session teardown)
 */
void Sender::teardown() {
    int global_ms = config_.timeout * 1000;
    int max_rto_ms = global_ms;
    int rto_ms = global_ms / 4;

    Packet fin(PacketType::FIN, 0, connection_id_, next_seq_, 0);
    socket_.send(fin.encode());

    std::chrono::steady_clock::time_point global_start = std::chrono::steady_clock::now();

    struct pollfd pfd{};
    pfd.fd = socket_.fd();
    pfd.events = POLLIN;

    while (true) {
        if (elapsed_ms(global_start) >= global_ms) {
            throw std::runtime_error("Teardown timeout");
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

                if (packet.type() != PacketType::FIN_ACK) { continue; }
                if (packet.connection_id() != connection_id_) { continue; }
                if (packet.ack_num() != next_seq_ + 1) { continue; }

                return;
            }
            catch (...) { continue; }
        }
        else {
            // timeout
            socket_.send(fin.encode());
            rto_ms = std::min(rto_ms * 2, max_rto_ms);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief constructs a Sender bound to the given configuration
 */
Sender::Sender(const Config& config)
    : config_(config), socket_(), window_(config.timeout), connection_id_(0), next_seq_(1)
{}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief resolves server address and runs handshake -> send_data -> teardown
 */
void Sender::run() {
    socket_.resolve(config_.address, std::to_string(config_.port));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(1, 65535);
    connection_id_ = dist(gen);

    handshake();
    send_data();
    teardown();
}