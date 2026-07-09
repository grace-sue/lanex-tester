#!/usr/bin/env bash
# Offline tests for Milestones 1 & 2 — no EdgeRouter or LAN-EX hardware required.
# Verifies:
#   M1  config parsing (new keys in config/targetBandwidth.conf)
#   M2  connection-drop detection + iperf3 command flag construction (via a fake ssh)
#
# Usage:  bash tests/run_offline_tests.sh
set -u
cd "$(dirname "$0")/.."   # run from repo root so config/ paths resolve

FAILS=0
check() { # desc, expected, actual
    if [ "$2" = "$3" ]; then
        echo "  PASS  $1"
    else
        echo "  FAIL  $1"
        echo "          expected: $2"
        echo "          actual:   $3"
        FAILS=$((FAILS + 1))
    fi
}

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Building harness..."
g++ -I. tests/harness.cpp iperfExecutor.cpp serverConfigurationLoader.cpp \
    -lncurses -pthread -o "$TMP/harness" || { echo "BUILD FAILED"; exit 1; }

echo
echo "M1 — config parsing"
check "new config keys load with expected values" \
    "tx=90 rx=190 phaseDuration=5 soakDuration=30 soakCap=10 maxConnDrops=0 retries=3" \
    "$("$TMP/harness" config)"

# Fake ssh: logs its args, then emits canned iperf3 output selected by FAKE_MODE.
mkdir -p "$TMP/bin"
cat > "$TMP/bin/ssh" <<'SSH'
#!/bin/sh
echo "$*" >> "$SSH_LOG"
case "$FAKE_MODE" in
  COMPLETED) printf 'Connecting to host\n[  5] connected to x\n[  5]   0.00-1.00   sec  100 MBytes   900 Mbits/sec\n[  5]   0.00-5.00   sec  500 MBytes   900 Mbits/sec   receiver\niperf Done.\n' ;;
  DROP)      printf 'Connecting to host\n[  5] connected to x\n[  5]   0.00-1.00   sec  100 MBytes   900 Mbits/sec\n' ;;
  SETUP)     printf 'iperf3: error - unable to connect to server: Connection refused\n' ;;
esac
SSH
chmod +x "$TMP/bin/ssh"
export PATH="$TMP/bin:$PATH"
export SSH_LOG="$TMP/ssh.log"

echo
echo "M2 — connection-drop detection"
check "clean finish        -> COMPLETED"       "COMPLETED"       "$(FAKE_MODE=COMPLETED "$TMP/harness" iperf 10 1 0)"
check "died mid-stream     -> CONNECTION_DROP" "CONNECTION_DROP" "$(FAKE_MODE=DROP      "$TMP/harness" iperf 10 1 0)"
check "never connected     -> SETUP_ERROR"     "SETUP_ERROR"     "$(FAKE_MODE=SETUP     "$TMP/harness" iperf 10 1 0)"

echo
echo "M2 — iperf3 command flags"
: > "$SSH_LOG"; FAKE_MODE=COMPLETED "$TMP/harness" iperf 10 1 0 >/dev/null
check "cap + bidir (no -R)" "iperf3 -c 10.0.0.2 --port 5201 -t 5 -b 10m --bidir" \
    "$(grep -o 'iperf3 .*' "$SSH_LOG")"
: > "$SSH_LOG"; FAKE_MODE=COMPLETED "$TMP/harness" iperf 0 0 1 >/dev/null
check "reverse, uncapped"   "iperf3 -c 10.0.0.2 --port 5201 -t 5 -R" \
    "$(grep -o 'iperf3 .*' "$SSH_LOG")"

echo
if [ "$FAILS" -eq 0 ]; then
    echo "ALL OFFLINE TESTS PASSED"
else
    echo "$FAILS TEST(S) FAILED"
    exit 1
fi
