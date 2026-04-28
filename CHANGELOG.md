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

## Known limitations

- none known at this stage of development
