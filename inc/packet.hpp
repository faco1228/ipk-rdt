/**
 * @file packet.hpp
 * @author your name (you@domain.com)
 * @brief Protocol packet definition, encoding, decoding, and integrity check.
 * @version 0.1
 * @date 2026-04-26
 * 
 * @copyright Copyright (c) 2026
 * 
 * Functionality:
 * enum PacketType - definition of all protocol packet type
 * header struct - mapping to 20B wire format
 * packet class - warpping header and payload, will handle encode/decode/CRC32
 * 
 */

#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <array>

// const
static constexpr size_t HEADER_SIZE = 20; // bytes
static constexpr size_t MAX_PAYLOAD = 1180;
static constexpr uint8_t FLAG_SACK = 0x01;

// packet type
enum class PacketType : uint8_t {
    SYN = 0x01,
    SYN_ACK = 0x02,
    DATA = 0x03,
    ACK = 0x04,
    FIN = 0x05,
    FIN_ACK = 0x06,
    RST = 0x07,
};

// header (wire format - 20B)
struct Header {
    uint8_t type; 
    uint8_t flags;
    uint16_t connection_id;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t payload_len;
    uint16_t reserved;
    uint32_t checksum;
} __attribute__((packed));

// packet class
class Packet {
public:
    /**
     * @brief 
     * 
     */
    explicit Packet(PacketType type, uint8_t flags, uint16_t connection_id, uint32_t seq_num, uint32_t ack_num, const std::vector<uint8_t>& payload = {});

    /**
     * @brief 
     * 
     */
    static Packet decode(const std::vector<uint8_t>& data);

    /**
     * @brief 
     * 
     */
    std::vector<uint8_t> encode() const;

    //getters
    PacketType type() const { return type_; }
    uint8_t flags() const { return flags_; }
    uint16_t connection_id() const { return connection_id_; }
    uint32_t seq_num() const { return seq_num_; }
    uint32_t ack_num() const { return ack_num_; }
    const std::vector<uint8_t>& payload() const { return payload_; }
    bool has_sack() const { return flags_ & FLAG_SACK; }

private:
    PacketType type_;
    uint8_t flags_;
    uint16_t connection_id_;
    uint32_t seq_num_;
    uint32_t ack_num_;
    std::vector<uint8_t> payload_;

    /**
     * @brief 
     * 
     */
    static uint32_t compute_crc32(const uint8_t* data, size_t len);
};

#endif // PACKET_HPP