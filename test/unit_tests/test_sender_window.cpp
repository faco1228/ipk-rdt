/**
 * @file test_sender_window.cpp
 * @author Samuel Facka (xfackas00)
 * @brief Unit tests for SenderWindow sliding window logic.
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * Build: g++ -std=c++20 -Iinc src/sender_window.cpp test/unit_tests/test_sender_window.cpp -o test_sender_window
 * Run:   ./test_sender_window
 */

#include "sender_window.hpp"
#include <iostream>
#include <thread>
#include <chrono>

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

static void test_initial_capacity() {
    SenderWindow win(1);
    check("initial has_capacity", win.has_capacity());
    check("initial all_acked",    win.all_acked());
}

static void test_push_and_capacity() {
    SenderWindow win(1);
    std::vector<uint8_t> data(100, 0xAB);

    for (int i = 0; i < 32; ++i) {
        check("has_capacity before full", win.has_capacity());
        win.push(i * 100, data);
    }
    check("no capacity when full", !win.has_capacity());
    check("not all acked when full", !win.all_acked());
}

static void test_cumulative_ack() {
    SenderWindow win(1);
    std::vector<uint8_t> data(100, 0x00);

    win.push(0,   data);   // seq 0,   covers 0–99
    win.push(100, data);   // seq 100, covers 100–199
    win.push(200, data);   // seq 200, covers 200–299

    // ACK 200 — confirms first two segments
    win.ack_received(200, {});
    check("cumulative ACK removes confirmed segments", win.has_capacity());
    check("one segment remains", !win.all_acked());

    // ACK 300 — confirms last segment
    win.ack_received(300, {});
    check("all acked after full ACK", win.all_acked());
}

static void test_sack_marking() {
    SenderWindow win(1);
    std::vector<uint8_t> data(100, 0x00);

    win.push(0,   data);
    win.push(100, data);
    win.push(200, data);

    // SACK block [200, 300) — marks third segment
    std::vector<std::pair<uint32_t, uint32_t>> sack = {{200, 300}};
    win.ack_received(0, sack);

    // get_timed_out should not include SACK'd segment immediately
    // (timers haven't expired yet)
    auto timed_out = win.get_timed_out();
    bool sacked_not_in_timeout = true;
    for (const auto& seg : timed_out) {
        if (seg.seq_num == 200) sacked_not_in_timeout = false;
    }
    check("SACK'd segment not retransmitted", sacked_not_in_timeout);
}

static void test_rto_expiry() {
    SenderWindow win(1);   // timeout=1s, initial RTO=250ms
    std::vector<uint8_t> data(10, 0xFF);

    win.push(0, data);

    // wait for RTO to expire (250ms + margin)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto timed_out = win.get_timed_out();
    check("segment timed out after RTO", !timed_out.empty());
    check("timed out segment has correct seq", timed_out[0].seq_num == 0);
}

static void test_mark_retransmitted() {
    SenderWindow win(1);
    std::vector<uint8_t> data(10, 0x00);
    win.push(0, data);

    win.mark_retransmitted(0);

    // after mark_retransmitted, timer is reset so segment should not time out immediately
    auto timed_out = win.get_timed_out();
    check("segment not timed out after retransmit reset", timed_out.empty());
}

static void test_next_timeout_positive() {
    SenderWindow win(1);
    std::vector<uint8_t> data(10, 0x00);

    win.push(0, data);

    int t = win.next_timeout();
    check("next_timeout positive after push", t > 0);
    check("next_timeout <= initial RTO (250ms)", t <= 250);
}

static void test_exponential_backoff() {
    SenderWindow win(4); // timeout=4s, initial RTO=1000ms, max=4000ms
    std::vector<uint8_t> data(10, 0x00);

    win.push(0, data);

    // wait for RTO to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto timed_out = win.get_timed_out();
    check("segment timed out before retransmit", !timed_out.empty());

    win.mark_retransmitted(0);

    // RTO was 1000ms, after doubling it's 2000ms
    // next_timeout should now be ~2000ms, not ~250ms
    int t = win.next_timeout();
    check("next_timeout doubled after retransmit", t > 1000);
}

static void test_multiple_sack_blocks() {
    SenderWindow win(1);
    std::vector<uint8_t> data(100, 0x00);

    win.push(0,   data); // seq 0,   covers 0-99
    win.push(100, data); // seq 100, covers 100-199
    win.push(200, data); // seq 200, covers 200-299

    // SACK marks both seg 100 and seg 200
    std::vector<std::pair<uint32_t, uint32_t>> sack = {{100, 200}, {200, 300}};
    win.ack_received(0, sack);

    // neither sack'd segment should appear in timed_out
    auto timed_out = win.get_timed_out();
    bool none_sacked = true;
    for (const auto& seg : timed_out) {
        if (seg.seq_num == 100 || seg.seq_num == 200) {
            none_sacked = false;
        }
    }
    check("multiple SACK'd segments not retransmitted", none_sacked);
}

static void test_window_slides_after_partial_ack() {
    SenderWindow win(1);
    std::vector<uint8_t> data(100, 0x00);

    // fill window completely
    for (int i = 0; i < 32; ++i) {
        win.push(i * 100, data);
    }
    check("window full", !win.has_capacity());

    // ACK first 10 segments
    win.ack_received(1000, {});
    check("capacity restored after partial ACK", win.has_capacity());
    check("not all acked yet", !win.all_acked());

    // ACK remaining 22 segments
    win.ack_received(3200, {});
    check("all acked after full ACK", win.all_acked());
}

static void test_ack_exact_boundary() {
    SenderWindow win(1);
    std::vector<uint8_t> data(100, 0x00);

    win.push(0, data);

    // ACK exactly covers the segment (0 + 100 = 100)
    win.ack_received(100, {});
    check("segment removed on exact boundary ACK", win.all_acked());
}

static void test_all_acked_partial_then_full() {
    SenderWindow win(1);
    std::vector<uint8_t> data(50, 0x00);

    win.push(0,   data);
    win.push(50,  data);
    win.push(100, data);

    win.ack_received(50, {});
    check("not all acked after partial", !win.all_acked());

    win.ack_received(100, {});
    check("not all acked after second partial", !win.all_acked());

    win.ack_received(150, {});
    check("all acked after full", win.all_acked());
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== SenderWindow unit tests ===\n\n";

    test_initial_capacity();
    test_push_and_capacity();
    test_cumulative_ack();
    test_sack_marking();
    test_rto_expiry();
    test_mark_retransmitted();
    test_next_timeout_positive();
    test_exponential_backoff();
    test_multiple_sack_blocks();
    test_window_slides_after_partial_ack();
    test_ack_exact_boundary();
    test_all_acked_partial_then_full();

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return (failed == 0) ? 0 : 1;
}
