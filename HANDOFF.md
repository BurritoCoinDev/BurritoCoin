# BurritoCoin Project — Handoff Document

**Last updated:** 2026-05-08
**Master tip when written:** `4142687a5`

This file documents only **public** information about the BurritoCoin project:
network parameters, repository state, pending work, and operational notes that
would already be visible to anyone who reads the source, runs a node, or visits
the website. It contains no secrets — no wallet passphrases, no RPC
credentials, no private keys, no SSH keys, and no paths to backup files. The
locations of those secrets are noted, but the secrets themselves live only in
the project lead's password manager and on a mode-`600` config file on the
production VPS.

The intent is that any contributor (or any future Claude Code session)
landing fresh on this repository can read `HANDOFF.md` plus `CHANGELOG.md` and
reconstruct enough context to be useful immediately, without needing to
interview anyone or trawl through chat history. If you find this file out of
date, fix it — see the "How to use this document" section at the bottom.

---

## 1. Project status

BurritoCoin's mainnet has been live since **2026-04-11**. As of the timestamp
on this file the chain tip is at block height **385** (queried locally with
`burritocoin-cli getblockcount` against the production node — this number
moves forward roughly every 2.5 minutes, so by the time you read this it will
be larger; the value is a sanity-check anchor, not a fixed reference). Block
production has been continuous since launch with no known reorgs deeper than
a single block and no consensus incidents.

The public-facing surface is:

- **Project website** — <https://burritoco.in/>. Static site served from
  `/var/www/burritoco.in/` on the production VPS via nginx with TLS from
  Let's Encrypt. Includes the spec page (`/spec`), the integrator reference,
  download links, and the project FAQ.
- **Block explorer** — <https://explorer.burritoco.in/>. `btc-rpc-explorer`
  pointed at the production `burritocoind` over RPC, fronted by nginx.
- **ElectrumX server** — public Electrum-protocol server on the canonical
  Electrum ports, indexing the BurritoCoin chain from the local node so
  light wallets can connect without running a full node.
- **DNS seed** — `seed.burritoco.in` resolves to `50.116.17.170` (the VPS).
  This is the bootstrap seed compiled into `chainparams.cpp`, so a fresh
  node with no known peers will reach out to it on startup. There is only
  one seed host today; adding redundant seeds is a deferred improvement
  (see the pending-tasks section).

There is currently one production full node and one peer node (both on the
same VPS — see "Infrastructure"), one mining process throttled to roughly
30% of one CPU core, and an unknown but small number of community nodes. No
exchanges list BRTO yet; the listing path is described in the
"Distribution path" section below.

---

## 2. Network identity (public table)

These parameters are baked into `src/chainparams.cpp` and the genesis block,
and they are the canonical reference for anyone integrating against
BurritoCoin (wallets, explorers, exchanges, miners). They are immutable for
the live mainnet.

| Field | Value |
|---|---|
| Coin name | BurritoCoin |
| Ticker | BRTO |
| Genesis block hash | `0x44615751d966cf772a051f65b8df4f3987adc48be1749a699369a18517418dce` |
| Mainnet P2P port | 9227 |
| Mainnet RPC port | 9226 |
| Testnet P2P port | 19227 |
| Testnet RPC port | 19226 |
| Network magic bytes | `0x42 0x52 0x54 0x4f` (ASCII "BRTO") |
| Proof-of-work algorithm | Scrypt (N=1024, r=1, p=1) |
| Target block time | 2.5 minutes |
| Difficulty retarget | every 2,016 blocks |
| Block reward (current era) | 10 BRTO |
| Halving interval | 1,042,600,000 blocks (~4,960 years per halving) |
| Maximum supply | 21,000,000,000 BRTO (21 billion) |
| Genesis premine | 148,000,000 BRTO, P2PK output, spendable after 100 confirmations |
| BIP34 / BIP65 / BIP66 / CSV / SegWit activation | height 1 (mainnet and testnet); genesis itself is exempt |
| MWEB | supported, activated via BIP8 |

The very long halving interval (almost five millennia) combined with the 21B
supply ceiling is the deliberate design choice that distinguishes BurritoCoin
from a straight Litecoin clone: emission is effectively flat over any
human-relevant time horizon. The genesis premine exists so that there is a
known, attributable allocation for early development, listings, marketing,
and grants — its custody is described in section 4.

The four Bitcoin/Litecoin-derived soft forks (BIP34, 65, 66, CSV) plus
SegWit activate at height 1 rather than at some later block, which means
the chain has been "fully modern" since the very first non-genesis block.
Genesis itself is exempt because BIP34 by construction can't apply to a
block that has no predecessor. MWEB (Mimblewimble Extension Blocks)
inherits the Litecoin design and activates via BIP8 signaling; pre-
activation enforcement in `src/rpc/mining.cpp` is one of the medium-
priority pending decisions (see section 7).

---

## 3. Infrastructure

All production services run on a single Linode VPS at **`50.116.17.170`**,
running Ubuntu 24.04 LTS. This is a single point of failure today; the
mitigation plan (off-VPS premine custody, off-site wallet backup, and
eventually a redundant DNS seed and a second relay node) is tracked in
section 7.

The following systemd units are expected to be `active`. You can verify
each with `systemctl is-active <name>`:

- **`burritocoind.service`** — the production full node. Data directory
  `/root/.burritocoin/`. Listens on the mainnet P2P/RPC ports above.
  Source of truth for the explorer, ElectrumX, and the miner.
- **`burritocoind-peer.service`** — a second `burritocoind` instance
  running out of `/root/.burritocoin-peer/`, peered to the main node so
  the network has at least two full nodes even from a single host. Useful
  for testing relay/propagation locally.
- **`btc-rpc-explorer.service`** — the block explorer at
  `explorer.burritoco.in`. Talks to `burritocoind.service` over RPC.
- **`electrumx.service`** — the public ElectrumX server. Indexes the
  chain from the local node and serves the Electrum protocol to light
  wallets.
- **`burritocoin-miner.service`** — the throttled CPU miner. Uses
  `cpulimit` (or equivalent) to cap usage at roughly 30% of a single
  core so that the node, explorer, and ElectrumX stay responsive on the
  same VPS. Mines to the `vps-mining` wallet (see section 4).
- **`nginx`** — fronts `burritoco.in` and `explorer.burritoco.in` and
  terminates TLS.

**Key paths on the VPS:**

- `/root/BurritoCoin/` — the source repository (this checkout).
- `/root/.burritocoin/` — main node data directory, including
  `wallet.dat` for the **mainwallet** and `burritocoin.conf` (mode 600,
  contains the RPC user/password — not in this file).
- `/root/.burritocoin-peer/` — peer node data directory.
- `/var/www/burritoco.in/` — static site root served by nginx. Updated
  by `git pull` from a content branch and an nginx reload.
- `/usr/local/bin/brto-miner.sh` — wrapper script invoked by
  `burritocoin-miner.service`; sets the throttle and the mining address.

---

## 4. Wallets

There are two production wallets. The on-VPS files are at
`/root/.burritocoin/wallet.dat` and the corresponding peer-node directory.

- **`mainwallet`** — holds the **148,000,000 BRTO** genesis premine. The
  premine output is a P2PK locked to a key that was generated at chain
  bringup; it became spendable 100 blocks after genesis. The wallet is
  AES-256 encrypted (the standard `walletpassphrase` flow). The
  `wallet.dat` file is currently backed up to OneDrive cloud storage.
  Keeping this much value on a public-facing VPS is the single biggest
  operational risk in the project today and is the top item on the
  pending-tasks list — see section 7.
- **`vps-mining`** — receives the coinbase outputs from
  `burritocoin-miner.service`. Mining pays out to
  `brto1q675hvplaa9udwt8uplvfv4cndt8z9x87sk324w` (a native SegWit / bech32
  address with the BurritoCoin `brto1` HRP). This wallet is also
  AES-256 encrypted.

The **passphrases** for both wallets live in the project lead's password
manager. The **RPC user and password** for the running node live only in
`/root/.burritocoin/burritocoin.conf` (mode 600, root-readable only).
The **VPS SSH credentials** also live only in the password manager. None
of these secrets appears in this file, in the repo, or in `CHANGELOG.md`.
If you are a future contributor and you need access to any of them, you
need to be the project lead or be vouched for by the project lead — there
is no alternative recovery path on purpose.

---

## 5. Recent work

For the full commit-by-commit history with verbatim commit-message bodies,
read **`CHANGELOG.md`** in this directory. It is auto-generated from
`git log` by `contrib/devtools/update-changelog.sh` and is regenerated by
the post-commit hook installed via `contrib/devtools/install-hooks.sh`, so
it is always in sync with the live tip.

What follows is a human-readable summary of the last ~15 commits at the
time of writing (newest first); reach for `CHANGELOG.md` if you need the
exact diff message for any of them.

The most visible recent thread of work has been a **multi-round audit-and-
cleanup pass** preparing the repo for first public release. The audit
caught several classes of issue that needed fixing before binaries shipped
to anyone outside the project:

- **License attribution restoration** (`c0669a9`). The original rebrand
  replaced "The Bitcoin Core developers" and "The Litecoin Core developers"
  with "The BurritoCoin Core developers" in the per-file copyright headers
  of **1,051 source files**. This both violates the MIT license's
  requirement to preserve original copyright notices and is straightforward
  misattribution: files authored by Bitcoin Core developers between
  2009-2020 cannot be attributed to a project that didn't exist before
  2026. The fix layered attribution correctly — Bitcoin Core devs for
  ~1,018 files with their original year ranges preserved, Litecoin Core
  devs for the 33 `src/libmw/` files, and BurritoCoin Core developers only
  for genuinely new BurritoCoin contributions.

- **`SECURITY.md` fix** (`a3bc99e`). The misleading security/release-
  signing identity in `SECURITY.md` was corrected so that vulnerability
  reporters reach the actual maintainers, and the documented release-
  signing fingerprint matches the key that is actually used.

- **URL canonicalization** (`87d5e22`, `3af5f1e`, `ae1c9320b`,
  `9af07643c`, `a455ae9`). Across more than 100 files in `doc/`, the
  repo root, the `.github/` directory, CI configs, `configure.ac`, and
  the rest of `src/`, stale URLs (pointing at `litecoin.org`,
  Bitcoin Core repos, or the wrong domain for BurritoCoin) were
  replaced with the canonical `burritoco.in` URLs and the canonical
  GitHub URL.

- **Bundle ID canonicalization** (`f23eb96`). The macOS/Linux reverse-
  DNS bundle identifier was changed from `org.burritocoin.*` to
  `in.burritoco.*`. The convention requires you to own the domain whose
  reverse you're using, and the project owns `burritoco.in`, not
  `burritocoin.org`. This matters for Apple notarization, which
  verifies the bundle ID against domain ownership and would reject a
  submission claiming a domain the developer doesn't control. Affected
  files include `src/qt/macnotificationhandler.mm`, `share/qt/Info.plist.in`,
  the launchd plist (renamed on disk), `contrib/init/README.md`,
  `doc/init.md`, `doc/Doxyfile.in`, and `doc/release-process.md`.

- **Doc restructure** (`a455ae9`). Historical Litecoin/Bitcoin release
  notes were moved out of the active `doc/release-notes.md` and archived
  under `doc/historical-release-notes/` so they are preserved for license
  reasons but don't confuse contributors looking for current BurritoCoin
  release notes.

- **Website accessibility, SEO, and quality** (`19817d58b`, `62074dc26`,
  `810a26e2c`, `34cdf22f2`, `3fc6c9f70`). The site got skip-to-content
  links, visible focus styles, a `robots.txt`, a `sitemap.xml`, canonical
  URL `<link>` tags, missing-alt-text fixes, mobile-layout repairs, dead
  download-link fixes (the Windows download was broken), TODO
  placeholders cleared, and a `/spec` integrator reference page wired
  into the nav.

- **Test framework rename** (`4844e21`). The `test/functional/` helper
  modules and importers used `ltc_*` prefixes inherited from the
  Litecoin codebase. Renamed the helper modules and updated every
  importer to `brto_*` so the test framework speaks BurritoCoin
  vocabulary throughout.

- **Image-file mode fix** (`4ec1a69`). Several image assets (`.png`,
  `.ico`, `.icns`, `.bmp`) under `share/` and `src/qt/res/` were tracked
  with mode `755` (executable). Dropped to `644` for all of them.

- **Misattributed-identity leaks** (`7f45b27`). A second-round audit
  caught remaining places where Bitcoin Core developers were credited
  as BurritoCoin developers — corrected.

- **`COPYRIGHT_YEAR` bump and translation `satoshi → burrioshi` sync**
  (`4142687`, current tip). `build_msvc/burritocoin_config.h:37` had
  `COPYRIGHT_YEAR=2024` while `configure.ac` was already on 2026, so
  Windows binaries shipped with the wrong year in `--license` output —
  bumped to 2026. Separately, 51 translation `.ts` files contained 168
  stale "satoshi" references in both `<source>` and `<translation>`
  elements; a bulk `\b`-bounded regex replace mapped
  `satoshi`/`satoshis`/`satoshi(s)` to `burrioshi`/`burrioshi`/
  `burrioshi(s)` while preserving "Satoshi Nakamoto" the proper noun.

A handful of earlier infrastructure commits — the spec page rebuild
(`3fc6c9f7`), the soft-fork-height regtest revert (`f1956550`), and the
`release/` `.gitignore` addition (`1a538d08`) — round out the recent
window.

---

## 6. Distribution path

Listing strategy is layered by realism. There is no point pursuing a
high-tier listing today; the chain has 385 blocks, no audited binaries,
and no liquidity, so any major exchange would (correctly) decline.

- **Tier 1 — first listings.** The realistic first listings are
  smaller, lower-friction exchanges that accept new chains based on a
  technical-spec submission and a small listing fee: notably
  **TradeOgre** and **XeggeX**. Prerequisites before approaching either:
  signed release binaries built via `depends/` (so the listing team can
  reproduce them), the integrator spec page on the website (already
  live at `burritoco.in/spec`), and a working ElectrumX server (already
  live). Once those are ready this is unblocking.
- **Tier 2 — mid-CEX.** Mid-tier centralized exchanges
  (XT, MEXC, BitMart and similar) require demonstrable trading volume,
  some KYC on the project side, and usually a registered legal entity.
  Premature application is wasted effort; revisit after Tier 1 has
  been live for several months and there is genuine BRTO turnover.
- **Tier 3 — major CEX.** Binance, Coinbase, Kraken, etc. Effectively
  out of reach without organic volume measured in millions of dollars
  per day, audited reserves, and a real corporate counterparty. Not a
  near-term goal; flagged here so nobody wastes time on it.
- **DEX bridge.** Wrapping BRTO onto Ethereum or another EVM chain via
  a bridge would unlock DEX trading (Uniswap-style). This should be
  deferred until well after Tier 1 — the engineering and security risk
  of running a bridge is meaningful, and a bridge with no underlying
  spot-market price discovery is just a faucet for arbitrageurs.

---

## 7. Pending operational tasks

In rough priority order. The single critical item is the premine custody
issue; everything else can wait on it.

1. **CRITICAL — move the 148M premine off the VPS.** The premine
   currently lives in the `mainwallet` `wallet.dat` on a public-facing
   single-host server. The fix is to install a fresh Qt wallet on a
   personal machine (ideally air-gapped or at least not Internet-
   exposed), generate a receive address there, and send the entire
   premine to it from the VPS in a single sweep transaction. Then
   re-encrypt and back up the new wallet (USB cold backup, see item 6).
   This is the highest-value single operational fix the project can
   make.
2. **HIGH — build official release binaries via `depends/`.** The
   reproducible-build system under `depends/` is the standard mechanism
   for producing Linux, macOS, and Windows binaries. Until binaries
   exist there is nothing to publish on the website's downloads page
   beyond source, and there is nothing for Tier 1 exchanges to
   integrate against. This is gating the listing path.
3. **HIGH — build the missing `burritocoin_scrypt` Python C-extension.**
   A subset of the functional test suite under `test/functional/` needs
   the Scrypt PoW callable from Python via a small C-extension. The
   extension hasn't been built yet, so those tests are currently
   skipped. Build it, wire it into the test runner, and turn the skips
   into real assertions.
4. **MEDIUM — add nginx security headers.** The site and explorer are
   served without `Strict-Transport-Security`, `X-Frame-Options`,
   `X-Content-Type-Options`, `Referrer-Policy`, or a basic
   `Content-Security-Policy`. Add them in the nginx server blocks for
   `burritoco.in` and `explorer.burritoco.in`.
5. **MEDIUM — decide on MWEB pre-activation enforcement in
   `src/rpc/mining.cpp`.** The MWEB code path uses BIP8 signaling, and
   there is an open question about how strictly the mining RPCs should
   refuse to produce MWEB-flavored templates before activation is
   final. Resolve and document the decision.
6. **LOW — USB cold backup of `wallet.dat`.** Independent of item 1,
   take an offline backup of the production `wallet.dat` to a USB
   drive kept in physical storage. OneDrive is fine as a hot backup
   but is a single-vendor dependency.
7. **LOW — three inflected `satoshi` stragglers in Finnish/Slovenian
   `.ts` files.** Inflected forms (`satoshia`, `satoshin`,
   `satoshijev`, `satošijev`) didn't match the `\b`-bounded regex used
   in commit `4142687` and need a native speaker to retranslate
   properly to the corresponding inflected forms of `burrioshi`.
8. **LOW — six `BRTO-TODO` markers in `src/chainparams.cpp`.** These
   are minor parameter-comment cleanups left behind by the rebrand.
   Walk through them and decide for each whether to clarify or
   delete.

---

## 8. Quick reference command list

Common invocations a maintainer needs day-to-day. Run these on the VPS
unless otherwise noted.

**Node interaction (`bcli` is the local alias for `burritocoin-cli` with
the right `-conf` and `-datadir`):**

```
src/burritocoin-cli -conf=/root/.burritocoin/burritocoin.conf -datadir=/root/.burritocoin getblockcount
src/burritocoin-cli -conf=/root/.burritocoin/burritocoin.conf -datadir=/root/.burritocoin getblockchaininfo
src/burritocoin-cli -conf=/root/.burritocoin/burritocoin.conf -datadir=/root/.burritocoin getpeerinfo
src/burritocoin-cli -conf=/root/.burritocoin/burritocoin.conf -datadir=/root/.burritocoin getmininginfo
src/burritocoin-cli -conf=/root/.burritocoin/burritocoin.conf -datadir=/root/.burritocoin getbalance
```

**Service control:**

```
systemctl status burritocoind.service
systemctl restart burritocoind.service
systemctl status btc-rpc-explorer.service
systemctl status electrumx.service
systemctl status burritocoin-miner.service
systemctl reload nginx
journalctl -u burritocoind.service -n 200 --no-pager
```

**Repo maintenance:**

```
git pull --ff-only origin master
./contrib/devtools/install-hooks.sh    # install the post-commit hook
./contrib/devtools/update-changelog.sh # regenerate CHANGELOG.md from git log
```

**Website deploy** (the website tree under `/var/www/burritoco.in/`):

```
cd /var/www/burritoco.in && git pull --ff-only && systemctl reload nginx
```

---

## 9. Related docs in this repo

The following files are the supporting documents a contributor will need;
read them alongside this handoff.

- `CHANGELOG.md` — full commit-by-commit history, auto-generated from
  `git log`, regenerated by the post-commit hook.
- `README.md` — top-level repository overview.
- `CONTRIBUTING.md` — contributor guidelines.
- `SECURITY.md` — vulnerability-reporting policy and signing fingerprint.
- `COPYING` — the MIT license text covering the project.
- `doc/burritocoin-conf.md` — reference for `burritocoin.conf`.
- `doc/build-*.md` — per-platform build instructions
  (`build-unix.md`, `build-osx.md`, `build-windows.md`, etc.).
- `doc/release-process.md` — the release-engineering procedure,
  including `depends/` and notarization.
- `doc/bips.md` — list of BIPs supported and their activation status.
- `website/spec.html` — the public integrator-facing spec page (also
  served at `burritoco.in/spec`).
- `contrib/vps/` — VPS provisioning and ElectrumX setup scripts.
- `contrib/init/` — systemd, OpenRC, launchd, and Upstart unit files.
- `contrib/devtools/update-changelog.sh` — regenerates `CHANGELOG.md`.
- `contrib/devtools/install-hooks.sh` — installs the post-commit hook
  that re-runs `update-changelog.sh` after every commit.

---

## 10. How to use this document

If you are a **new contributor** picking up this project for the first
time, read this file top to bottom, then read `CHANGELOG.md` for the
last ten commits or so to get the feel of recent work, then read
`README.md` and the relevant `doc/build-*.md` for your platform. By
the time you finish those four documents you should know what
BurritoCoin is, what the network parameters are, what's running where,
who holds what, what's most urgent to fix, and how to build and run
the code. If you don't, this file has a gap — please fix it.

If you are **updating this document**, the rules are:

- Keep it factual and public. No secrets, ever.
- When you change something operationally important (a service moves,
  a wallet is rotated, the premine is moved off the VPS, a Tier 1
  exchange listing goes live), update the affected section in this
  file in the same commit as the operational change. Stale handoff
  docs are worse than no handoff doc.
- Bump the **Last updated** date at the top, and update the **Master
  tip when written** field with the output of `git rev-parse --short
  HEAD` *as of the commit that introduces your change*. The tip
  reference makes it possible to ask "was this file accurate at any
  point?" by checking the file out at that revision.
- If you find a section that has drifted from reality (a service is no
  longer active, a path has moved, the block height anchor is silly-
  old), fix it; don't leave a TODO unless you genuinely cannot fix it
  yourself.
- Pending tasks (section 7) should be re-prioritized as the world
  changes. When you complete one, remove it from the list rather
  than crossing it off — the commit history records that you did it.

The companion file `CHANGELOG.md` is auto-generated; do not edit it by
hand. Run `./contrib/devtools/update-changelog.sh` to regenerate it,
or rely on the post-commit hook installed by
`./contrib/devtools/install-hooks.sh`.
