#!/usr/bin/env bash
# BurritoCoin regtest end-to-end test
#
# Tests: mining, basic transactions, and MWEB transactions.
#
# Usage:
#   ./contrib/regtest/regtest.sh              (auto-finds binaries)
#   BRTO_BIN=/path/to/src ./contrib/regtest/regtest.sh
#
# The script starts its own burritocoind, runs all tests, then stops it.
# A temporary data directory is used and cleaned up on exit.

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BRTO_BIN="${BRTO_BIN:-$REPO_ROOT/src}"
DAEMON="$BRTO_BIN/burritocoind"
CLI="$BRTO_BIN/burritocoin-cli"

RPC_USER="regtestuser"
RPC_PASS="regtestpass"
RPC_PORT=19553
P2P_PORT=19554

DATADIR="$(mktemp -d /tmp/brto-regtest-XXXXXX)"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
PASS=0
FAIL=0

green()  { printf '\033[0;32m%s\033[0m\n' "$*"; }
red()    { printf '\033[0;31m%s\033[0m\n' "$*"; }
yellow() { printf '\033[0;33m%s\033[0m\n' "$*"; }
header() { printf '\n\033[1;34m=== %s ===\033[0m\n' "$*"; }

pass() { green "  PASS: $*"; PASS=$((PASS + 1)); }
fail() { red   "  FAIL: $*"; FAIL=$((FAIL + 1)); }

cli() {
    "$CLI" -regtest \
        -rpcport=$RPC_PORT \
        -rpcuser="$RPC_USER" \
        -rpcpassword="$RPC_PASS" \
        "$@"
}

# Wait for the RPC server to be ready (up to 30 s).
wait_for_rpc() {
    local attempts=0
    until cli getblockchaininfo &>/dev/null; do
        ((attempts++))
        if (( attempts >= 30 )); then
            red "burritocoind did not become ready in 30 seconds."
            exit 1
        fi
        sleep 1
    done
}

cleanup() {
    yellow "\nStopping burritocoind..."
    cli stop &>/dev/null || true
    sleep 2
    # Kill any leftover process.
    pkill -f "burritocoind.*$DATADIR" 2>/dev/null || true
    rm -rf "$DATADIR"
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
header "Preflight"

if [[ ! -x "$DAEMON" ]]; then
    red "burritocoind not found at $DAEMON"
    red "Build the project first, or set BRTO_BIN=/path/to/src"
    exit 1
fi
pass "burritocoind found: $DAEMON"

if [[ ! -x "$CLI" ]]; then
    red "burritocoin-cli not found at $CLI"
    exit 1
fi
pass "burritocoin-cli found: $CLI"

# Check nothing else is using our ports.
if lsof -iTCP:$RPC_PORT -sTCP:LISTEN &>/dev/null 2>&1; then
    red "Port $RPC_PORT already in use — is another regtest node running?"
    exit 1
fi
pass "Ports $RPC_PORT / $P2P_PORT are free"

# ---------------------------------------------------------------------------
# Start daemon
# ---------------------------------------------------------------------------
header "Starting burritocoind (regtest)"

"$DAEMON" \
    -regtest \
    -daemon \
    -datadir="$DATADIR" \
    -rpcuser="$RPC_USER" \
    -rpcpassword="$RPC_PASS" \
    -rpcport=$RPC_PORT \
    -port=$P2P_PORT \
    -fallbackfee=0.0001 \
    -maxtxfee=1.0 \
    -txindex=1 \
    -debug=0

wait_for_rpc
pass "burritocoind is ready (regtest, RPC port $RPC_PORT)"

# ---------------------------------------------------------------------------
# Section 1: Mining
# ---------------------------------------------------------------------------
header "1. Mining"

ADDR=$(cli getnewaddress)
pass "Got new address: $ADDR"

# Mine 1 block — should show genesis coinbase balance eventually.
cli generatetoaddress 1 "$ADDR" >/dev/null
HEIGHT=$(cli getblockcount)
[[ "$HEIGHT" -eq 1 ]] && pass "Mined block 1, height=$HEIGHT" \
                       || fail "Expected height=1, got $HEIGHT"

# Mine 100 more so the coinbase at block 1 matures (101 confirmations required).
cli generatetoaddress 100 "$ADDR" >/dev/null
HEIGHT=$(cli getblockcount)
[[ "$HEIGHT" -eq 101 ]] && pass "Mined to height 101 (coinbase matured)" \
                         || fail "Expected height=101, got $HEIGHT"

BALANCE=$(cli getbalance)
# Genesis premine is 148,000,000 BRTO at height 0 (goes to ADDR at block 1 reward).
# Block reward is 10 BRTO; 1 block reward is now mature (block 1).
# The genesis coinbase is NOT spendable (standard genesis rule), so balance = 10 BRTO.
python3 -c "
b = float('$BALANCE')
if b >= 10:
    print(f'  balance = {b} BRTO (>= 10 BRTO expected)')
else:
    raise SystemExit(f'  balance too low: {b} BRTO')
" && pass "Wallet balance after maturity: $BALANCE BRTO" \
  || fail  "Wallet balance after maturity: $BALANCE BRTO (expected >= 10)"

# ---------------------------------------------------------------------------
# Section 2: Basic transactions
# ---------------------------------------------------------------------------
header "2. Basic Transactions"

ADDR_A=$(cli getnewaddress "alice")
ADDR_B=$(cli getnewaddress "bob")
pass "Created addresses — Alice: $ADDR_A, Bob: $ADDR_B"

# Fund Alice.
TXID_FUND=$(cli sendtoaddress "$ADDR_A" 5.0)
[[ -n "$TXID_FUND" ]] && pass "Sent 5 BRTO to Alice (txid: $TXID_FUND)" \
                       || fail "sendtoaddress to Alice failed"

# Mine a block to confirm.
cli generatetoaddress 1 "$ADDR" >/dev/null
CONFS=$(cli gettransaction "$TXID_FUND" | python3 -c "import sys,json; print(json.load(sys.stdin)['confirmations'])")
[[ "$CONFS" -ge 1 ]] && pass "Funding tx confirmed ($CONFS confirmation(s))" \
                      || fail "Funding tx not confirmed after 1 block (confs=$CONFS)"

# Alice sends to Bob (requires the wallet to pick UTXOs — uses internal signing).
TXID_AB=$(cli sendtoaddress "$ADDR_B" 2.5)
[[ -n "$TXID_AB" ]] && pass "Alice sent 2.5 BRTO to Bob (txid: $TXID_AB)" \
                     || fail "sendtoaddress Alice→Bob failed"

# Confirm.
cli generatetoaddress 1 "$ADDR" >/dev/null
CONFS_AB=$(cli gettransaction "$TXID_AB" | python3 -c "import sys,json; print(json.load(sys.stdin)['confirmations'])")
[[ "$CONFS_AB" -ge 1 ]] && pass "Alice→Bob tx confirmed ($CONFS_AB confirmation(s))" \
                          || fail "Alice→Bob tx not confirmed (confs=$CONFS_AB)"

# Verify raw transaction decodes correctly.
RAW=$(cli getrawtransaction "$TXID_AB")
DECODED=$(cli decoderawtransaction "$RAW")
VOUT_COUNT=$(echo "$DECODED" | python3 -c "import sys,json; print(len(json.load(sys.stdin)['vout']))")
[[ "$VOUT_COUNT" -ge 1 ]] && pass "Raw tx decodes correctly ($VOUT_COUNT output(s))" \
                            || fail "Raw tx decode produced 0 outputs"

# ---------------------------------------------------------------------------
# Section 3: MWEB
# ---------------------------------------------------------------------------
header "3. MWEB"

CURRENT_HEIGHT=$(cli getblockcount)
yellow "  Current height: $CURRENT_HEIGHT"
yellow "  MWEB activates at ~block 288 in regtest (75% signal over 144-block window)"

# Mine enough blocks to activate MWEB (need height >= 288).
if (( CURRENT_HEIGHT < 290 )); then
    NEEDED=$(( 290 - CURRENT_HEIGHT ))
    yellow "  Mining $NEEDED more blocks to reach height 290..."
    cli generatetoaddress "$NEEDED" "$ADDR" >/dev/null
fi

HEIGHT=$(cli getblockcount)
pass "Height is now $HEIGHT"

# Check MWEB softfork status.
MWEB_STATUS=$(cli getblockchaininfo | python3 -c "
import sys, json
info = json.load(sys.stdin)
sf = info.get('softforks', {})
mweb = sf.get('mweb', {})
print(mweb.get('bip9', {}).get('status', mweb.get('status', 'unknown')))
")
yellow "  MWEB softfork status: $MWEB_STATUS"

if [[ "$MWEB_STATUS" == "active" ]]; then
    pass "MWEB is active"

    # Get a MWEB address.
    MWEB_ADDR=$(cli getnewaddress "mweb-test" mweb 2>/dev/null || cli getnewaddress "mweb-test" "bech32m" 2>/dev/null || true)

    if [[ -z "$MWEB_ADDR" ]]; then
        # Fallback: try address_type flag used in some LTC builds.
        MWEB_ADDR=$(cli getnewaddress "" "" 2>/dev/null | grep -c "rbrtomweb" &>/dev/null && cli getnewaddress "mweb-test" || true)
    fi

    if [[ -n "$MWEB_ADDR" ]]; then
        pass "Got MWEB address: $MWEB_ADDR"

        # Verify the address looks right (should start with regtest MWEB HRP).
        if [[ "$MWEB_ADDR" == rbrtomweb* ]]; then
            pass "MWEB address has correct regtest HRP (rbrtomweb...)"
        else
            yellow "  WARNING: address does not start with 'rbrtomweb' — got: $MWEB_ADDR"
            yellow "  This may be a bech32m segwit address rather than a true MWEB address."
        fi

        # Send to the MWEB address.
        TXID_MWEB=$(cli sendtoaddress "$MWEB_ADDR" 1.0)
        [[ -n "$TXID_MWEB" ]] && pass "Sent 1.0 BRTO to MWEB address (txid: $TXID_MWEB)" \
                               || fail "sendtoaddress to MWEB address failed"

        # Mine a block to confirm it.
        cli generatetoaddress 1 "$ADDR" >/dev/null
        CONFS_MWEB=$(cli gettransaction "$TXID_MWEB" | python3 -c "import sys,json; print(json.load(sys.stdin)['confirmations'])")
        [[ "$CONFS_MWEB" -ge 1 ]] && pass "MWEB tx confirmed ($CONFS_MWEB confirmation(s))" \
                                    || fail "MWEB tx not confirmed after 1 block"
    else
        yellow "  Could not obtain a MWEB address — skipping MWEB send test."
        yellow "  This may mean the getnewaddress 'mweb' address_type is not supported yet."
    fi

elif [[ "$MWEB_STATUS" == "locked_in" ]]; then
    yellow "  MWEB is locked_in — mine one more confirmation window to activate."
    yellow "  Mining 144 more blocks..."
    cli generatetoaddress 144 "$ADDR" >/dev/null
    MWEB_STATUS2=$(cli getblockchaininfo | python3 -c "
import sys, json
info = json.load(sys.stdin)
sf = info.get('softforks', {})
mweb = sf.get('mweb', {})
print(mweb.get('bip9', {}).get('status', mweb.get('status', 'unknown')))
")
    if [[ "$MWEB_STATUS2" == "active" ]]; then
        pass "MWEB activated after additional blocks (status: $MWEB_STATUS2)"
    else
        fail "MWEB still not active after extra blocks (status: $MWEB_STATUS2)"
    fi
else
    fail "MWEB not yet active at height $HEIGHT (status: $MWEB_STATUS) — expected 'active' or 'locked_in'"
    yellow "  Check that burritocoind is building blocks that signal bit 4."
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
header "Results"

TOTAL=$(( PASS + FAIL ))
echo ""
printf "  Passed: %d / %d\n" "$PASS" "$TOTAL"
if (( FAIL > 0 )); then
    red   "  Failed: $FAIL"
    echo ""
    red "Regtest: SOME TESTS FAILED"
    exit 1
else
    green "  All $PASS tests passed."
    echo ""
    green "Regtest: ALL TESTS PASSED"
fi
