#!/usr/bin/env bash
# Install ElectrumX with BurritoCoin support on the VPS, then wire it into
# the existing btc-rpc-explorer.
#
# What this gives you: address/balance lookups in the block explorer (the
# pages that currently say "No address API is configured").
#
# Usage: sudo ./contrib/vps/setup-electrumx.sh
#
# After the first run, the initial chain index will take a few minutes — the
# chain is short so it's not the multi-day affair it would be on a long-lived
# coin. You can watch progress with:  journalctl -u electrumx -f
#
# Re-running the script is safe; existing files are kept.

set -euo pipefail

PRIMARY_CONF="${PRIMARY_CONF:-/root/.burritocoin/burritocoin.conf}"
ELECTRUMX_DIR="${ELECTRUMX_DIR:-/opt/electrumx}"
ELECTRUMX_DB="${ELECTRUMX_DB:-/var/lib/electrumx/db}"
ELECTRUMX_USER="${ELECTRUMX_USER:-electrumx}"
ELECTRUMX_REPO="${ELECTRUMX_REPO:-https://github.com/spesmilo/electrumx.git}"
# Pinned deliberately. The clone used to track HEAD, so two machines set up
# months apart got different ElectrumX versions: 2.0.0 renamed Coin.header_hash
# to header_hash_rev, which breaks the BurritoCoin genesis_block override below
# at startup. This commit is the revision running on the live explorer.
ELECTRUMX_REF="${ELECTRUMX_REF:-24865dc3a8ac8d360e467df704777c1bbef72706}"
EXPLORER_ENV="${EXPLORER_ENV:-/etc/btc-rpc-explorer/.env}" # leave blank if explorer .env lives elsewhere

green()  { printf '\033[0;32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[0;33m%s\033[0m\n' "$*"; }
red()    { printf '\033[0;31m%s\033[0m\n' "$*"; }
die()    { red "ERROR: $*"; exit 1; }

[[ $EUID -eq 0 ]] || die "Run as root."
[[ -f "$PRIMARY_CONF" ]] || die "Primary burritocoin.conf not found at $PRIMARY_CONF."

# Credentials: prefer the environment, fall back to plaintext rpcuser/
# rpcpassword in the node config. A config that uses the safer rpcauth= form
# stores only a salted hash, so the plaintext cannot be recovered from it —
# in that case pass the password in, e.g.
#   sudo RPC_USER=brtorpc RPC_PASS="$(cat ~/.burritocoin/rpcpass.txt)" \
#        PRIMARY_CONF=~/.burritocoin/burritocoin.conf ./setup-electrumx.sh
RPC_USER="${RPC_USER:-$(awk -F= '/^rpcuser=/{print $2; exit}' "$PRIMARY_CONF")}"
RPC_PASS="${RPC_PASS:-$(awk -F= '/^rpcpassword=/{print $2; exit}' "$PRIMARY_CONF")}"
RPC_PORT=$(awk -F= '/^rpcport=/{print $2; exit}' "$PRIMARY_CONF")
RPC_PORT="${RPC_PORT:-9226}"

[[ -n "$RPC_USER" && -n "$RPC_PASS" ]] || die "No RPC credentials. Set RPC_USER and RPC_PASS in the environment, or put rpcuser=/rpcpassword= in $PRIMARY_CONF."

# -------- 1. System packages ----------------------------------------------
green "[1/6] Installing system packages..."
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    python3 python3-pip python3-venv python3-dev \
    libleveldb-dev build-essential git pkg-config

# -------- 2. User, dirs ---------------------------------------------------
green "[2/6] Creating user and directories..."
id "$ELECTRUMX_USER" &>/dev/null || \
    useradd --system --create-home --home-dir "/var/lib/electrumx" \
            --shell /usr/sbin/nologin "$ELECTRUMX_USER"
install -d -o "$ELECTRUMX_USER" -g "$ELECTRUMX_USER" -m 0750 "$ELECTRUMX_DB"

# -------- 3. Clone + install ElectrumX ------------------------------------
green "[3/6] Installing ElectrumX..."
if [[ ! -d "$ELECTRUMX_DIR/.git" ]]; then
    git clone --quiet "$ELECTRUMX_REPO" "$ELECTRUMX_DIR"
    git -C "$ELECTRUMX_DIR" checkout --quiet "$ELECTRUMX_REF"
fi

if [[ ! -d "$ELECTRUMX_DIR/.venv" ]]; then
    python3 -m venv "$ELECTRUMX_DIR/.venv"
fi
"$ELECTRUMX_DIR/.venv/bin/pip" install --upgrade --quiet pip wheel
"$ELECTRUMX_DIR/.venv/bin/pip" install --quiet -e "$ELECTRUMX_DIR"
# uvloop is required, not optional: step 5 writes EVENT_LOOP_POLICY=uvloop
# into /etc/electrumx.conf, and ElectrumX imports it during Env() before
# it does anything else — a missing uvloop aborts startup immediately with
# ModuleNotFoundError, not a fallback to the default asyncio loop.
"$ELECTRUMX_DIR/.venv/bin/pip" install --quiet plyvel aiohttp pylru uvloop

# -------- 4. Inject BurritoCoin Coin class --------------------------------
green "[4/6] Patching coins.py with BurritoCoin class..."
COINS_PY="$ELECTRUMX_DIR/src/electrumx/lib/coins.py"
[[ -f "$COINS_PY" ]] || die "ElectrumX layout changed — coins.py not at $COINS_PY."

if ! grep -q "^class BurritoCoin" "$COINS_PY"; then
    cat >> "$COINS_PY" <<'PYEOF'


class BurritoCoin(Coin):
    """BurritoCoin — Scrypt PoW cryptocurrency.

    Constants mirror src/chainparams.cpp on the BurritoCoinDev/BurritoCoin
    repo. If BurritoCoin parameters change (new prefixes, new network magic),
    update them here too and reindex the ElectrumX DB.
    """
    NAME = "BurritoCoin"
    SHORTNAME = "BRTO"
    NET = "mainnet"
    XPUB_VERBYTES = bytes.fromhex("0188D9CE")
    XPRV_VERBYTES = bytes.fromhex("0188D26A")
    P2PKH_VERBYTE = bytes.fromhex("19")
    P2SH_VERBYTES = (bytes.fromhex("1c"), bytes.fromhex("05"))
    WIF_BYTE = bytes.fromhex("99")
    GENESIS_HASH = ('44615751d966cf772a051f65b8df4f39'
                    '87adc48be1749a699369a18517418dce')
    DESERIALIZER = lib_tx.DeserializerSegWit
    TX_COUNT = 100
    TX_COUNT_HEIGHT = 100
    TX_PER_BLOCK = 1
    RPC_PORT = 9226
    REORG_LIMIT = 800
    PEERS: list = []

    @classmethod
    def genesis_block(cls, block):
        # The 148M BRTO premine sits in this coinbase. The default
        # Coin.genesis_block() strips the genesis coinbase as unspendable,
        # which would cause any later tx spending the premine to fail with
        # "UTXO not found in h table". Override to preserve the UTXO.
        from electrumx.lib.hash import hash_to_hex_str
        header = cls.block_header(block, 0)
        # ElectrumX 2.0.0 renamed header_hash -> header_hash_rev (same
        # double_sha256 implementation). Accept whichever this build provides
        # so the class is not tied to one revision.
        _header_hash = getattr(cls, "header_hash", None) or cls.header_hash_rev
        header_hex_hash = hash_to_hex_str(_header_hash(header))
        if header_hex_hash != cls.GENESIS_HASH:
            raise CoinError(f"genesis hash {header_hex_hash} expected {cls.GENESIS_HASH}")
        return block
PYEOF
fi

# -------- 5. Config + service ---------------------------------------------
green "[5/6] Writing /etc/electrumx.conf and systemd unit..."
cat > /etc/electrumx.conf <<EOF
COIN=BurritoCoin
DAEMON_URL=http://${RPC_USER}:${RPC_PASS}@127.0.0.1:${RPC_PORT}/
DB_DIRECTORY=${ELECTRUMX_DB}
DB_ENGINE=leveldb
SERVICES=tcp://0.0.0.0:50001,rpc://127.0.0.1:8000
EVENT_LOOP_POLICY=uvloop
EOF
chmod 0640 /etc/electrumx.conf
chown root:"$ELECTRUMX_USER" /etc/electrumx.conf

cat > /etc/systemd/system/electrumx.service <<EOF
[Unit]
Description=ElectrumX server for BurritoCoin
After=network-online.target burritocoind.service
Wants=network-online.target

[Service]
Type=simple
EnvironmentFile=/etc/electrumx.conf
ExecStart=${ELECTRUMX_DIR}/.venv/bin/electrumx_server
User=${ELECTRUMX_USER}
Group=${ELECTRUMX_USER}
LimitNOFILE=65536
TimeoutStopSec=10min
RestartSec=10s
Restart=on-failure
Nice=10

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable electrumx >/dev/null
systemctl restart electrumx

# -------- 6. Wire btc-rpc-explorer ----------------------------------------
green "[6/6] Wiring btc-rpc-explorer..."
if [[ -n "$EXPLORER_ENV" && -f "$EXPLORER_ENV" ]]; then
    sed -i.bak \
        -e 's|^BTCEXP_ADDRESS_API=.*|BTCEXP_ADDRESS_API=electrumx|' \
        -e 's|^BTCEXP_ELECTRUMX_SERVERS=.*|BTCEXP_ELECTRUMX_SERVERS=tcp://127.0.0.1:50001|' \
        "$EXPLORER_ENV"
    grep -q '^BTCEXP_ADDRESS_API=' "$EXPLORER_ENV" || \
        printf '\nBTCEXP_ADDRESS_API=electrumx\n' >> "$EXPLORER_ENV"
    grep -q '^BTCEXP_ELECTRUMX_SERVERS=' "$EXPLORER_ENV" || \
        printf 'BTCEXP_ELECTRUMX_SERVERS=tcp://127.0.0.1:50001\n' >> "$EXPLORER_ENV"
    green "Updated $EXPLORER_ENV — restart the explorer to pick it up."
else
    yellow "Explorer .env not found at $EXPLORER_ENV. Add these lines manually:"
    yellow "  BTCEXP_ADDRESS_API=electrumx"
    yellow "  BTCEXP_ELECTRUMX_SERVERS=tcp://127.0.0.1:50001"
    yellow "Then restart the explorer service."
fi

printf '\n'
green "ElectrumX is starting up. Initial index will take a few minutes."
yellow "Tail logs:           journalctl -u electrumx -f"
yellow "Smoke-test (TCP):    echo '{\"id\":1,\"method\":\"server.version\",\"params\":[]}' | nc 127.0.0.1 50001"
