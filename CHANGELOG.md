# Changelog

All notable work on the project is summarized here.

---

## 2026-04-15

### Protocol design and project setup

- reviewed assignment requirements and shared IPK guidelines
- designed custom reliable transport protocol over UDP
- decided on Selective Repeat with cumulative ACK + SACK
- decided on fixed window of 32 segments, exponential backoff RTO
- designed 20-byte packet header format (type, flags, conn_id, seq_num, ack_num, payload_len, reserved, checksum)
- designed 3-way handshake (SYN → SYN_ACK → ACK)
- designed session teardown (FIN → FIN_ACK)
- defined SYN and FIN consuming 1 byte in sequence space
- defined dual-timer model: per-segment RTO + global progress timer
- created full FSM tables for both client and server
- created DESIGN.md documenting all protocol decisions
- created initial project file structure (inc/, src/, test/)

---

## 2026-04-26

### Argument parsing and configuration

- implemented `Config` struct with `Mode` enum (CLIENT / SERVER)
- implemented `ArgParser` class with `parse()`, `print_help()`, `validate_config()`
- added support for all required CLI options: `-c`, `-s`, `-p`, `-a`, `-i`, `-o`, `-w`, `-h`
- added bounds checking for port (1–65535) and timeout (> 0)
- added missing-value detection for all options that require an argument
- added unknown-argument detection
- added stubs for `Packet`, `SenderWindow`, `RecvBuffer` classes
- finalized file structure in DESIGN.md

---

## 2026-04-27

### Packet encode and CRC32

- implemented `Packet` class constructor with initializer list
- implemented `encode()` — fills Header struct, serializes to raw bytes via memcpy, computes and writes CRC32
- implemented `compute_crc32()` — IEEE 802.3 CRC32 with lookup table initialized via static lambda
- defined `Header` struct (20 B, packed) with all wire-format fields
- defined `PacketType` enum and protocol constants (HEADER_SIZE, MAX_PAYLOAD, FLAG_SACK)

---

## 2026-04-27 (2)

### Packet decode

- implemented `decode()` — validates length, payload_len, CRC32, and packet type
- loads header via memcpy, zeroes checksum field before CRC verification
- converts all fields from big-endian via ntohs/ntohl
- throws std::runtime_error on any invalid or corrupted packet

---

## 2026-04-28

### UdpSocket implementation

- implemented `UdpSocket` class with RAII socket management
- implemented `open()` — creates UDP socket via socket(family, SOCK_DGRAM, 0)
- implemented `bind()` — server side: getaddrinfo + SO_REUSEADDR + bind()
- implemented `resolve()` — client side: getaddrinfo + stores dest_addr_ via memcpy
- implemented `send()` — sendto stored dest_addr_
- implemented `send_to()` — sendto explicit address (server → client)
- implemented `receive()` — recvfrom with buffer resize to actual received size
- added non-copyable semantics (deleted copy constructor and assignment operator)
- destructor closes socket fd if open

---

## 2026-04-28 (2)

### main.cpp

- implemented signal handling via sigaction (SIGINT, SIGTERM) with global g_interrupted flag
- implemented argument parsing and mode dispatch (CLIENT → Sender, SERVER → Receiver)
- added exception handling (invalid_argument, runtime_error, catch-all)
- Sender/Receiver calls stubbed out pending implementation

---

## 2026-04-29

### SenderWindow implementation

- implemented `SenderWindow` class with sliding window logic (max 32 segments)
- implemented `push()` — adds segment with send_time and initial RTO
- implemented `ack_received()` — cumulative ACK erase via remove_if + SACK marking
- implemented `get_timed_out()` — returns segments whose RTO has expired
- implemented `mark_retransmitted()` — doubles RTO (exponential backoff), resets send_time
- implemented `has_capacity()` — checks window size < 32
- implemented `all_acked()` — checks segments_ is empty
- implemented `next_timeout()` — returns ms until nearest expiring timer (for poll())

---

## 2026-04-29 (2)

### RecvBuffer implementation

- implemented `RecvBuffer` class with reorder buffer using std::map
- implemented `insert()` — ignores duplicates and segments before exp_next_
- implemented `flush()` — delivers in-order data, advances exp_next_
- implemented `get_ack_num()` — returns next expected byte
- implemented `get_sack_blocks()` — returns max 3 SACK blocks for out-of-order segments
- added unit tests for RecvBuffer, all passing

---

## 2026-04-29 (3)

### Sender implementation

- implemented `Sender` class: constructor, `run()`, `handshake()`, `send_data()`, `teardown()`
- constructor initialises members via initializer list; `next_seq_` starts at 1 (SYN consumed seq 0)
- `run()` resolves server address, generates random `connection_id` (1–65535 via mt19937), calls handshake → send_data → teardown
- `handshake()` — 3-way handshake with poll()-based SYN retransmit and exponential backoff RTO
- `send_data()` — sliding window transfer: reads input in 1180 B chunks, sends DATA packets, processes ACK+SACK, retransmits on RTO expiry, tracks global progress timer
- `teardown()` — sends FIN, waits for FIN_ACK with exponential backoff retransmit
- `elapsed_ms()` — static helper, returns ms since a given steady_clock time point
- `parse_sack()` — parses up to 3 SACK blocks from ACK packet payload (8 B each, network byte order)
- added `next_seq_` member to `Sender` for sharing state between `send_data()` and `teardown()`

---

## 2026-04-29 (4)

### Receiver implementation

- implemented `Receiver` class: constructor, `run()`, `handshake()`, `receive_data()`, `send_ack()`
- `run()` binds socket via `socket_.bind()`, then calls `handshake()` → `receive_data()`
- `handshake()` — waits for SYN, stores `connection_id_` and `client_addr_`, sends SYN_ACK, returns
- `receive_data()` — writes output to file or stdout via `std::ostream*`, processes DATA/FIN/SYN packets, handles out-of-order delivery via `RecvBuffer`, sends ACK+SACK after each DATA
- `send_ack()` — builds ACK packet with cumulative ack_num + SACK payload from `buffer_.get_sack_blocks()`; SACK blocks serialised as 8 B each (htonl start + htonl end)
- handles FIN received before all DATA: buffers `fin_seq`, checks after each DATA insert if gap is filled
- handles duplicate SYN in ESTABLISHED: retransmits SYN_ACK
- `elapsed_ms()` — static helper, identical to Sender
- added `receiver.hpp` and `receiver.cpp` to project

---

## Known limitations

- none known at this stage of development
