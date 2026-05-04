#!/usr/bin/env bash
# Bootstrap a second burritocoind instance on the same VPS, peered with the
# primary daemon over loopback.
#
# Why: a freshly-installed burritocoind that addnodes only the seed VPS will
# learn exactly one peer. That's enough for it to relay, but getblocktemplate
# (used by external mining software) treats a peer count of 0 on the *seed*
# itself as "not connected" and refuses to mine. Running a sibling instance
# on the same machine guarantees the primary always has at least one peer.
#
# Layout produced by this script (matches the live deployment in the handoff):
#   Primary node:   /root/.burritocoin/                   (assumed running already)
#   Secondary node: /root/.burritocoin-peer/
#   Secondary P2P:  127.0.0.1:29227
#   Secondary RPC:  127.0.0.1:29226
#
# Re-running the script is safe — it only adds missing pieces.
#
# Usage: sudo ./contrib/vps/setup-second-peer.sh

set -euo pipefail

PRIMARY_DATADIR="${PRIMARY_DATADIR:-/root/.burritocoin}"
PRIMARY_CONF="${PRIMARY_CONF:-$PRIMARY_DATADIR/burritocoin.conf}"
SECONDARY_DATADIR="${SECONDARY_DATADIR:-/root/.burritocoin-peer}"
SECONDARY_CONF="$SECONDARY_DATADIR/burritocoin.conf"
BINARY="${BINARY:-/root/BurritoCoin/src/burritocoind}"

PRIMARY_P2P_PORT=9227
SECONDARY_P2P_PORT=29227
SECONDARY_RPC_PORT=29226

green()  { printf '\033[0;32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[0;33m%s\033[0m\n' "$*"; }
red()    { printf '\033[0;31m%s\033[0m\n' "$*"; }
die()    { red "ERROR: $*"; exit 1; }

[[ $EUID -eq 0 ]] || die "Run as root."
[[ -x "$BINARY" ]] || die "burritocoind not executable at $BINARY (set BINARY env var if elsewhere)."
[[ -f "$PRIMARY_CONF" ]] || die "Primary config not found at $PRIMARY_CONF."

# -------- 1. Secondary datadir + config -----------------------------------
install -d -m 0700 "$SECONDARY_DATADIR"

if [[ ! -f "$SECONDARY_CONF" ]]; then
    SECONDARY_RPC_PASS="$(tr -dc 'a-zA-Z0-9' < /dev/urandom | head -c 32)"
    cat > "$SECONDARY_CONF" <<EOF
# Sibling peer for the primary BurritoCoin daemon. Keeps the primary's peer
# count above zero so getblocktemplate works for external miners.
rpcuser=brtopeer
rpcpassword=$SECONDARY_RPC_PASS
rpcallowip=127.0.0.1
rpcbind=127.0.0.1
rpcport=$SECONDARY_RPC_PORT

listen=1
bind=127.0.0.1
port=$SECONDARY_P2P_PORT

# Connect only to the primary instance — this node exists solely to be a peer.
addnode=127.0.0.1:$PRIMARY_P2P_PORT
maxconnections=8
dbcache=128
daemon=0
server=1
debug=0
EOF
    chmod 0600 "$SECONDARY_CONF"
    green "Wrote $SECONDARY_CONF"
else
    yellow "Secondary config already exists — leaving it untouched."
fi

# -------- 2. Systemd service ----------------------------------------------
SERVICE_FILE=/etc/systemd/system/burritocoind-peer.service
if [[ ! -f "$SERVICE_FILE" ]]; then
    cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=BurritoCoin sibling peer daemon
After=network-online.target burritocoind.service
Wants=network-online.target

[Service]
Type=simple
ExecStart=$BINARY -datadir=$SECONDARY_DATADIR -conf=$SECONDARY_CONF
Restart=on-failure
RestartSec=10s
TimeoutStopSec=60s
User=root
Nice=15

[Install]
WantedBy=multi-user.target
EOF
    chmod 0644 "$SERVICE_FILE"
    systemctl daemon-reload
    green "Installed $SERVICE_FILE"
fi

systemctl enable burritocoind-peer.service >/dev/null
systemctl restart burritocoind-peer.service

# -------- 3. Wire the primary back to the secondary -----------------------
ADDNODE_LINE="addnode=127.0.0.1:$SECONDARY_P2P_PORT"
if ! grep -qE "^addnode=127\.0\.0\.1:$SECONDARY_P2P_PORT\b" "$PRIMARY_CONF"; then
    printf '\n# Loopback peer — see contrib/vps/setup-second-peer.sh\n%s\n' "$ADDNODE_LINE" >> "$PRIMARY_CONF"
    green "Appended $ADDNODE_LINE to $PRIMARY_CONF"
    systemctl restart burritocoind.service
    yellow "Restarted primary burritocoind to pick up the new addnode."
else
    yellow "Primary config already references the loopback peer — no change."
fi

# -------- 4. Verify --------------------------------------------------------
sleep 8
PRIMARY_PEERS=$(curl -s --user "$(grep ^rpcuser "$PRIMARY_CONF" | cut -d= -f2):$(grep ^rpcpassword "$PRIMARY_CONF" | cut -d= -f2)" \
    --data-binary '{"jsonrpc":"1.0","id":"verify","method":"getconnectioncount","params":[]}' \
    -H 'content-type: text/plain;' http://127.0.0.1:9226/ 2>/dev/null | sed -E 's/.*"result":([0-9]+).*/\1/')

printf '\n'
if [[ "${PRIMARY_PEERS:-0}" -ge 1 ]]; then
    green "Primary now reports $PRIMARY_PEERS peer(s). External miners can call getblocktemplate."
else
    yellow "Couldn't auto-confirm peer count. Check manually:"
    yellow "  bcli getconnectioncount"
    yellow "  bcli getpeerinfo"
fi
