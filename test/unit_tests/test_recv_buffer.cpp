/**
 * @file test_recv_buffer.cpp
 * @author Samuel Facka (xfackas00)
 * @brief Unit tests for RecvBuffer reorder buffer logic.
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * Build: g++ -std=c++20 -Iinc src/recv_buffer.cpp test/unit_tests/test_recv_buffer.cpp -o test_recv_buffer
 * Run:   ./test_recv_buffer
 */

#include "recv_buffer.hpp"
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

static void test_initial_state() {
    RecvBuffer buf;
    check("initial ack_num is 1", buf.get_ack_num() == 1);
    check("initial flush is empty", buf.flush().empty());
    check("initial sack_blocks empty", buf.get_sack_blocks().empty());
}

static void test_in_order_delivery() {
    RecvBuffer buf;
    std::vector<uint8_t> d1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> d2 = {0x04, 0x05, 0x06};

    buf.insert(1, d1);
    buf.insert(4, d2);

    auto data = buf.flush();
    check("in-order flush size", data.size() == 6);
    check("in-order flush content", data == std::vector<uint8_t>({0x01,0x02,0x03,0x04,0x05,0x06}));
    check("ack_num after flush", buf.get_ack_num() == 7);
}

static void test_out_of_order_delivery() {
    RecvBuffer buf;
    std::vector<uint8_t> d1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> d2 = {0x04, 0x05, 0x06};

    // insert out of order
    buf.insert(4, d2);   // seq=4, but next expected is 1
    auto data1 = buf.flush();
    check("out-of-order flush empty before gap filled", data1.empty());
    check("ack_num unchanged", buf.get_ack_num() == 1);

    // now insert missing segment
    buf.insert(1, d1);
    auto data2 = buf.flush();
    check("flush after gap filled", data2.size() == 6);
    check("ack_num after gap filled", buf.get_ack_num() == 7);
}

static void test_duplicate_ignored() {
    RecvBuffer buf;
    std::vector<uint8_t> data = {0xAA, 0xBB};

    buf.insert(1, data);
    buf.insert(1, data);   // duplicate

    auto result = buf.flush();
    check("duplicate ignored, size correct", result.size() == 2);
    check("ack_num correct after duplicate", buf.get_ack_num() == 3);
}

static void test_old_segment_ignored() {
    RecvBuffer buf;
    std::vector<uint8_t> data = {0x01, 0x02};

    buf.insert(1, data);
    buf.flush();   // exp_next_ = 3

    // insert segment before exp_next_
    buf.insert(1, data);
    auto result = buf.flush();
    check("old segment ignored", result.empty());
    check("ack_num unchanged", buf.get_ack_num() == 3);
}

static void test_sack_blocks() {
    RecvBuffer buf;
    std::vector<uint8_t> data(10, 0x00);

    // insert out-of-order segments
    buf.insert(11, data);   // gap at 1-10
    buf.insert(21, data);
    buf.insert(31, data);
    buf.insert(41, data);   // 4th block — should be ignored (max 3)

    auto sack = buf.get_sack_blocks();
    check("max 3 sack blocks", sack.size() == 3);
    check("sack block 1 start", sack[0].first == 11);
    check("sack block 1 end",   sack[0].second == 21);
    check("sack block 2 start", sack[1].first == 21);
    check("sack block 2 end",   sack[1].second == 31);
    check("sack block 3 start", sack[2].first == 31);
    check("sack block 3 end",   sack[2].second == 41);
}

static void test_empty_flush() {
    RecvBuffer buf;
    auto result = buf.flush();
    check("empty flush returns empty vector", result.empty());
}

static void test_multiple_flushes() {
    RecvBuffer buf;
    std::vector<uint8_t> d1 = {0x01, 0x02};
    std::vector<uint8_t> d2 = {0x03, 0x04};
    std::vector<uint8_t> d3 = {0x05, 0x06};

    buf.insert(1, d1);
    auto r1 = buf.flush();
    check("first flush size", r1.size() == 2);
    check("ack_num after first flush", buf.get_ack_num() == 3);

    buf.insert(3, d2);
    auto r2 = buf.flush();
    check("second flush size", r2.size() == 2);
    check("ack_num after second flush", buf.get_ack_num() == 5);

    buf.insert(5, d3);
    auto r3 = buf.flush();
    check("third flush size", r3.size() == 2);
    check("ack_num after third flush", buf.get_ack_num() == 7);
}

static void test_single_byte_segments() {
    RecvBuffer buf;

    buf.insert(1, {0xAA});
    buf.insert(2, {0xBB});
    buf.insert(3, {0xCC});

    auto data = buf.flush();
    check("three 1-byte segments size", data.size() == 3);
    check("1-byte segment content", data == std::vector<uint8_t>({0xAA, 0xBB, 0xCC}));
    check("ack_num after 1-byte segments", buf.get_ack_num() == 4);
}

static void test_duplicate_seq_different_data() {
    RecvBuffer buf;

    buf.insert(1, {0x01, 0x02});
    buf.insert(1, {0xFF, 0xFF}); // same seq, different data — should be ignored

    auto data = buf.flush();
    check("second insert with same seq ignored", data == std::vector<uint8_t>({0x01, 0x02}));
}

static void test_sack_one_block() {
    RecvBuffer buf;
    std::vector<uint8_t> data(5, 0x00);

    buf.insert(6, data); // gap at 1-5

    auto sack = buf.get_sack_blocks();
    check("one out-of-order = one sack block", sack.size() == 1);
    check("sack block start", sack[0].first == 6);
    check("sack block end",   sack[0].second == 11);
}

static void test_sack_two_blocks() {
    RecvBuffer buf;
    std::vector<uint8_t> data(5, 0x00);

    buf.insert(6,  data); // gap at 1-5
    buf.insert(16, data); // gap at 11-15

    auto sack = buf.get_sack_blocks();
    check("two sack blocks", sack.size() == 2);
    check("block 1 start", sack[0].first == 6);
    check("block 1 end",   sack[0].second == 11);
    check("block 2 start", sack[1].first == 16);
    check("block 2 end",   sack[1].second == 21);
}

static void test_flush_stops_at_gap() {
    RecvBuffer buf;

    buf.insert(1, {0x01, 0x02}); // seq 1, size 2 → covers 1-2
    buf.insert(4, {0x03, 0x04}); // seq 4 — gap at 3

    auto data = buf.flush();
    check("flush stops before gap", data.size() == 2);
    check("ack_num stops at gap", buf.get_ack_num() == 3);
    check("sack block for segment after gap", !buf.get_sack_blocks().empty());
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== RecvBuffer unit tests ===\n\n";

    test_initial_state();
    test_in_order_delivery();
    test_out_of_order_delivery();
    test_duplicate_ignored();
    test_old_segment_ignored();
    test_sack_blocks();
    test_empty_flush();
    test_multiple_flushes();
    test_single_byte_segments();
    test_duplicate_seq_different_data();
    test_sack_one_block();
    test_sack_two_blocks();
    test_flush_stops_at_gap();

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return (failed == 0) ? 0 : 1;
}
