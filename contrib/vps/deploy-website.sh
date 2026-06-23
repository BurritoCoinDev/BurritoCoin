#!/usr/bin/env bash
# BurritoCoin website deploy script
#
# Syncs the repo's website/ directory into the nginx web root and reloads
# nginx. Run on the VPS as root (or via sudo) from a fresh `git pull`.
#
# Usage:
#   sudo ./contrib/vps/deploy-website.sh [--prune] [--dry-run] [WEBROOT]
#
# WEBROOT resolution order:
#   1. positional argument
#   2. $BRTO_WEBROOT environment variable
#   3. /var/www/burritoco.in   (if it exists)
#   4. /var/www/html           (Ubuntu nginx default)
#
# Flags:
#   --prune    Delete files in WEBROOT that are not in website/.
#              Off by default — safer.
#   --dry-run  Show what would change. Implies rsync -n. nginx is NOT reloaded.

set -euo pipefail

# ---------------------------------------------------------------------------
# Args
# ---------------------------------------------------------------------------
PRUNE=0
DRY_RUN=0
WEBROOT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prune)   PRUNE=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        --*)
            printf 'Unknown option: %s\n' "$1" >&2
            exit 1
            ;;
        *)
            [[ -z "$WEBROOT" ]] || { printf 'Multiple webroots given\n' >&2; exit 1; }
            WEBROOT="$1"
            shift
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SOURCE_DIR="$REPO_ROOT/website"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
green()  { printf '\033[0;32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[0;33m%s\033[0m\n' "$*"; }
red()    { printf '\033[0;31m%s\033[0m\n' "$*"; }
step()   { printf '\n\033[1;34m[%s]\033[0m %s\n' "$(date +%H:%M:%S)" "$*"; }
die()    { red "ERROR: $*"; exit 1; }

# ---------------------------------------------------------------------------
# Preflight
# ---------------------------------------------------------------------------
step "Preflight checks"

[[ $EUID -eq 0 ]] || die "Must run as root (use: sudo $0)"
[[ -d "$SOURCE_DIR" ]]            || die "website/ not found at $SOURCE_DIR"
[[ -f "$SOURCE_DIR/index.html" ]] || die "$SOURCE_DIR/index.html missing"

command -v rsync     >/dev/null || die "rsync not installed (apt install rsync)"
command -v nginx     >/dev/null || die "nginx not installed"
command -v systemctl >/dev/null || die "systemd required"

green "Preflight OK."

# ---------------------------------------------------------------------------
# Resolve webroot
# ---------------------------------------------------------------------------
if [[ -z "$WEBROOT" ]]; then
    WEBROOT="${BRTO_WEBROOT:-}"
fi
if [[ -z "$WEBROOT" ]]; then
    if   [[ -d /var/www/burritoco.in ]]; then WEBROOT=/var/www/burritoco.in
    else                                       WEBROOT=/var/www/html
    fi
fi

[[ -d "$WEBROOT" ]] || die "Web root $WEBROOT does not exist (create it or pass another path)"

step "Source: $SOURCE_DIR"
step "Target: $WEBROOT"
[[ $PRUNE   -eq 1 ]] && yellow "--prune ON: files in $WEBROOT not in website/ will be DELETED"
[[ $DRY_RUN -eq 1 ]] && yellow "--dry-run ON: no changes will be made"

# ---------------------------------------------------------------------------
# Determine nginx user (Ubuntu default: www-data)
# ---------------------------------------------------------------------------
NGINX_USER="$(awk '/^\s*user\s+/ {gsub(";",""); print $2; exit}' /etc/nginx/nginx.conf 2>/dev/null || true)"
NGINX_USER="${NGINX_USER:-www-data}"
green "nginx user: $NGINX_USER"

# ---------------------------------------------------------------------------
# rsync
# ---------------------------------------------------------------------------
step "Syncing files"

RSYNC_FLAGS=(-rv --checksum --no-perms --chmod=D755,F644 --chown="$NGINX_USER:$NGINX_USER")
[[ $PRUNE   -eq 1 ]] && RSYNC_FLAGS+=(--delete)
[[ $DRY_RUN -eq 1 ]] && RSYNC_FLAGS+=(--dry-run)

rsync "${RSYNC_FLAGS[@]}" "$SOURCE_DIR/" "$WEBROOT/"

if [[ $DRY_RUN -eq 1 ]]; then
    yellow "Dry run complete — nginx NOT reloaded."
    exit 0
fi

# ---------------------------------------------------------------------------
# nginx test + reload
# ---------------------------------------------------------------------------
step "Testing nginx config"
if ! nginx -t; then
    die "nginx config test failed — NOT reloading. Fix the config and rerun."
fi
green "nginx config OK"

step "Reloading nginx"
systemctl reload nginx
green "nginx reloaded"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
printf '\n'
green "========================================"
green " Website deploy complete"
green "========================================"
yellow "Deployed to:  $WEBROOT"
yellow "Verify with:  curl -sSI https://burritoco.in/ | head -5"
yellow "             curl -sS  https://burritoco.in/mine-mac.html | grep BREW_PREFIX"
