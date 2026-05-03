#!/usr/bin/env bash
# BurritoCoin VPS node setup script
#
# Installs burritocoind as a systemd service on a fresh Ubuntu/Debian VPS.
# Run as root or with sudo.
#
# Usage:
#   sudo ./contrib/vps/setup.sh [mainnet|testnet]   (default: mainnet)
#
# What it does:
#   1. Creates a dedicated 'burritocoin' system user
#   2. Installs the burritocoind binary to /usr/local/bin/
#   3. Writes /etc/burritocoin/burritocoin.conf (from template, prompting for RPC creds)
#   4. Installs and enables the systemd service
#   5. Opens the required firewall port (ufw if available)

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults / args
# ---------------------------------------------------------------------------
NETWORK="${1:-mainnet}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BINARY_SRC="$REPO_ROOT/src/burritocoind"
CLI_SRC="$REPO_ROOT/src/burritocoin-cli"
BINARY_DEST="/usr/local/bin/burritocoind"
CLI_DEST="/usr/local/bin/burritocoin-cli"
SERVICE_SRC="$REPO_ROOT/contrib/init/burritocoind.service"
CONF_DIR="/etc/burritocoin"
DATA_DIR="/var/lib/burritocoind"
SERVICE_USER="burritocoin"
SERVICE_GROUP="burritocoin"

if [[ "$NETWORK" == "testnet" ]]; then
    CONF_TEMPLATE="$SCRIPT_DIR/burritocoin-testnet.conf"
    P2P_PORT=19227
else
    CONF_TEMPLATE="$SCRIPT_DIR/burritocoin.conf"
    P2P_PORT=9227
fi

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
green()  { printf '\033[0;32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[0;33m%s\033[0m\n' "$*"; }
red()    { printf '\033[0;31m%s\033[0m\n' "$*"; }
step()   { printf '\n\033[1;34m[%s]\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; }
die()    { red "ERROR: $*"; exit 1; }

require_root() {
    if [[ $EUID -ne 0 ]]; then
        die "This script must be run as root (use: sudo $0)"
    fi
}

gen_password() {
    tr -dc 'a-zA-Z0-9' < /dev/urandom | head -c 32
}

# ---------------------------------------------------------------------------
# Preflight
# ---------------------------------------------------------------------------
require_root

step "Preflight checks"

[[ -x "$BINARY_SRC" ]] || die "burritocoind not found at $BINARY_SRC — build the project first."
[[ -x "$CLI_SRC"    ]] || die "burritocoin-cli not found at $CLI_SRC — build the project first."
[[ -f "$SERVICE_SRC" ]] || die "systemd service file not found at $SERVICE_SRC"
[[ -f "$CONF_TEMPLATE" ]] || die "Config template not found at $CONF_TEMPLATE"

command -v systemctl &>/dev/null || die "systemd not found — this script requires a systemd-based OS."

green "All preflight checks passed."

# ---------------------------------------------------------------------------
# 1. Create system user
# ---------------------------------------------------------------------------
step "Creating system user '$SERVICE_USER'"

if id "$SERVICE_USER" &>/dev/null; then
    yellow "User '$SERVICE_USER' already exists — skipping creation."
else
    useradd --system --no-create-home --shell /usr/sbin/nologin \
            --comment "BurritoCoin daemon" \
            --user-group "$SERVICE_USER"
    green "Created user: $SERVICE_USER"
fi

# ---------------------------------------------------------------------------
# 2. Install binaries
# ---------------------------------------------------------------------------
step "Installing binaries"

install -o root -g root -m 0755 "$BINARY_SRC" "$BINARY_DEST"
green "Installed burritocoind → $BINARY_DEST"

install -o root -g root -m 0755 "$CLI_SRC" "$CLI_DEST"
green "Installed burritocoin-cli → $CLI_DEST"

# ---------------------------------------------------------------------------
# 3. Create directories
# ---------------------------------------------------------------------------
step "Creating data and config directories"

install -d -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 0710 "$DATA_DIR"
green "Data dir: $DATA_DIR"

install -d -o root -g "$SERVICE_GROUP" -m 0710 "$CONF_DIR"
green "Config dir: $CONF_DIR"

# ---------------------------------------------------------------------------
# 4. Write config file
# ---------------------------------------------------------------------------
CONF_DEST="$CONF_DIR/burritocoin.conf"
step "Writing config: $CONF_DEST"

if [[ -f "$CONF_DEST" ]]; then
    yellow "Config already exists at $CONF_DEST — leaving it untouched."
    yellow "Delete it and re-run to regenerate."
else
    # Generate RPC credentials if not already set in the environment.
    RPC_USER="${BRTO_RPC_USER:-brtonode}"
    RPC_PASS="${BRTO_RPC_PASS:-$(gen_password)}"

    sed \
        -e "s|__RPC_USER__|$RPC_USER|" \
        -e "s|__RPC_PASS__|$RPC_PASS|" \
        "$CONF_TEMPLATE" > "$CONF_DEST"

    chown root:"$SERVICE_GROUP" "$CONF_DEST"
    chmod 0640 "$CONF_DEST"
    green "Config written."
    yellow "RPC credentials:"
    yellow "  rpcuser=$RPC_USER"
    yellow "  rpcpassword=$RPC_PASS"
    yellow "Save these — they won't be shown again."
fi

# ---------------------------------------------------------------------------
# 5. Install and enable systemd service
# ---------------------------------------------------------------------------
step "Installing systemd service"

# Patch the service file to use our binary location instead of /usr/bin/.
SERVICE_DEST="/etc/systemd/system/burritocoind.service"
sed "s|/usr/bin/burritocoind|$BINARY_DEST|g" "$SERVICE_SRC" > "$SERVICE_DEST"
chmod 0644 "$SERVICE_DEST"

systemctl daemon-reload
systemctl enable burritocoind.service
green "Service installed and enabled."

# ---------------------------------------------------------------------------
# 6. Open firewall port
# ---------------------------------------------------------------------------
step "Firewall"

if command -v ufw &>/dev/null; then
    ufw allow "$P2P_PORT"/tcp comment "BurritoCoin P2P ($NETWORK)"
    green "ufw: opened port $P2P_PORT/tcp"
else
    yellow "ufw not found — open port $P2P_PORT/tcp manually."
fi

# ---------------------------------------------------------------------------
# 7. Start the service
# ---------------------------------------------------------------------------
step "Starting burritocoind"

systemctl start burritocoind.service
sleep 3

if systemctl is-active --quiet burritocoind.service; then
    green "burritocoind is running."
else
    red "burritocoind failed to start. Check logs:"
    red "  journalctl -u burritocoind.service -n 50"
    exit 1
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
printf '\n'
green "========================================"
green " BurritoCoin node setup complete"
green "========================================"
printf '\n'
yellow "Useful commands:"
printf '  Status:  systemctl status burritocoind\n'
printf '  Logs:    journalctl -u burritocoind -f\n'
printf '  CLI:     burritocoin-cli -conf=%s getblockchaininfo\n' "$CONF_DEST"
printf '  Stop:    systemctl stop burritocoind\n'
printf '\n'
yellow "Data directory: $DATA_DIR"
yellow "Config file:    $CONF_DEST"
