#!/bin/bash
# End-to-end tests for ipk-rdt (Reliable File Transfer over UDP)
# Samuel Facka (xfackas00)
#
# Run with: make test
#       or: chmod +x test/test.sh && ./test/test.sh

BINARY="./ipk-rdt"
PORT=19500
TIMEOUT=3
TMP_IN="/tmp/ipk_test_input.bin"
TMP_OUT="/tmp/ipk_test_output.bin"

PASS=0
FAIL=0

# ── helpers ───────────────────────────────────────────────────────────────────

check_exit() {
    local desc=$1 expected=$2 actual=$3
    if [ "$actual" -eq "$expected" ]; then
        echo "PASS: $desc"
        ((PASS++))
    else
        echo "FAIL: $desc (expected exit $expected, got $actual)"
        ((FAIL++))
    fi
}

check_contains() {
    local desc=$1 expected=$2 actual=$3
    if echo "$actual" | grep -qF "$expected"; then
        echo "PASS: $desc"
        ((PASS++))
    else
        echo "FAIL: $desc"
        echo "  expected: $expected"
        echo "  actual:   $actual"
        ((FAIL++))
    fi
}

check_files_equal() {
    local desc=$1 file1=$2 file2=$3
    if cmp -s "$file1" "$file2"; then
        echo "PASS: $desc"
        ((PASS++))
    else
        echo "FAIL: $desc (files differ)"
        ((FAIL++))
    fi
}

start_receiver() {
    local port=$1 outfile=$2
    "$BINARY" -s -p "$port" -o "$outfile" -w "$TIMEOUT" &
    RCV_PID=$!
}

stop_receiver() {
    local pid=$1
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
}

cleanup() {
    rm -f "$TMP_IN" "$TMP_OUT"
}

trap cleanup EXIT

# ── prerequisite ──────────────────────────────────────────────────────────────

if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary '$BINARY' not found. Run 'make' first."
    exit 1
fi

echo "================================="
echo " ipk-rdt — End-to-End Test Suite"
echo "================================="
echo ""

# ── [1] Argument parsing ──────────────────────────────────────────────────────

echo "--- [1] Argument parsing ---"

output=$("$BINARY" -h 2>&1); code=$?
check_contains "-h prints usage" "Usage" "$output"
check_exit "-h exits 0" 0 $code

output=$("$BINARY" 2>&1); code=$?
check_exit "no arguments exits 1" 1 $code

output=$("$BINARY" -c 2>&1); code=$?
check_exit "-c without -p exits 1" 1 $code

output=$("$BINARY" -s 2>&1); code=$?
check_exit "-s without -p exits 1" 1 $code

output=$("$BINARY" -c -p 9000 2>&1); code=$?
check_exit "-c without -a exits 1" 1 $code

output=$("$BINARY" -p 9000 2>&1); code=$?
check_exit "no mode flag exits 1" 1 $code

output=$("$BINARY" -c -s -p 9000 -a 127.0.0.1 2>&1); code=$?
check_exit "-c and -s together exits 1" 1 $code

output=$("$BINARY" -c -p 0 -a 127.0.0.1 2>&1); code=$?
check_exit "port 0 exits 1" 1 $code

output=$("$BINARY" -c -p 99999 -a 127.0.0.1 2>&1); code=$?
check_exit "port 99999 exits 1" 1 $code

output=$("$BINARY" -c -p abc -a 127.0.0.1 2>&1); code=$?
check_exit "non-numeric port exits 1" 1 $code

output=$("$BINARY" -c -p 9000 -a 127.0.0.1 -w 0 2>&1); code=$?
check_exit "timeout 0 exits 1" 1 $code

output=$("$BINARY" -c -p 9000 -a 127.0.0.1 -z 2>&1); code=$?
check_exit "unknown argument exits 1" 1 $code

echo ""

# ── [2] Basic transfer — small string ────────────────────────────────────────

echo "--- [2] Basic transfer (small string) ---"

start_receiver $PORT "$TMP_OUT"
sleep 0.2

echo -n "hello world" | "$BINARY" -c -p $PORT -a 127.0.0.1 -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0" 0 $SND_CODE
check_exit "receiver exits 0" 0 $RCV_CODE
result=$(cat "$TMP_OUT" 2>/dev/null)
check_contains "output contains transferred data" "hello world" "$result"
rm -f "$TMP_OUT"

echo ""

# ── [3] Empty input ───────────────────────────────────────────────────────────

echo "--- [3] Empty input ---"

start_receiver $((PORT+1)) "$TMP_OUT"
sleep 0.2

echo -n "" | "$BINARY" -c -p $((PORT+1)) -a 127.0.0.1 -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 on empty input" 0 $SND_CODE
check_exit "receiver exits 0 on empty input" 0 $RCV_CODE
size=$(wc -c < "$TMP_OUT" 2>/dev/null || echo "0")
if [ "$size" -eq 0 ]; then
    echo "PASS: output file is empty"
    ((PASS++))
else
    echo "FAIL: output file is not empty (size=$size)"
    ((FAIL++))
fi
rm -f "$TMP_OUT"

echo ""

# ── [4] File transfer (-i / -o) ───────────────────────────────────────────────

echo "--- [4] File transfer (-i / -o) ---"

echo "test file content 1234" > "$TMP_IN"

start_receiver $((PORT+2)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+2)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (file input)" 0 $SND_CODE
check_exit "receiver exits 0 (file output)" 0 $RCV_CODE
check_files_equal "input and output files are identical" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [5] Large file (1 MB) ─────────────────────────────────────────────────────

echo "--- [5] Large file transfer (1 MB) ---"

dd if=/dev/urandom of="$TMP_IN" bs=1024 count=1024 2>/dev/null

start_receiver $((PORT+3)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+3)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (1 MB)" 0 $SND_CODE
check_exit "receiver exits 0 (1 MB)" 0 $RCV_CODE
check_files_equal "1 MB file transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [6] Multiline text ────────────────────────────────────────────────────────

echo "--- [6] Multiline text ---"

printf "line1\nline2\nline3\n" > "$TMP_IN"

start_receiver $((PORT+4)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+4)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (multiline)" 0 $SND_CODE
check_exit "receiver exits 0 (multiline)" 0 $RCV_CODE
check_files_equal "multiline text transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [7] Timeout — no server running ──────────────────────────────────────────

echo "--- [7] Timeout (no server) ---"

start_ts=$(date +%s)
echo -n "data" | "$BINARY" -c -p $((PORT+5)) -a 127.0.0.1 -w 1
code=$?
end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

check_exit "sender exits non-zero when no server" 1 $code
if [ "$elapsed" -le 3 ]; then
    echo "PASS: sender timed out within expected window (${elapsed}s)"
    ((PASS++))
else
    echo "FAIL: sender took too long (${elapsed}s)"
    ((FAIL++))
fi

echo ""

# ── [8] IPv6 loopback ─────────────────────────────────────────────────────────

echo "--- [8] IPv6 loopback (::1) ---"

start_receiver $((PORT+6)) "$TMP_OUT"
sleep 0.2

echo -n "ipv6 test" | "$BINARY" -c -p $((PORT+6)) -a ::1 -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (IPv6)" 0 $SND_CODE
check_exit "receiver exits 0 (IPv6)" 0 $RCV_CODE
result=$(cat "$TMP_OUT" 2>/dev/null)
check_contains "IPv6 transfer output correct" "ipv6 test" "$result"
rm -f "$TMP_OUT"

echo ""

# ── [9] 1-byte transfer ───────────────────────────────────────────────────────

echo "--- [9] 1-byte transfer ---"

start_receiver $((PORT+7)) "$TMP_OUT"
sleep 0.2

printf 'X' | "$BINARY" -c -p $((PORT+7)) -a 127.0.0.1 -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (1-byte)" 0 $SND_CODE
check_exit "receiver exits 0 (1-byte)" 0 $RCV_CODE
result=$(cat "$TMP_OUT" 2>/dev/null)
check_contains "1-byte output correct" "X" "$result"
rm -f "$TMP_OUT"

echo ""

# ── [10] Exactly MAX_PAYLOAD bytes (1180) ─────────────────────────────────────

echo "--- [10] Exactly MAX_PAYLOAD bytes (1180) ---"

dd if=/dev/urandom of="$TMP_IN" bs=1180 count=1 2>/dev/null

start_receiver $((PORT+8)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+8)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (1180 B)" 0 $SND_CODE
check_exit "receiver exits 0 (1180 B)" 0 $RCV_CODE
check_files_equal "1180-byte file transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [11] MAX_PAYLOAD+1 bytes (1181) — forces 2 segments ──────────────────────

echo "--- [11] MAX_PAYLOAD+1 bytes (1181, forces 2 segments) ---"

dd if=/dev/urandom of="$TMP_IN" bs=1181 count=1 2>/dev/null

start_receiver $((PORT+9)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+9)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (1181 B)" 0 $SND_CODE
check_exit "receiver exits 0 (1181 B)" 0 $RCV_CODE
check_files_equal "1181-byte file transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [12] Binary data (null bytes, 0x00–0xFF) ──────────────────────────────────

echo "--- [12] Binary data (0x00-0xFF) ---"

python3 -c "import sys; sys.stdout.buffer.write(bytes(range(256)))" > "$TMP_IN"

start_receiver $((PORT+10)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+10)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (binary)" 0 $SND_CODE
check_exit "receiver exits 0 (binary)" 0 $RCV_CODE
check_files_equal "binary data transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [13] Receiver stdout (no -o flag) ─────────────────────────────────────────

echo "--- [13] Receiver stdout (no -o) ---"

"$BINARY" -s -p $((PORT+11)) -w $TIMEOUT > "$TMP_OUT" &
RCV_PID=$!
sleep 0.2

echo -n "stdout test" | "$BINARY" -c -p $((PORT+11)) -a 127.0.0.1 -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (stdout)" 0 $SND_CODE
check_exit "receiver exits 0 (stdout)" 0 $RCV_CODE
result=$(cat "$TMP_OUT" 2>/dev/null)
check_contains "stdout output correct" "stdout test" "$result"
rm -f "$TMP_OUT"

echo ""

# ── [14] Receiver timeout — no sender arrives ─────────────────────────────────

echo "--- [14] Receiver timeout (no sender) ---"

start_ts=$(date +%s)
"$BINARY" -s -p $((PORT+12)) -w 1
RCV_CODE=$?
end_ts=$(date +%s)
elapsed=$((end_ts - start_ts))

check_exit "receiver exits non-zero when no sender" 1 $RCV_CODE
if [ "$elapsed" -le 3 ]; then
    echo "PASS: receiver timed out within expected window (${elapsed}s)"
    ((PASS++))
else
    echo "FAIL: receiver took too long (${elapsed}s)"
    ((FAIL++))
fi

echo ""

# ── [15] 10 MB file ───────────────────────────────────────────────────────────

echo "--- [15] Large file transfer (10 MB) ---"

dd if=/dev/urandom of="$TMP_IN" bs=1024 count=10240 2>/dev/null

start_receiver $((PORT+13)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+13)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (10 MB)" 0 $SND_CODE
check_exit "receiver exits 0 (10 MB)" 0 $RCV_CODE
check_files_equal "10 MB file transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── [16] Full window (32 × 1180 = 37 760 bytes) ───────────────────────────────

echo "--- [16] Full window transfer (32 × MAX_PAYLOAD = 37760 B) ---"

dd if=/dev/urandom of="$TMP_IN" bs=37760 count=1 2>/dev/null

start_receiver $((PORT+14)) "$TMP_OUT"
sleep 0.2

"$BINARY" -c -p $((PORT+14)) -a 127.0.0.1 -i "$TMP_IN" -w $TIMEOUT
SND_CODE=$?
wait $RCV_PID
RCV_CODE=$?

check_exit "sender exits 0 (full window)" 0 $SND_CODE
check_exit "receiver exits 0 (full window)" 0 $RCV_CODE
check_files_equal "full-window file transferred correctly" "$TMP_IN" "$TMP_OUT"
rm -f "$TMP_IN" "$TMP_OUT"

echo ""

# ── results ───────────────────────────────────────────────────────────────────

echo "================================="
echo " Results: $PASS passed, $FAIL failed"
echo "================================="

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
