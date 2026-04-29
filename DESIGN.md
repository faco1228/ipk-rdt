# IPK Project 2 — Protocol & Architecture Design

## 1. Kľúčové rozhodnutia

| Oblasť | Rozhodnutie | Dôvod |
|---|---|---|
| ACK stratégia | Cumulative ACK + SACK v payload ACK paketov | Základ + efektivita bez zložitej fixnej hlavičky |
| Retransmisia | Selective Repeat | Retransmituje sa len stratený segment, nie celé okno |
| Window | Fixed 32 segmentov | Congestion control nie je required; jednoduché, rýchle |
| Retransmission timer | Exponential backoff (RTO × 2 po každom timeout) | Inšpirované RFC 6298 |
| Checksum | CRC32 cez celý paket (header + payload) | Rýchle, ľahko implementovateľné, silnejšie ako internet checksum |
| Connection ID | 2 B náhodné ID generované klientom | Ochrana pred zámenou paketov z rôznych relácií |
| Handshake | 3-way (SYN → SYN_ACK → ACK) | Ochrana pred zatúlanými SYN retransmitmi (viď prednáška L03) |
| Segment size | max 1180 B payload | 1200 − 20 B (hlavička) = 1180 B, pod MTU-safe limit |
| Seq/ACK priestor | SYN a FIN konzumujú 1 byte | Jednoznačné ACKovanie kontrolných paketov, ako v TCP |

---

## 2. Formát paketu (PDU)

Každý paket = hlavička (20 B) + payload (0–1180 B).
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
 ├─────────────────┬─────────────────┤
 │   payload_len   │   (reserved)    │  (2 B + 2 B)
 ├─────────────────┴─────────────────┤
 │              checksum             │  (4 B, CRC32)
 ├───────────────────────────────────┤
 │         payload (0–1180 B)        │
 └───────────────────────────────────┘

Celková hlavička: 20 B
```

### Polia hlavičky

| Pole | Veľkosť | Popis |
|---|---|---|
| `type` | 1 B | Typ paketu (enum, viď nižšie) |
| `flags` | 1 B | Bit 0 = SACK prítomný v payload; bity 1–7 = rezerva |
| `conn_id` | 2 B | Náhodné ID relácie (generuje klient pri SYN) |
| `seq_num` | 4 B | Byte offset prvého bajtu payloadu; SYN a FIN konzumujú 1 virtuálny byte |
| `ack_num` | 4 B | Next expected byte od príjemcu (cumulative ACK) |
| `payload_len` | 2 B | Dĺžka payloadu v bajtoch (0–1180); umožňuje detekciu truncated paketov |
| `reserved` | 2 B | Rezervované, musí byť 0 |
| `checksum` | 4 B | CRC32 cez celý paket, pole nastavené na 0 pri výpočte |
| payload | 0–1180 B | Dáta alebo SACK bloky (len v ACK paketoch) |

### Validácia paketu (prijímacia strana)

Prijatý paket sa zahodí ak:
1. Dĺžka UDP datagramu < 20 B (neúplná hlavička)
2. `payload_len` + 20 > dĺžka UDP datagramu (truncated payload)
3. CRC32 nesedí (corrupted)
4. `conn_id` nesedí s aktívnou reláciou (okrem SYN na serveri)
5. Neznámy `type`

### Typy paketov (`type`)

| Hodnota | Názov | Smer | Popis |
|---|---|---|---|
| `0x01` | `SYN` | C → S | Začiatok relácie (konzumuje 1 byte v seq priestore) |
| `0x02` | `SYN_ACK` | S → C | Potvrdenie relácie |
| `0x03` | `DATA` | C → S | Dátový segment |
| `0x04` | `ACK` | S → C | Potvrdenie dát (môže obsahovať SACK) |
| `0x05` | `FIN` | C → S | Koniec dát (konzumuje 1 byte v seq priestore) |
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

`sack_start` = prvý byte prijatého bloku, `sack_end` = prvý byte za blokom (exclusive).

### CRC32 výpočet

1. Postav celý paket (hlavička + payload)
2. Nastav pole `checksum` na `0x00000000`
3. Vypočítaj CRC32 cez celý buffer
4. Zapíš výsledok do `checksum` (big-endian)

Prijímacia strana: uloží `checksum`, vynuluje ho, prepočíta, porovná.

---

## 3. Životný cyklus relácie

### Handshake (3-way)

```
Klient                          Server
  │                               │  Server: LISTEN
  │──── SYN (conn_id, seq=0) ────►│  Server: SYN_RCVD
  │                               │
  │◄─── SYN_ACK (ack=1) ──────────│
  │                               │
  │──── ACK (ack=1) ─────────────►│  Server: ESTABLISHED
  │                               │  Klient: ESTABLISHED
  │         [prenos dát]          │
```

**Prečo 3-way a nie 2-way:**
Prednáška L03 (Transportní vrstva) ukazuje, že 2-way handshake je problematický — zatúlaný retransmitnutý SYN môže otvoriť falošnú reláciu, a zatúlané dáta môžu byť vložené do novej. Tretí paket (ACK) potvrdzuje, že klient je skutočne živý.

**SYN konzumuje 1 byte v sekvenčnom priestore:**
- Klient posiela `SYN` s `seq=0`
- Server odpovedá `SYN_ACK` s `ack=1` (potvrdzuje SYN)
- Klient posiela `ACK` s `ack=1`
- Prvý DATA segment bude mať `seq=1`

**Server pred handshake:**
Server ignoruje všetky pakety okrem SYN. Duplicitné, malformované, alebo nesúvisiace pakety sa ticho zahadzujú (podľa zadania).

### Prenos dát (Selective Repeat, window = 32)

```
Klient                          Server
  │                               │
  │──── DATA (seq=1,   1180 B) ──►│
  │──── DATA (seq=1181) ─────────►│
  │──── DATA (seq=2361) ─────────►│  ← window = 32 segmentov in-flight
  │         ...                   │
  │◄─── ACK (ack=1181, SACK) ─────│  ← cumulative + SACK bloky
  │                               │
  │  [retransmisia stratených]    │
  │──── DATA (seq=...) ──────────►│
```

**Sekvenčné čísla sú byte offsets** (nie segment čísla). `seq_num` v DATA pakete = offset prvého bajtu payloadu v celkovom streame. Prvý DATA segment má `seq=1` (SYN konzumoval byte 0).

**Cumulative ACK:** `ack_num` = next expected byte. Ak server dostal byty 1–1180 a 2361–3540 (chýba 1181–2360), pošle `ACK` s `ack=1181` a SACK blok `[2361, 3541)`.

**Window:** Klient drží max 32 nepotvrdených segmentov in-flight. Keď príde ACK, okno sa posunie. Window sa nemení dynamicky (žiadny congestion control).

### Handling prázdneho vstupu

Ak je vstup prázdny (0 bytov), klient po handshake okamžite posiela FIN:

```
Klient                          Server
  │──── SYN (seq=0) ────────────►│
  │◄─── SYN_ACK (ack=1) ─────────│
  │──── ACK (ack=1) ────────────►│
  │──── FIN (seq=1) ────────────►│  ← žiadne DATA, hneď FIN
  │◄─── FIN_ACK (ack=2) ─────────│
  │  [obe strany ukončia]        │
```

Server zapíše 0 bytov na výstup a exitne s kódom 0.

### Teardown

```
Klient                          Server
  │                               │
  │──── FIN (seq=N) ─────────────►│  ← všetky dáta odoslané a ACKnuté
  │                               │
  │◄─── FIN_ACK (ack=N+1) ────────│  ← server dostal FIN
  │                               │
  [klient ukončí]                 [server zapíše, ukončí]
```

**FIN konzumuje 1 byte v sekvenčnom priestore:**
- `FIN` s `seq=N` (kde N = 1 + celková dĺžka dát), potvrdenie cez `FIN_ACK` s `ack=N+1`

**Klient posiela FIN až keď:** všetky dáta boli odoslané aj potvrdené (cumulative ACK pokrýva všetko).

**RST paket:** Ktorákoľvek strana môže poslať RST pri fatálnej chybe. Prijatie RST = okamžité ukončenie bez ďalšej komunikácie (exit non-zero).

---

## 4. Konečný stavový automat (FSM)

### 4.0 FSM diagramy

**Klient (Sender):**

```
┌─────────┐
│  CLOSED │──── spustenie / send SYN ───────────────────────────────►┐
└─────────┘                                                          │
                                                                     ▼
                ┌── RTO timeout: retransmit SYN, RTO×2      ┌─────────────────┐
                └──────────────────────────────────────────►│    SYN_SENT     │
                                                            └────────┬────────┘
                                                                     │ recv SYN_ACK / send ACK
                                                                     ▼
                ┌── recv ACK: posun okna                    ┌─────────────────┐
                ├── recv SACK: označ segmenty            ┌──┤   ESTABLISHED   │
                ├── RTO timeout: retransmit seg., RTO×2  └─►└────────┬────────┘
                └──────────────────────────────────────────────────  │
                                                     EOF + all_acked │ / send FIN
                                                                     ▼
                ┌── RTO timeout: retransmit FIN, RTO×2      ┌─────────────────┐
                └──────────────────────────────────────────►│    FIN_SENT     │
                                                            └────────┬────────┘
                                                                     │ recv FIN_ACK
                                                                     ▼
                                                            ┌─────────────────┐
                                                            │      DONE       │
                                                            └─────────────────┘
         global timeout / RST / SIGINT → ERROR  (z ľubovoľného stavu)
```

**Server (Receiver):**

```
┌─────────┐
│  LISTEN │──── recv SYN / send SYN_ACK ───────────────────────────►┐
└─────────┘                                                         │
                                                                    ▼
           ┌── RTO timeout: retransmit SYN_ACK, RTO×2       ┌───────────────┐
           ├── recv dup. SYN: retransmit SYN_ACK         ┌──┤   SYN_RCVD    │
           └──────────────────────────────────────────►  └─►└───────┬───────┘
                                                                     │ recv ACK / recv DATA
                                                                     ▼
           ┌── recv DATA: RecvBuffer, flush, ACK+SACK     ┌─────────────────┐
           ├── recv dup. DATA: send ACK               ┌───┤   ESTABLISHED   │
           └────────────────────────────────────────► └──►└─────────┬───────┘
                                                                    │ recv FIN (all data rcvd)
                                                                    │ flush / send FIN_ACK
                                                                    ▼
                                                           ┌─────────────────┐
                                                           │      DONE       │
                                                           └─────────────────┘
         global timeout / RST / SIGINT → ERROR  (z ľubovoľného stavu)
```

---

### 4.1 Klient (Sender)

| Stav | Udalosť | Akcia | Nový stav |
|---|---|---|---|
| `CLOSED` | program spustený s `-c` | vygeneruj `conn_id`, pošli `SYN(seq=0)`, spusti RTO timer | `SYN_SENT` |
| `SYN_SENT` | recv `SYN_ACK(ack=1)` s platným `conn_id` | pošli `ACK(ack=1)`, reset global timer | `ESTABLISHED` |
| `SYN_SENT` | RTO timeout | retransmitni `SYN`, RTO × 2 | `SYN_SENT` |
| `SYN_SENT` | global timeout (`-w`) | — | `ERROR` |
| `SYN_SENT` | recv RST | — | `ERROR` |
| `SYN_SENT` | recv čokoľvek iné | ignoruj | `SYN_SENT` |
| `ESTABLISHED` | dáta dostupné na vstupe, okno nie je plné | čítaj blok, pošli `DATA(seq=offset)`, spusti RTO timer pre segment | `ESTABLISHED` |
| `ESTABLISHED` | recv `ACK` s novým `ack_num` (> predchádzajúci) | posuň okno, zruš RTO pre potvrdené segmenty, reset global timer | `ESTABLISHED` |
| `ESTABLISHED` | recv `ACK` s SACK blokmi | označ SACK segmenty ako doručené, neplánuj ich retransmisiu | `ESTABLISHED` |
| `ESTABLISHED` | recv duplicitný `ACK` (rovnaký `ack_num`) | ignoruj | `ESTABLISHED` |
| `ESTABLISHED` | RTO timeout pre segment | retransmitni daný segment, RTO × 2 pre ten segment | `ESTABLISHED` |
| `ESTABLISHED` | global timeout (`-w`) | — | `ERROR` |
| `ESTABLISHED` | vstup EOF + všetky dáta ACKnuté | pošli `FIN(seq=N)`, spusti RTO timer | `FIN_SENT` |
| `ESTABLISHED` | recv RST | — | `ERROR` |
| `ESTABLISHED` | SIGINT / SIGTERM | pošli RST, cleanup | `ERROR` |
| `FIN_SENT` | recv `FIN_ACK(ack=N+1)` | — | `DONE` |
| `FIN_SENT` | RTO timeout | retransmitni `FIN`, RTO × 2 | `FIN_SENT` |
| `FIN_SENT` | global timeout (`-w`) | — | `ERROR` |
| `FIN_SENT` | recv RST | — | `ERROR` |
| `DONE` | — | cleanup, exit 0 | — |
| `ERROR` | — | cleanup, exit non-zero | — |

### 4.2 Server (Receiver)

| Stav | Udalosť | Akcia | Nový stav |
|---|---|---|---|
| `LISTEN` | recv `SYN` s platným formátom | uloži `conn_id`, pošli `SYN_ACK(ack=1)`, spusti RTO timer | `SYN_RCVD` |
| `LISTEN` | recv čokoľvek iné | ignoruj | `LISTEN` |
| `LISTEN` | global timeout (`-w`) | — | `ERROR` |
| `LISTEN` | SIGINT / SIGTERM | cleanup | `ERROR` |
| `SYN_RCVD` | recv `ACK(ack=1)` s platným `conn_id` | reset global timer | `ESTABLISHED` |
| `SYN_RCVD` | recv `DATA` s platným `conn_id` | prijmi ako implicitný ACK handshake + spracuj DATA | `ESTABLISHED` |
| `SYN_RCVD` | recv duplicitný `SYN` (rovnaký `conn_id`) | retransmitni `SYN_ACK` | `SYN_RCVD` |
| `SYN_RCVD` | RTO timeout | retransmitni `SYN_ACK`, RTO × 2 | `SYN_RCVD` |
| `SYN_RCVD` | global timeout (`-w`) | — | `ERROR` |
| `ESTABLISHED` | recv nový `DATA(seq=X)` (nie duplicitný) | ulož do recv bufferu, doruč in-order dáta na výstup, pošli `ACK` s cum. ack + SACK, reset global timer | `ESTABLISHED` |
| `ESTABLISHED` | recv duplicitný `DATA` (seq už prijatý) | pošli `ACK` s aktuálnym cum. ack (žiadny progress) | `ESTABLISHED` |
| `ESTABLISHED` | recv `FIN(seq=N)`, všetky dáta do N sú doručené | flush výstup, pošli `FIN_ACK(ack=N+1)` | `DONE` |
| `ESTABLISHED` | recv `FIN(seq=N)`, ešte chýbajú dáta pred N | bufferuj FIN, pokračuj v prijímaní DATA | `ESTABLISHED` |
| `ESTABLISHED` | global timeout (`-w`) | — | `ERROR` |
| `ESTABLISHED` | recv RST | — | `ERROR` |
| `ESTABLISHED` | SIGINT / SIGTERM | pošli RST, cleanup | `ERROR` |
| `DONE` | — | cleanup, exit 0 | — |
| `ERROR` | — | cleanup, exit non-zero | — |

### 4.3 Dôležité edge cases pokryté FSM

**Zatúlaný SYN:** Server v `ESTABLISHED` prijme SYN s iným `conn_id` → ignoruje. SYN s rovnakým `conn_id` → ignoruje (relácia už beží).

**FIN pred poslednými dátami:** Ak server prijme FIN, ale ešte nedostal všetky DATA segmenty pred ním (out-of-order), bufferuje FIN a čaká na chýbajúce dáta. Až keď cum. ack dosiahne FIN seq → spracuje FIN.

**Duplicitné pakety:** Duplicitné DATA sa rozpoznajú podľa seq_num (už v recv bufferi). Duplicitné ACK na klientovi neposúvajú okno. Ani jedno nepredstavuje progress.

**Strata tretieho paketu handshake (ACK):** Ak sa ACK stratí, ale klient už posiela DATA, server v `SYN_RCVD` akceptuje DATA s platným `conn_id` ako implicitný ACK handshake a prejde do `ESTABLISHED`. Toto zabraňuje deadlocku.

---

## 5. Timeout a retransmisia

### Dva nezávislé timery

**1. Globálny progress timer (`-w TIMEOUT`):**
- Beží nepretržite počas celej relácie
- Resetuje sa pri každom protocol progress
- Ak vyprší → exit non-zero (peer je nedostupný)

**2. Per-segment retransmission timer (RTO):**
- Každý nepotvrdený segment (vrátane SYN a FIN) má vlastný timer
- Počiatočný RTO = `TIMEOUT / 4` (napr. `-w 1` → RTO = 250 ms)
- Po každom timeoutu: `RTO = min(RTO × 2, TIMEOUT)` (exponential backoff)

### Definícia protocol progress

Podľa zadania, progress = prijatie platného paketu, ktorý posunie stav relácie:
- Handshake: prijatie SYN_ACK (klient) alebo ACK/DATA (server)
- Dátový prenos: nový ACK pokrývajúci predtým nepotvrdené dáta (klient), alebo nový DATA segment (server)
- Teardown: prijatie FIN_ACK (klient) alebo FIN (server)

Duplicity a retransmisie sa nepočítajú ako progress.

---

## 6. OOP návrh tried

```
main.cpp         — entry point, sigaction(SIGINT/SIGTERM) handler
ArgParser        — parsuje CLI argumenty → Config
Config           — struct s parsed hodnotami (mode, port, addr, timeout, ...)

Packet           — encode/decode jedného PDU, výpočet a validácia CRC32
UdpSocket        — RAII wrapper: socket(), bind(), sendto(), recvfrom()
                   podporuje IPv4 + IPv6 cez getaddrinfo()

SenderWindow     — sliding window: sleduje unACKed segmenty, per-segment timery
                   spracováva cumulative ACK + SACK, rozhoduje o retransmisiách
RecvBuffer       — reorder buffer: prijíma out-of-order DATA, vydáva in-order
                   generuje cumulative ACK + SACK bloky

Sender           — klientský FSM: CLOSED → SYN_SENT → ESTABLISHED → FIN_SENT → DONE
                   číta vstup (súbor/stdin), orchestruje SenderWindow + UdpSocket
Receiver         — serverový FSM: LISTEN → SYN_RCVD → ESTABLISHED → DONE
                   orchestruje RecvBuffer + UdpSocket, zapisuje výstup (súbor/stdout)
```

Každá trieda = jeden súbor v `inc/` + `src/`.
Konvencie: `explicit` konštruktory, `trailing _` pre member premenné, `const&` parametre, Doxygen komentáre, `sigaction` pre signály.

---

## 7. Neblokujúce I/O

Klient aj server musia súčasne prijímať pakety, odosielať pakety a sledovať timery (RTO aj globálny timeout). Riešenie je `poll()` na UDP socket s timeoutom nastaveným na najbližší expirujúci timer. Ak `poll()` vráti udalosť, spracuje sa prijatý paket. Ak vyprší timeout, skontrolujú sa RTO timery a prípadne globálny timeout. Busy-waiting sa nepoužíva — zadanie explicitne trestá implementácie, ktoré naň spoliehajú.

---

## 8. Štruktúra súborov

```
riesenie/
├── src/
│   ├── main.cpp              # entry point, signal handling
│   ├── arg.cpp               # argument parsing
│   ├── packet.cpp            # packet encode/decode, CRC32
│   ├── socket.cpp            # UDP socket RAII wrapper (IPv4/IPv6)
│   ├── sender_window.cpp     # sliding window, per-segment timery
│   ├── recv_buffer.cpp       # reorder buffer, cumulative ACK + SACK
│   ├── sender.cpp            # client FSM, orchestruje SenderWindow + UdpSocket
│   └── receiver.cpp          # server FSM, orchestruje RecvBuffer + UdpSocket
├── inc/
│   ├── config.hpp            # Config struct + Mode enum (žiadny .cpp)
│   ├── arg.hpp               # ArgParser
│   ├── packet.hpp            # PacketType enum, Header struct, Packet class
│   ├── socket.hpp            # UdpSocket
│   ├── sender_window.hpp     # SenderWindow
│   ├── recv_buffer.hpp       # RecvBuffer
│   ├── sender.hpp            # Sender
│   └── receiver.hpp          # Receiver
├── test/
│   └── test.sh
├── Makefile
├── README.md
├── CHANGELOG.md
└── LICENSE
```

---

## 9. Čo NIE JE v scope

Podľa zadania explicitne vynechávame:
- Congestion control
- Šifrovanie / autentifikácia
- Viacero klientov naraz
- Bidirectional transfer
- Kompatibilita s TCP/QUIC/SCTP
- Transfer resume po crashe