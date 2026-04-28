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

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Packet unit tests ===\n\n";

    test_encode_decode_syn();
    test_encode_decode_data();
    test_encode_decode_ack_with_sack();
    test_corrupted_packet();
    test_too_short_packet();
    test_empty_payload();

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return (failed == 0) ? 0 : 1;
}
