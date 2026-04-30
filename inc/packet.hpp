/**
 * @file packet.hpp
 * @author Samuel Fačka (xfackas00)
 * @brief Protocol packet definition, encoding, decoding and CRC32 integrity check
 * @version 0.1
 * @date 2026-04-26
 *
 * @copyright Copyright (c) 2026
 *
 * What happens here:
 *   1. packetType enumerates all protocol PDU types (SYN, DATA, FIN, ...)
 *   2. header maps directly onto the 20-byte wire format (packed, big-endian)
 *   3. packet wraps header fields and payload; encode() serialises to raw bytes
 *      and stamps CRC32, decode() deserialises and verifies integrity
 */

#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <array>

static constexpr size_t HEADER_SIZE = 20;
static constexpr size_t MAX_PAYLOAD = 1180;
static constexpr uint8_t FLAG_SACK = 0x01; // flags bit: SACK blocks present in payload

/**
 * @brief all PDU types used by the protocol
 *
 * values match the wire-format type field (1 byte)
 * SYN and FIN each consume one byte in the sequence number space
 */
enum class PacketType : uint8_t {
    SYN = 0x01,
    SYN_ACK = 0x02,
    DATA = 0x03,
    ACK = 0x04,
    FIN = 0x05,
    FIN_ACK = 0x06,
    RST = 0x07,
};

/**
 * @brief 20-byte wire-format header
 *
 * all multi-byte fields are in network byte order (big-endian)
 * packed to guarantee no padding between fields
 */
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

/**
 * @brief represents one protocol PDU with header fields and payload
 *
 * immutable after construction. encode() and decode() handle all
 * serialisation, byte-order conversion and integrity verification
 */
class Packet {
public:
    /**
     * @brief constructs a Packet with the given header fields and payload
     *
     * @param type PDU type
     * @param flags flags byte (FLAG_SACK or 0)
     * @param connection_id session identifier
     * @param seq_num sequence number
     * @param ack_num acknowledgement number
     * @param payload optional payload bytes (default: empty)
     */
    explicit Packet(PacketType type, uint8_t flags, uint16_t connection_id,
                    uint32_t seq_num, uint32_t ack_num,
                    const std::vector<uint8_t>& payload = {});

    /**
     * @brief deserialises a packet from raw bytes and verifies integrity
     *
     * checks length, payload_len, CRC32 and packet type
     * converts all multi-byte fields from network to host byte order
     *
     * @param raw raw bytes received from the socket
     * @return decoded Packet with host-order fields
     * @throws std::runtime_error if packet is too short, truncated,
     *         CRC32 mismatches or type is unknown
     */
    static Packet decode(const std::vector<uint8_t>& raw);

    /**
     * @brief serialises the packet to raw bytes ready for sending
     *
     * fills the Header, copies payload, computes CRC32 over the full
     * buffer and writes it back into the checksum field
     *
     * @return raw bytes in wire format (header + payload, CRC32 stamped)
     */
    std::vector<uint8_t> encode() const;

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
     * @brief computes IEEE 802.3 CRC32 over a byte buffer
     *
     * lookup table is initialised once via a static lambda (thread-safe in C++11)
     * the checksum field must be zeroed before calling this during verification
     *
     * @param data pointer to the buffer
     * @param len  number of bytes to process
     * @return CRC32 checksum
     */
    static uint32_t compute_crc32(const uint8_t* data, size_t len);
};

#endif // PACKET_HPP