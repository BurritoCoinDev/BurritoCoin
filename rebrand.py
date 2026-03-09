#!/usr/bin/env python3
"""
BurritoCoin rebranding script.
Replaces Bitcoin/Litecoin references with BurritoCoin/BRTO throughout the codebase.

Usage:
    python3 rebrand.py           # dry run (shows what would change)
    python3 rebrand.py --apply   # apply changes
"""

import os
import sys
import re
import argparse
from pathlib import Path

# ─────────────────────────────────────────────────────────────
# Replacement rules (applied IN ORDER – order matters!)
# Each entry: (pattern, replacement, use_regex, description)
# ─────────────────────────────────────────────────────────────
REPLACEMENTS = [
    # ── Litecoin variants ──────────────────────────────────────
    ("LITECOIN",        "BURRITOCOIN",      False, "ALL-CAPS coin name"),
    ("LitecoinCore",    "BurritoCoinCore",  False, "Client name"),
    ("Litecoin Core",   "BurritoCoin Core", False, "Spaced client name"),
    ("Litecoin",        "BurritoCoin",      False, "Title-case coin name"),
    ("litecoin",        "burritocoin",      False, "Lower-case coin name"),

    # ── Bitcoin remnants ──────────────────────────────────────
    ("BitcoinCore",     "BurritoCoinCore",  False, "Bitcoin client name"),
    ("Bitcoin Core",    "BurritoCoin Core", False, "Bitcoin spaced client name"),
    ("Bitcoin",         "BurritoCoin",      False, "Title-case bitcoin"),
    ("bitcoin",         "burritocoin",      False, "Lower-case bitcoin"),
    ("BITCOIN",         "BURRITOCOIN",      False, "ALL-CAPS bitcoin"),

    # ── Ticker / currency unit ────────────────────────────────
    # LTC as a standalone ticker (not part of a larger word)
    (r'\bLTC\b',        "BRTO",             True,  "LTC ticker symbol"),
    (r'\bBTC\b',        "BRTO",             True,  "BTC ticker symbol"),

    # ── Bech32 / MWEB human-readable parts ───────────────────
    # These appear as string literals like "ltc", "tltc", "rltc"
    ('"tltc"',          '"tbrto"',          False, "Testnet bech32 HRP"),
    ('"rltc"',          '"rbrto"',          False, "Regtest bech32 HRP"),
    ('"ltc"',           '"brto"',           False, "Mainnet bech32 HRP"),
    ('"tmweb"',         '"tbrtomweb"',      False, "Testnet MWEB HRP"),
    ('"ltcmweb"',       '"brtomweb"',       False, "Mainnet MWEB HRP"),

    # ── Unit names (in bitcoinunits.cpp) ─────────────────────
    ('"liteoshi"',      '"burrioshi"',      False, "Smallest unit name"),
    ('"photons"',       '"morsels"',        False, "Mid unit name"),
    ('"lites"',         '"burritos"',       False, "Light unit name"),
    ('"Litecoins"',     '"BurritoCoins"',   False, "Full unit name"),

    # ── DNS seeds (comment them out) ─────────────────────────
    # We'll handle these separately via the seed-clearing logic below.

    # ── Misc labels in docs / RPC help text ──────────────────
    ("liteoshi",        "burrioshi",        False, "Unit name unquoted"),
    ("photons",         "morsels",          False, "Unit name unquoted"),
    ("lites",           "burritos",         False, "Unit name unquoted"),
]

# ─────────────────────────────────────────────────────────────
# Files / directories to skip entirely
# ─────────────────────────────────────────────────────────────
SKIP_DIRS = {
    ".git", "depends", "build", "__pycache__",
    "leveldb", "secp256k1", "univalue", "crc32c",
}

SKIP_EXTENSIONS = {
    # Compiled / binary
    ".o", ".a", ".so", ".dll", ".dylib", ".exe", ".elf",
    # Images / assets
    ".png", ".ico", ".svg", ".gif", ".jpg", ".jpeg",
    # Archives
    ".zip", ".tar", ".gz", ".bz2", ".xz",
    # Python cache
    ".pyc",
}

# Don't modify this script itself
SKIP_FILES = {"rebrand.py"}

# ─────────────────────────────────────────────────────────────
# DNS seed pattern – comment out any vSeeds lines pointing to
# litecoin / bitcoin domains so the build still compiles.
# ─────────────────────────────────────────────────────────────
SEED_PATTERN = re.compile(
    r'([ \t]*vSeeds\.emplace_back\("[^"]*(?:litecoin|bitcoin|loshan|litepool)[^"]*"\);)',
    re.IGNORECASE,
)


def is_binary(path: Path) -> bool:
    """Return True if the file looks binary (has null bytes in first 8 KB)."""
    try:
        with open(path, "rb") as f:
            return b"\x00" in f.read(8192)
    except OSError:
        return True


def apply_replacements(text: str) -> tuple[str, list[str]]:
    """Apply all replacement rules; return (new_text, list_of_change_descriptions)."""
    changes = []
    for pattern, replacement, is_regex, description in REPLACEMENTS:
        if is_regex:
            new_text, count = re.subn(pattern, replacement, text)
        else:
            count = text.count(pattern)
            new_text = text.replace(pattern, replacement)
        if count:
            changes.append(f"  [{description}] {count}x  '{pattern}' → '{replacement}'")
            text = new_text

    # Comment out litecoin/bitcoin DNS seed lines
    def comment_seed(m):
        return "// BRTO-TODO: replace with BurritoCoin seeds\n// " + m.group(1)

    new_text, count = SEED_PATTERN.subn(comment_seed, text)
    if count:
        changes.append(f"  [DNS seeds] commented out {count} seed line(s)")
        text = new_text

    return text, changes


def process_file(path: Path, apply: bool) -> bool:
    """Process one file. Returns True if the file would be / was changed."""
    if is_binary(path):
        return False
    try:
        original = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        print(f"  SKIP (read error): {e}")
        return False

    modified, changes = apply_replacements(original)
    if modified == original:
        return False

    print(f"\n{'CHANGE' if apply else 'WOULD CHANGE'}: {path}")
    for c in changes:
        print(c)

    if apply:
        try:
            path.write_text(modified, encoding="utf-8")
        except OSError as e:
            print(f"  ERROR writing: {e}")
            return False

    return True


def walk_repo(root: Path, apply: bool):
    changed = 0
    skipped_dirs = 0
    total = 0

    for dirpath, dirnames, filenames in os.walk(root):
        # Prune skip-dirs in-place so os.walk won't descend
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]

        for fname in filenames:
            if fname in SKIP_FILES:
                continue
            ext = Path(fname).suffix.lower()
            if ext in SKIP_EXTENSIONS:
                continue

            fpath = Path(dirpath) / fname
            total += 1
            if process_file(fpath, apply):
                changed += 1

    print(f"\n{'─'*60}")
    print(f"Files scanned : {total}")
    print(f"Files {'changed' if apply else 'to change'}: {changed}")
    if not apply:
        print("\nRun with --apply to make changes.")


def main():
    parser = argparse.ArgumentParser(description="BurritoCoin rebranding script")
    parser.add_argument(
        "--apply", action="store_true",
        help="Actually write changes (default is dry-run)",
    )
    parser.add_argument(
        "--root", default=".",
        help="Repository root (default: current directory)",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    print(f"Repository root : {root}")
    print(f"Mode            : {'APPLY' if args.apply else 'DRY RUN'}")
    print(f"{'─'*60}")

    walk_repo(root, apply=args.apply)


if __name__ == "__main__":
    main()
