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

- **Project website** — <https://burritoco.in/>. Static site on **Cloudflare
  Pages**, rebuilt from `website/` on every push to `master`. There is no
  server behind it and nothing to deploy or restart. Includes the spec page
  (`/spec`), the integrator reference, download links, and the project FAQ.
- **Block explorer** — <https://explorer.burritoco.in/>. `btc-rpc-explorer`
  pointed at the production `burritocoind` over RPC, fronted by nginx.
- **ElectrumX server** — public Electrum-protocol server on the canonical
  Electrum ports, indexing the BurritoCoin chain from the local node so
  light wallets can connect without running a full node.
- **DNS seed** — `seed.burritoco.in` resolves to `129.146.160.229`. This is
  the bootstrap seed compiled into `chainparams.cpp`, so a fresh node with no
  known peers will reach out to it on startup. Because discovery goes through
  a DNS name rather than a hard-coded address, the seed host can be replaced
  without shipping a new wallet — which is what made the 2026-08 migration
  survivable for already-distributed binaries. There is still only one seed
  host; adding redundant seeds remains a deferred improvement.

There is currently one production full node (see "Infrastructure") and an
unknown but small number of community nodes. **No mining is running.** The
miner previously ran on the retired Linode; it has not been re-established
elsewhere, so the chain is not advancing until someone starts one. Difficulty
freezes while mining is stopped, so resuming does not face an inflated
target. No exchanges list BRTO yet; the listing path is described in the
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

Production is split across two providers as of 2026-08-18, neither of which
bills anything:

- **Static site** (`burritoco.in`, `www.burritoco.in`) — **Cloudflare Pages**,
  built from `website/` on every push to `master`. No server, nothing to
  deploy or restart. The `www` -> apex 301 is a zone-level Cloudflare
  Redirect Rule, not a file in the repo (Pages matches `_redirects` against
  paths only, so a hostname-sourced rule there silently never fires).
- **Node, ElectrumX, explorer** — one **Oracle Cloud A1** instance at
  **`129.146.160.229`** (us-phoenix-1 / AD-1, `VM.Standard.A1.Flex`,
  2 OCPU / 12 GB, Ubuntu 24.04 aarch64, 70 GB boot volume). Access is
  `ssh -i <key> ubuntu@129.146.160.229`; the key is in the project lead's
  password manager. The account is Pay As You Go, which exempts it from
  Oracle's 7-day idle-reclamation rule, with a $1 budget alert at a 1%
  threshold so any charge at all sends mail.
- **DNS** — Cloudflare. `seed` and `explorer` are DNS-only (grey cloud);
  `seed` must stay that way permanently, because P2P on 9227 is not HTTP and
  cannot traverse Cloudflare's proxy. The apex and `www` are proxied to Pages.

**Mining must never run on this infrastructure.** Oracle's Cloud Services
Agreement §1.3(d) prohibits crypto mining on every account type, and
enforcement is automated and disable-first — instances are disabled and
argued about afterwards. A validating/relaying node and a block explorer are
outside the ban's text; mining is not. Mining belongs on hardware the project
owns. See `doc/oracle-migration.md`.

The Linode at `50.116.17.170` is being retired; every service moved off it on
2026-08-18. Once the instance is cancelled that IP is reassigned to an
unrelated customer, so any surviving reference to it should be deleted rather
than trusted.

The following systemd units are expected to be `active` on the Oracle box.
Verify each with `systemctl is-active <name>`:

- **`burritocoind.service`** — the full node, running as `ubuntu` out of
  `/home/ubuntu/.burritocoin/`. Source of truth for the explorer and
  ElectrumX. Runs with `disablewallet=1`, so no wallet is loaded and none can
  be created: **this box holds no keys.** (The build cannot use
  `--disable-wallet` — `libmw/src/wallet/Keychain.cpp` pulls in Berkeley DB
  headers unconditionally — so the wallet is compiled in and switched off at
  runtime, which is the stronger guarantee anyway.)
- **`electrumx.service`** — ElectrumX 1.19.0, pinned to upstream `24865dc3`.
  Serves TCP 50001 publicly, admin RPC on 127.0.0.1:8000. Config at
  `/etc/electrumx.conf`, mode 600 — it contains the node RPC password.
- **`btc-rpc-explorer.service`** — the explorer, bound to **127.0.0.1:3002**
  with nginx in front. Config at `/opt/btc-rpc-explorer/.env`, mode 600.
  `BTCEXP_SECURE_SITE=true` is required behind the proxy; without it Express
  never sets `trust proxy`, every request appears to come from nginx, and the
  200-request/15-minute rate limiter applies to all visitors collectively.
- **`nginx`** — terminates TLS for `explorer.burritoco.in` and proxies to
  3002. Let's Encrypt certificate, renewed automatically by `certbot.timer`.

There is deliberately **no second/loopback daemon**. It existed on the Linode
only so `getblocktemplate` would see a non-zero peer count, since the daemon
refuses to serve mining templates when it believes it is disconnected. With
no mining here it has no purpose.

**Key paths on the Oracle box:**

- `/home/ubuntu/BurritoCoin/` — the checkout the binaries were built from.
- `/home/ubuntu/.burritocoin/` — node data directory and `burritocoin.conf`
  (mode 600). RPC auth uses `rpcauth=`, which stores only a salted hash, so
  the plaintext password cannot be recovered from the config; it is stashed
  at `/home/ubuntu/.burritocoin/rpcpass.txt` for ElectrumX and the explorer.
- `/opt/electrumx/` — ElectrumX checkout (pinned) and its venv.
- `/opt/btc-rpc-explorer/` — explorer checkout, pinned to `26e282a` with
  `contrib/explorer/burritocoin-explorer.patch` applied.
- `/usr/local/bin/burritocoind`, `/usr/local/bin/burritocoin-cli`.

Ports: **9227** (P2P) and **50001** (ElectrumX) are open to the internet;
**80/443** serve the explorer; **9226** (node RPC), **8000** (ElectrumX admin)
and **3002** (explorer) are loopback-only and must stay closed. Both the VCN
security list *and* the in-image iptables rules have to allow a port — opening
only the security list is the most common reason a port appears dead on OCI.

---

## 4. Wallets

**Neither wallet lives on production infrastructure any more.** The Oracle
node runs with `disablewallet=1`, so it loads no wallet and cannot create
one. What follows describes the wallets themselves and where they now live.

- **`mainwallet`** — holds the **148,000,000 BRTO** genesis premine. The
  premine output is a P2PK locked to a key that was generated at chain
  bringup; it became spendable 100 blocks after genesis. The wallet is
  AES-256 encrypted (the standard `walletpassphrase` flow). Since the
  Linode was retired the **only** copy is the `wallet.dat` backed up to
  OneDrive. That removes the old risk — 148M BRTO sitting on a
  public-facing server — and replaces it with a different one: a single
  backup location. The encryption means a OneDrive compromise alone does
  not spend the coins, but a OneDrive *loss* is unrecoverable. A second
  offline copy is the outstanding action; see section 7.
- **`vps-mining`** — held the coinbase outputs mined on the Linode
  (roughly 1,500 BRTO), paid to
  `brto1q675hvplaa9udwt8uplvfv4cndt8z9x87sk324w`. **This wallet was
  deliberately abandoned** with the Linode rather than migrated: the balance
  was judged not worth the handling, and the coins are unrecoverable. The
  address remains valid and its history is still visible on the explorer;
  nothing can spend from it.

The **passphrases** for both wallets live in the project lead's password
manager. The node's **RPC credentials** are stored as an `rpcauth=` salted
hash in `/home/ubuntu/.burritocoin/burritocoin.conf` (mode 600) — the
plaintext is not recoverable from that file and is kept alongside it in
`rpcpass.txt`, which ElectrumX and the explorer were configured from.
The **Oracle SSH private key** lives only in the project lead's password
manager; there is no password login on that box. None
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

1. **RESOLVED, with a successor risk — premine custody.** The premine is
   no longer on a public-facing server: every service moved to hosts that
   hold no wallet, and the Linode that held `mainwallet` was retired
   (2026-08-18). What remains is a **single-copy backup problem** — the
   encrypted `wallet.dat` exists only in OneDrive. The remaining work is a
   second copy on offline media kept somewhere physically separate, and a
   restore test (open the backup in a fresh Qt wallet and confirm the
   balance) so the backup is known-good rather than assumed-good. An
   untested backup is not a backup.
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
4. **MEDIUM — add security headers.** Neither surface sets
   `Strict-Transport-Security`, `X-Frame-Options`, `X-Content-Type-Options`,
   `Referrer-Policy`, or a basic `Content-Security-Policy`. These now live in
   two different places: for `explorer.burritoco.in`, the nginx server block
   on the Oracle box; for `burritoco.in`, a `website/_headers` file, since
   Cloudflare Pages has no nginx to configure.
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

Common invocations a maintainer needs day-to-day. Run these on the Oracle
box (`ssh -i <key> ubuntu@129.146.160.229`) unless otherwise noted.

**Node interaction** — `~/.burritocoin` is the default datadir for the
`ubuntu` user, so no `-conf`/`-datadir` flags are needed:

```
burritocoin-cli getblockcount
burritocoin-cli getblockchaininfo
burritocoin-cli getpeerinfo
burritocoin-cli getmininginfo
# no getbalance: the node runs disablewallet=1 and holds no wallet
```

**Service control:**

```
systemctl status burritocoind.service
systemctl restart burritocoind.service
systemctl status btc-rpc-explorer.service
systemctl status electrumx.service
systemctl reload nginx
journalctl -u burritocoind.service -n 200 --no-pager
```

**Repo maintenance:**

```
git pull --ff-only origin master
./contrib/devtools/install-hooks.sh    # install the post-commit hook
./contrib/devtools/update-changelog.sh # regenerate CHANGELOG.md from git log
```

**Website deploy** — there is no deploy step. Cloudflare Pages rebuilds
`burritoco.in` from `website/` on every push to `master`, normally within a
minute:

```
git push origin master        # that is the whole deploy
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
