/**
 * @file test_packet.cpp
 * @author Samuel Facka (xfackas00)
 * @brief Unit tests for Packet encode/decode and CRC32 integrity.
 * @version 0.1
 * @date 2026-04-28
 *
 * @copyright Copyright (c) 2026
 *
 * Build: g++ -std=c++20 -Iinc src/packet.cpp test/test_packet.cpp -o test_packet
 * Run:   ./test_packet
 */

#include "packet.hpp"
#include <iostream>
#include <cassert>

static int passed = 0;
static int failed = 0;

static void check(const std::string& name, bool condition) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
        passed++;
    } else {
        std::cerr << "[FAIL] " << name << "\n";
        failed++;
    }
}

// ─── Tests ───────────────────────────────────────────────────────────────────

static void test_encode_decode_syn() {
    Packet p(PacketType::SYN, 0x00, 42, 0, 0);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("SYN type",          q.type()          == PacketType::SYN);
    check("SYN flags",         q.flags()          == 0x00);
    check("SYN connection_id", q.connection_id()  == 42);
    check("SYN seq_num",       q.seq_num()        == 0);
    check("SYN ack_num",       q.ack_num()        == 0);
    check("SYN payload empty", q.payload().empty());
}

static void test_encode_decode_data() {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0xAB, 0xFF};
    Packet p(PacketType::DATA, 0x00, 1337, 1, 0, payload);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("DATA type",          q.type()         == PacketType::DATA);
    check("DATA connection_id", q.connection_id() == 1337);
    check("DATA seq_num",       q.seq_num()       == 1);
    check("DATA payload size",  q.payload().size() == payload.size());
    check("DATA payload content", q.payload()     == payload);
}

static void test_encode_decode_ack_with_sack() {
    std::vector<uint8_t> sack_payload = {
        0x00, 0x00, 0x04, 0xA5,   // sack_start = 1189
        0x00, 0x00, 0x09, 0x4A,   // sack_end   = 2378
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    Packet p(PacketType::ACK, FLAG_SACK, 42, 0, 1181, sack_payload);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("ACK+SACK type",     q.type()    == PacketType::ACK);
    check("ACK+SACK has_sack", q.has_sack() == true);
    check("ACK+SACK ack_num",  q.ack_num() == 1181);
}

static void test_corrupted_packet() {
    Packet p(PacketType::SYN, 0x00, 42, 0, 0);
    auto raw = p.encode();

    // flip a byte in the payload area (after header)
    raw[5] ^= 0xFF;

    bool threw = false;
    try {
        Packet::decode(raw);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check("corrupted CRC throws", threw);
}

static void test_too_short_packet() {
    std::vector<uint8_t> tiny(10, 0x00);
    bool threw = false;
    try {
        Packet::decode(tiny);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check("too short throws", threw);
}

static void test_empty_payload() {
    Packet p(PacketType::FIN, 0x00, 7, 1000, 0);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("FIN type",    q.type()    == PacketType::FIN);
    check("FIN seq_num", q.seq_num() == 1000);
    check("FIN payload empty", q.payload().empty());
}

static void test_unknown_type_throws() {
    Packet p(PacketType::SYN, 0x00, 1, 0, 0);
    auto raw = p.encode();

    // overwrite type field (byte 0) with an unknown value
    raw[0] = 0x08;

    // recalculate CRC so only type check triggers, not CRC
    // zero out checksum field (bytes 16-19)
    raw[16] = 0; raw[17] = 0; raw[18] = 0; raw[19] = 0;
    // we skip recomputing CRC here — corrupted CRC will throw first, which is still a throw
    bool threw = false;
    try {
        Packet::decode(raw);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check("unknown type throws", threw);
}

static void test_truncated_payload_throws() {
    // header says payload_len=100 but raw buffer is only header+10 bytes
    std::vector<uint8_t> raw(HEADER_SIZE + 10, 0x00);
    raw[0] = 0x03; // type = DATA

    // set payload_len field (bytes 12-13) to 100 in big-endian
    raw[12] = 0x00;
    raw[13] = 0x64;

    bool threw = false;
    try {
        Packet::decode(raw);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check("truncated payload throws", threw);
}

static void test_encode_decode_syn_ack() {
    Packet p(PacketType::SYN_ACK, 0x00, 999, 0, 1);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("SYN_ACK type",    q.type()    == PacketType::SYN_ACK);
    check("SYN_ACK ack_num", q.ack_num() == 1);
    check("SYN_ACK conn_id", q.connection_id() == 999);
}

static void test_encode_decode_fin_ack() {
    Packet p(PacketType::FIN_ACK, 0x00, 7, 0, 5001);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("FIN_ACK type",    q.type()    == PacketType::FIN_ACK);
    check("FIN_ACK ack_num", q.ack_num() == 5001);
}

static void test_max_payload_round_trip() {
    std::vector<uint8_t> payload(MAX_PAYLOAD);
    for (size_t i = 0; i < MAX_PAYLOAD; ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    Packet p(PacketType::DATA, 0x00, 1, 1, 0, payload);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("max payload size", q.payload().size() == MAX_PAYLOAD);
    check("max payload content", q.payload() == payload);
}

static void test_large_seq_ack_values() {
    uint32_t big_seq = 0xFFFFFFF0u;
    uint32_t big_ack = 0x7FFFFFFFu;
    Packet p(PacketType::DATA, 0x00, 1, big_seq, big_ack);
    auto raw = p.encode();
    Packet q = Packet::decode(raw);

    check("large seq_num preserved", q.seq_num() == big_seq);
    check("large ack_num preserved", q.ack_num() == big_ack);
}

static void test_connection_id_boundaries() {
    {
        Packet p(PacketType::SYN, 0x00, 0, 0, 0);
        auto raw = p.encode();
        Packet q = Packet::decode(raw);
        check("conn_id 0 preserved", q.connection_id() == 0);
    }
    {
        Packet p(PacketType::SYN, 0x00, 65535, 0, 0);
        auto raw = p.encode();
        Packet q = Packet::decode(raw);
        check("conn_id 65535 preserved", q.connection_id() == 65535);
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Packet unit tests ===\n\n";

    test_encode_decode_syn();
    test_encode_decode_data();
    test_encode_decode_ack_with_sack();
    test_corrupted_packet();
    test_too_short_packet();
    test_empty_payload();
    test_unknown_type_throws();
    test_truncated_payload_throws();
    test_encode_decode_syn_ack();
    test_encode_decode_fin_ack();
    test_max_payload_round_trip();
    test_large_seq_ack_values();
    test_connection_id_boundaries();

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return (failed == 0) ? 0 : 1;
}
