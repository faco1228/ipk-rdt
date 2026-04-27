/**
 * @file packet.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "packet.hpp"
#include <arpa/inet.h>
#include <cstring>


/**
 * @brief 
 * 
 * @param data 
 * @param len 
 * @return uint32_t 
 */
uint32_t Packet::compute_crc32(const uint8_t* data, size_t len) {
    // lookup-table
    static const auto table  = []() {
        std::array<uint32_t, 256> t{};
        for (size_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (size_t j = 0; j < 8; ++j) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);    
            }
            t[i] = crc;
        }
        return t;
    }();


    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ table[(crc & 0xFF) ^ data[i]];
    }

    return crc ^ 0xFFFFFFFF;
}
/**
 * @brief Construct a new Packet:: Packet object
 * 
 * @param type 
 * @param flags 
 * @param connection_id 
 * @param seq_num 
 * @param ack_num 
 * @param payload 
 */
Packet::Packet(PacketType type, uint8_t flags, uint16_t connection_id, uint32_t seq_num, uint32_t ack_num, const std::vector<uint8_t>& payload)
    : type_(type), flags_(flags), connection_id_(connection_id), seq_num_(seq_num), ack_num_(ack_num), payload_(payload)
{}

/**
 * @brief 
 * 
 * @return std::vector<uint8_t> 
 */
std::vector<uint8_t> Packet::encode() const {
    // fill up the header
    Header head;
    head.type = (uint8_t)type_;
    head.flags = flags_;
    head.connection_id = htons(connection_id_);
    head.seq_num = htonl(seq_num_);
    head.ack_num = htonl(ack_num_);
    head.payload_len = htons(payload_.size());
    head.reserved = 0;
    head.checksum = 0;

    // copy header into buffer
    std::vector<uint8_t> raw = std::vector<uint8_t>(HEADER_SIZE + payload_.size());
    std::memcpy(raw.data(), &head, HEADER_SIZE);

    // copy payload after the header
    std::memcpy(raw.data() + HEADER_SIZE, payload_.data(), payload_.size());

    // compute CRC32, and write back
    uint32_t crc = compute_crc32(raw.data(), raw.size());
    head.checksum = htonl(crc);
    std::memcpy(raw.data(), &head, HEADER_SIZE);
    return raw;
}

Packet Packet::decode(const std::vector<uint8_t>& raw){
    // check for minimal lenght
    if (raw.size() < HEADER_SIZE) {
        throw std::runtime_error("Packet too short");
    }

    // load header
    Header head;
    std::memcpy(&head, raw.data(), HEADER_SIZE);

    // check payload_len
    uint16_t payload_len = ntohs(head.payload_len);
    if (HEADER_SIZE + payload_len > raw.size()) {
        throw std::runtime_error("Truncated payload");
    }

    // check CRC32
    uint32_t received_crc = ntohl(head.checksum);
    head.checksum = 0;
    std::vector<uint8_t> buf(raw.begin(), raw.begin() + HEADER_SIZE + payload_len);
    std::memcpy(buf.data(), &head, HEADER_SIZE);
    uint32_t computed_crc = compute_crc32(buf.data(), buf.size());
    if (received_crc != computed_crc) {
        throw std::runtime_error("CRC32 mismatch");
    }

    // check type
    PacketType type = static_cast<PacketType>(head.type);
    if (head.type < 0x01 || head.type > 0x07) {
        throw std::runtime_error("Uknown packet type");
    }

    // build payload and return
    std::vector<uint8_t> payload(raw.begin() + HEADER_SIZE, raw.begin() + HEADER_SIZE + payload_len);
    return Packet(type, head.flags, ntohs(head.connection_id), ntohl(head.seq_num), ntohl(head.ack_num), payload);
} 