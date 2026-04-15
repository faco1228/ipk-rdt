# IPK Project 2 — Protocol & Architecture Design

## 1. Kľúčové rozhodnutia

| Oblasť | Rozhodnutie | Dôvod |
|---|---|---|
| ACK stratégia | Cumulative ACK + SACK v payload ACK paketov | Základ + efektivita bez zložitej fixnej hlavičky |
| Retransmisia | Selective Repeat | Retransmituje sa len stratený segment, nie celé okno |
| Window | Fixed 32 segmentov | Congestion control nie je required; jednoduché, rýchle |
| Retransmission timer | Exponential backoff (RTO × 2 po každom timeout) | Inšpirované RFC 6298, vyzerá seriózne |
| Checksum | CRC32 cez celý paket (header + payload) | Rýchle, ľahko implementovateľné |
| Connection ID | 2 B náhodné ID generované klientom | Ochrana pred zámenou paketov z rôznych relácií |
| Segment size | max 1184 B payload | 1200 − 16 B (hlavička) = 1184 B, pod MTU-safe limit |

---

## 2. Formát paketu (PDU)

Každý paket = hlavička (16 B) + payload (0–1184 B).
Celkovo max 1200 B UDP payload — splňuje požiadavku zadania.

```
Byte offset:
 0        1        2        3
 ┌────────┬────────┬─────────────────┐
 │  type  │ flags  │    conn_id      │
 ├────────┴────────┴─────────────────┤
 │              seq_num              │  (4 B, big-endian)
 ├───────────────────────────────────┤
 │              ack_num              │  (4 B, big-endian)
 ├───────────────────────────────────┤
 │              checksum             │  (4 B, CRC32)
 ├───────────────────────────────────┤
 │         payload (0–1184 B)        │
 └───────────────────────────────────┘

Celková hlavička: 16 B
```

### Polia hlavičky

| Pole | Veľkosť | Popis |
|---|---|---|
| `type` | 1 B | Typ paketu (enum, viď nižšie) |
| `flags` | 1 B | Bit 0 = SACK prítomný v payload; bity 1–7 = rezerva |
| `conn_id` | 2 B | Náhodné ID relácie (generuje klient pri SYN) |
| `seq_num` | 4 B | Byte offset prvého bajtu payloadu (ako v TCP) |
| `ack_num` | 4 B | Next expected byte od príjemcu (cumulative ACK) |
| `checksum` | 4 B | CRC32 cez celý paket, pole nastavené na 0 pri výpočte |
| payload | 0–1184 B | Dáta alebo SACK bloky (len v ACK paketoch) |

### Typy paketov (`type`)

| Hodnota | Názov | Smer | Popis |
|---|---|---|---|
| `0x01` | `SYN` | C → S | Začiatok relácie |
| `0x02` | `SYN_ACK` | S → C | Potvrdenie relácie |
| `0x03` | `DATA` | C → S | Dátový segment |
| `0x04` | `ACK` | S → C | Potvrdenie dát (môže obsahovať SACK) |
| `0x05` | `FIN` | C → S | Koniec dát |
| `0x06` | `FIN_ACK` | S → C | Potvrdenie FIN |
| `0x07` | `RST` | oba smery | Chyba, prerušenie relácie |

### SACK bloky (v payload ACK paketu, ak flags bit 0 = 1)

Každý SACK blok = 8 B (start + end, oba uint32_t big-endian).
Max 3 bloky = 24 B v payloade ACK paketu.

```
┌─────────────────┬─────────────────┐
│   sack_start    │    sack_end     │  (8 B na blok)
└─────────────────┴─────────────────┘
```

---

## 3. Životný cyklus relácie

### Handshake (2-way)
```
Klient                          Server
  │                               │
  │──── SYN (conn_id, seq=0) ────►│
  │                               │
  │◄─── SYN_ACK (ack=1) ─────────│
  │                               │
  │         [prenos dát]          │
```

> SYN_ACK potvrdzuje reláciu; klient môže ihneď začať posielať DATA.
> Jednoduchší 2-way handshake je postačujúci — bidirectional transfer nie je požadovaný.

### Prenos dát (Selective Repeat, window = 32)
```
Klient                          Server
  │                               │
  │──── DATA (seq=0,  1184 B) ───►│
  │──── DATA (seq=1184) ─────────►│
  │──── DATA (seq=2368) ─────────►│  ← window = 32 segmentov
  │         ...                   │
  │◄─── ACK (ack=1184, SACK) ────│  ← cumulative + SACK
  │                               │
  │  [retransmisia stratených]    │
  │──── DATA (seq=...) ──────────►│
```

### Teardown (3-way)
```
Klient                          Server
  │                               │
  │──── FIN (seq=N) ─────────────►│  ← všetky dáta odoslané
  │                               │
  │◄─── FIN_ACK (ack=N+1) ───────│  ← server dostal všetko
  │                               │
  [klient ukončí, server zapíše a ukončí]
```

---

## 4. Timeout a retransmisia

- Každý odoslaný segment má vlastný timer.
- Počiatočný RTO = `timeout` parameter / 4 (napr. pri `-w 1` → RTO = 250 ms).
- Po každom timeoutu: RTO × 2 (exponential backoff), max = `timeout` hodnota.
- Ak uplynie celý `-w TIMEOUT` bez protocol progress → exit s non-zero kódom.
- Protocol progress = nový ACK pokrývajúci predtým nepotvrdené dáta.

---

## 5. OOP návrh tried

```
ArgParser       — parsuje CLI argumenty → Config
Config          — struct s parsed hodnotami (mode, port, addr, ...)
Packet          — encode/decode jedného PDU, výpočet a validácia CRC32
UdpSocket       — RAII wrapper: socket(), bind(), sendto(), recvfrom(); IPv4 + IPv6
SenderWindow    — sliding window: sleduje unACKed segmenty, per-segment timery
RecvBuffer      — reorder buffer: prijíma out-of-order, vydáva in-order dáta
Sender          — klientský mód: číta vstup, orchestruje SenderWindow + UdpSocket
Receiver        — serverský mód: orchestruje RecvBuffer + UdpSocket, zapisuje výstup
```

Každá trieda = jeden súbor v `inc/` + `src/`. Žiadne God-objekty.

---

## 6. Štruktúra súborov

```
riesenie/
├── src/
│   ├── main.cpp          entry point, signal handling
│   ├── arg.cpp           argument parsing
│   ├── packet.cpp        PDU encode/decode, CRC32
│   ├── socket.cpp        UDP socket RAII wrapper
│   ├── sender_window.cpp sliding window + timers (klient)
│   ├── recv_buffer.cpp   reorder buffer (server)
│   ├── sender.cpp        klientský mód
│   └── receiver.cpp      serverský mód
├── inc/
│   ├── arg.hpp
│   ├── config.hpp
│   ├── packet.hpp
│   ├── socket.hpp
│   ├── sender_window.hpp
│   ├── recv_buffer.hpp
│   ├── sender.hpp
│   └── receiver.hpp
├── test/
│   └── test.sh
├── Makefile
├── README.md
├── CHANGELOG.md
├── LICENSE
└── DESIGN.md             (tento súbor)
```

---

## 7. Čo NIE JE v scope

Podľa zadania explicitne vynechávame:
- Congestion control
- Šifrovanie / autentifikácia
- Viacero klientov naraz
- Bidirectional transfer
- Kompatibilita s TCP/QUIC/SCTP
