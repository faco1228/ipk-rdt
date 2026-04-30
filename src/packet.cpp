/**
 * @file packet.cpp
 * @author Samuel Fačka (xfackas00)
 * @brief Protocol packet encoding, decoding and CRC32 computation
 * @version 0.1
 * @date 2026-04-26
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. compute_crc32() calculates IEEE 802.3 CRC32 via a static lookup table
 *   2. encode() fills the Header struct, serialises header + payload to raw
 *      bytes, computes CRC32 over the full buffer and writes it back
 *   3. decode() validates length, payload_len, CRC32 and packet type;
 *      converts all fields from network to host byte order
 */

#include "packet.hpp"
#include <arpa/inet.h>
#include <cstring>

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief computes IEEE 802.3 CRC32 over a byte buffer
 *
 * The 256-entry lookup table is built once via a static lambda — thread-safe
 * in C++11. The checksum field in the header must be zeroed before this
 * function is called during verification.
 */
uint32_t Packet::compute_crc32(const uint8_t* data, size_t len) {
    static const auto table = []() {
        std::array<uint32_t, 256> t{};
        for (size_t i = 0; i < 256; ++i) {
            uint32_t crc = static_cast<uint32_t>(i);
            for (size_t j = 0; j < 8; ++j) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
            }
            t[i] = crc;
        }
        return t;
    }();

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ table[(crc & 0xFF) ^ data[i]];
    }
    return crc ^ 0xFFFFFFFFu;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief constructs a Packet with the given header fields and payload
 */
Packet::Packet(PacketType type, uint8_t flags, uint16_t connection_id, uint32_t seq_num, uint32_t ack_num, const std::vector<uint8_t>& payload)
    : type_(type), flags_(flags), connection_id_(connection_id), seq_num_(seq_num), ack_num_(ack_num), payload_(payload)
{}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief serialises the packet to raw bytes ready for sending
 */
std::vector<uint8_t> Packet::encode() const {
    Header head{};
    head.type = static_cast<uint8_t>(type_);
    head.flags = flags_;
    head.connection_id = htons(connection_id_);
    head.seq_num = htonl(seq_num_);
    head.ack_num = htonl(ack_num_);
    head.payload_len = htons(static_cast<uint16_t>(payload_.size()));
    head.reserved = 0;
    head.checksum = 0; // must be zero before CRC computation

    std::vector<uint8_t> raw(HEADER_SIZE + payload_.size());
    std::memcpy(raw.data(), &head, HEADER_SIZE);
    std::memcpy(raw.data() + HEADER_SIZE, payload_.data(), payload_.size());

    // compute CRC32 over the full packet and write back into the header
    head.checksum = htonl(compute_crc32(raw.data(), raw.size()));
    std::memcpy(raw.data(), &head, HEADER_SIZE);
    return raw;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief deserialises a packet from raw bytes and verifies integrity
 */
Packet Packet::decode(const std::vector<uint8_t>& raw) {
    if (raw.size() < HEADER_SIZE) {
        throw std::runtime_error("Packet too short");
    }

    Header head{};
    std::memcpy(&head, raw.data(), HEADER_SIZE);

    uint16_t payload_len = ntohs(head.payload_len);
    if (HEADER_SIZE + payload_len > raw.size()) {
        throw std::runtime_error("Truncated payload");
    }

    // zero checksum before recomputing, then compare against received value
    uint32_t received_crc = ntohl(head.checksum);
    head.checksum = 0;
    std::vector<uint8_t> buffer(raw.begin(), raw.begin() + HEADER_SIZE + payload_len);
    std::memcpy(buffer.data(), &head, HEADER_SIZE);
    if (received_crc != compute_crc32(buffer.data(), buffer.size())) {
        throw std::runtime_error("CRC32 mismatch");
    }

    if (head.type < 0x01 || head.type > 0x07) {
        throw std::runtime_error("Unknown packet type");
    }

    PacketType type = static_cast<PacketType>(head.type);
    std::vector<uint8_t> payload(raw.begin() + HEADER_SIZE, raw.begin() + HEADER_SIZE + payload_len);
    
    return Packet(type, head.flags, ntohs(head.connection_id), ntohl(head.seq_num), ntohl(head.ack_num), payload);
}