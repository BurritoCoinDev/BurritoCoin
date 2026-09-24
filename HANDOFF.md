# BurritoCoin Project — Handoff Document

**Last updated:** 2026-09-23
**Master tip when written:** `b5bb0e0`

This file documents only **public** information about the BurritoCoin project:
network parameters, repository state, pending work, and operational notes that
would already be visible to anyone who reads the source, runs a node, or visits
the website. It contains no secrets — no wallet passphrases, no RPC
credentials, no private keys, no SSH keys, and no paths to backup files. The
locations of those secrets are noted, but the secrets themselves live with
the project lead — their password manager, their own machine, and the
encrypted OneDrive wallet backup (§3, §4) — and in mode-`600` files on the
production server.

The intent is that any contributor (or any future Claude Code session)
landing fresh on this repository can read `HANDOFF.md` plus `CHANGELOG.md` and
reconstruct enough context to be useful immediately, without needing to
interview anyone or trawl through chat history. If you find this file out of
date, fix it — see the "How to use this document" section at the bottom.

---

## 1. Project status

BurritoCoin's mainnet has been live since **2026-04-11**. On 2026-09-22 the
chain tip was at block height **10,578** (`burritocoin-cli getblockcount` on
the production node), and it is **not moving: nobody is mining.** Mining
stopped when the Linode was retired, so the chain hasn't advanced since
mid-August 2026, and difficulty stays frozen until someone mines again. Until
then every node, the seed included, reports `initialblockdownload: true`,
because its tip is more than 24 hours old. That is expected, and it clears
across the network as soon as anyone finds a new block. There have been no
known reorgs deeper than a single block and no consensus incidents.

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
| MWEB | supported; height-based BIP8 on bit 4 with forced lock-in: locks in at block 16,128, **active from block 24,192** (`started` at 10,578) |

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
  key-only SSH, `ssh -i <key> ubuntu@129.146.160.229` — see **Access and
  recovery** below. The account is Pay As You Go, which exempts it from
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

The Linode at `50.116.17.170` is **gone** — every service moved off it on
2026-08-18 and the instance was then deleted. That IP is now reassigned to an
unrelated Linode customer, so any surviving reference to it must be deleted
rather than trusted.

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

**The seed must run with `maxtipage=315360000`** (see
`contrib/oracle/burritocoin.conf.example`) — not yet applied to the live box as
of 2026-09-24; that's §7 item 3. Without it, once nobody has mined
for 24 hours the seed considers itself still syncing, and a node in that state
ignores `getheaders` from peers — so it stops handing the chain to new nodes and
every fresh wallet sits at block 0. That is exactly what happened after mining
stopped in August 2026, and it was reproduced with two regtest nodes on
2026-09-24. Don't remove the line while the chain can go quiet.

After editing any unit file, run `sudo systemctl daemon-reload` before
restarting it. On 2026-09-22 `burritocoind.service` turned out to have been
edited on disk without a reload, so systemd was still running an older
definition — the next `restart` prints a "changed on disk" warning; heed it.
After reloading, its `User`, `ExecStart`, `ExecStop`, `Restart`,
`RestartSec` and `TimeoutStopSec` were checked against
`contrib/oracle/burritocoind.service` and match. `TimeoutStopSec=600` is the
one that matters: it keeps systemd from killing the daemon in the middle of
a slow shutdown flush.

**Access and recovery.** SSH is key-only; there is no password login. The
working key is an ed25519 pair with the comment `burritocoin-oracle`,
created 2026-09-22 because the original key's private half couldn't be found
on the project lead's machine. The original's *public* half was still in
`~/.ssh/authorized_keys` alongside the new one, which leaves an authorized key
whose whereabouts nobody can vouch for — resolving that is §7 item 2. The new
private key is held by the project lead
and belongs in the password manager too — a key that exists on one machine
only is one lost laptop away from locking everyone out.

With no working key at all, get in through the Oracle Cloud console:

1. Instance → **Management** → Oracle Cloud Agent: the **Bastion** plugin
   must be Enabled and Running (it is, as of 2026-09-22).
2. Identity & Security → **Bastion** → `burritocoinbastion` (target subnet
   `public subnet-burritocoin-vcn`) → **Sessions** → **Create session**:
   type *Managed SSH session*, username `ubuntu`, instance `burritocoin`,
   target IP `10.0.0.238` (the instance's private address), and paste a
   fresh public key.
3. When the session is Active, its menu offers **Copy SSH command**, which
   tunnels through the bastion with a `ProxyCommand`. On Windows, keep the
   private key at a path without spaces so the nested quoting survives
   PowerShell.
4. Append the new public key to `~/.ssh/authorized_keys`. The session's own
   copy disappears when the session expires (3 hours at most), so skipping
   this step means repeating all of the above next time.
5. Remove every key whose private half you can't account for: delete its
   line from `~/.ssh/authorized_keys` (the trailing comment on each line
   identifies it). A lost key that still works is a standing risk, and on
   this box `ubuntu` has passwordless sudo.

The bastion costs nothing to keep and is the break-glass path, so leave it
in place. The corollary deserves stating plainly: **anyone who controls the
Oracle Cloud account can get a shell on this box**, so that login is part of
the box's security.

**Don't use Run Command.** The instance's Management tab has a Run command
panel, but the Oracle Cloud Agent on this instance offers no Run Command
plugin (it is absent from the plugin list). The console accepts a command
and shows delivery *Visible*, execution *Accepted* — and nothing ever runs.
Established the slow way on 2026-09-22; go straight to the bastion.

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
The **Oracle SSH private key** (`burritocoin-oracle`, see §3) is held by the
project lead; there is no password login on that box. None
of these secrets appears in this file, in the repo, or in `CHANGELOG.md`.
If you are a future contributor and you need access to any of them, you
need to be the project lead or be vouched for by the project lead. No
recovery path bypasses the project lead, on purpose: a lost SSH key is
recoverable only through the project lead's Oracle Cloud account (§3), and a
lost wallet passphrase is not recoverable at all.

---

## 5. Recent work

For the full commit-by-commit history with verbatim commit-message bodies,
read **`CHANGELOG.md`** in this directory. It is auto-generated from
`git log` by `contrib/devtools/update-changelog.sh`. The post-commit hook
from `contrib/devtools/install-hooks.sh` regenerates it automatically — but
only in clones where someone ran that script, and Claude Code sessions
don't, so it can lag the tip (it once went two months stale). Regenerate it
before relying on it, but only in a **full** clone. In a shallow one
(`git rev-parse --is-shallow-repository` prints `true`; Claude Code sessions
usually are) `git log` stops at the shallow boundary, so the script refuses
to run rather than silently dropping most entries — `git fetch --unshallow`
first. Its **Archived history (pre-rewrite)** section holds
150 entries for commits no longer reachable from `master`, 64 of which exist
nowhere else; the generator carries that section through untouched.

**History rewrite, 2026-08-28.** `master` was rewritten with `git filter-repo`
to drop 44 superseded copies of `contrib/release/burritocoin-qt-win64.exe`,
which renumbered every commit. A clone made before then can neither
fast-forward nor safely push: re-clone. The commit hashes quoted in this file
were remapped to their post-rewrite values on 2026-09-23; a pre-rewrite hash
found anywhere else can be resolved by searching `CHANGELOG.md` for the
commit's subject.

Work since 2026-08 — the move off Linode and the rebuilt Windows wallet with
its download buttons — is covered in §3, §7 and `doc/oracle-migration.md`
rather than summarised here. The summary below predates it.

What follows is a human-readable summary of the last ~15 commits at the
time of writing (newest first); reach for `CHANGELOG.md` if you need the
exact diff message for any of them.

The most visible recent thread of work has been a **multi-round audit-and-
cleanup pass** preparing the repo for first public release. The audit
caught several classes of issue that needed fixing before binaries shipped
to anyone outside the project:

- **License attribution restoration** (`ebdd080`). The original rebrand
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

- **`SECURITY.md` fix** (`3957ae9`). The misleading security/release-
  signing identity in `SECURITY.md` was corrected so that vulnerability
  reporters reach the actual maintainers, and the documented release-
  signing fingerprint matches the key that is actually used.

- **URL canonicalization** (`c64347b`, `ce87ed1`, `95cb8c943`,
  `f227758d6`, `574981f`). Across more than 100 files in `doc/`, the
  repo root, the `.github/` directory, CI configs, `configure.ac`, and
  the rest of `src/`, stale URLs (pointing at `litecoin.org`,
  Bitcoin Core repos, or the wrong domain for BurritoCoin) were
  replaced with the canonical `burritoco.in` URLs and the canonical
  GitHub URL.

- **Bundle ID canonicalization** (`f2837d3`). The macOS/Linux reverse-
  DNS bundle identifier was changed from `org.burritocoin.*` to
  `in.burritoco.*`. The convention requires you to own the domain whose
  reverse you're using, and the project owns `burritoco.in`, not
  `burritocoin.org`. This matters for Apple notarization, which
  verifies the bundle ID against domain ownership and would reject a
  submission claiming a domain the developer doesn't control. Affected
  files include `src/qt/macnotificationhandler.mm`, `share/qt/Info.plist.in`,
  the launchd plist (renamed on disk), `contrib/init/README.md`,
  `doc/init.md`, `doc/Doxyfile.in`, and `doc/release-process.md`.

- **Doc restructure** (`574981f`). Historical Litecoin/Bitcoin release
  notes were moved out of the active `doc/release-notes.md` and archived
  under `doc/historical-release-notes/` so they are preserved for license
  reasons but don't confuse contributors looking for current BurritoCoin
  release notes.

- **Website accessibility, SEO, and quality** (`18d948344`, `02854b151`,
  `1624a91e3`, `115625294`, `d3a94cd74`). The site got skip-to-content
  links, visible focus styles, a `robots.txt`, a `sitemap.xml`, canonical
  URL `<link>` tags, missing-alt-text fixes, mobile-layout repairs, dead
  download-link fixes (the Windows download was broken), TODO
  placeholders cleared, and a `/spec` integrator reference page wired
  into the nav.

- **Test framework rename** (`0972384`). The `test/functional/` helper
  modules and importers used `ltc_*` prefixes inherited from the
  Litecoin codebase. Renamed the helper modules and updated every
  importer to `brto_*` so the test framework speaks BurritoCoin
  vocabulary throughout.

- **Image-file mode fix** (`4e16495`). Several image assets (`.png`,
  `.ico`, `.icns`, `.bmp`) under `share/` and `src/qt/res/` were tracked
  with mode `755` (executable). Dropped to `644` for all of them.

- **Misattributed-identity leaks** (`64ec2b2`). A second-round audit
  caught remaining places where Bitcoin Core developers were credited
  as BurritoCoin developers — corrected.

- **`COPYRIGHT_YEAR` bump and translation `satoshi → burrioshi` sync**
  (`5576f7c`, the tip when this summary was written). `build_msvc/burritocoin_config.h:37` had
  `COPYRIGHT_YEAR=2024` while `configure.ac` was already on 2026, so
  Windows binaries shipped with the wrong year in `--license` output —
  bumped to 2026. Separately, 51 translation `.ts` files contained 168
  stale "satoshi" references in both `<source>` and `<translation>`
  elements; a bulk `\b`-bounded regex replace mapped
  `satoshi`/`satoshis`/`satoshi(s)` to `burrioshi`/`burrioshi`/
  `burrioshi(s)` while preserving "Satoshi Nakamoto" the proper noun.

A handful of earlier infrastructure commits — the spec page rebuild
(`d3a94cd7`), the soft-fork-height regtest revert (`c88f93f1`), and the
`release/` `.gitignore` addition (`f8880ca1`) — round out the recent
window.

---

## 6. Distribution path

Listing strategy is layered by realism. There is no point pursuing a
high-tier listing today; the chain has about 10,600 blocks and nobody is mining it, no audited binaries,
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

1. **HIGH — premine: a second, offline copy of `wallet.dat`.** The premine
   is no longer on a public-facing server: every service moved to hosts that
   hold no wallet, and the Linode that held `mainwallet` was retired
   (2026-08-18). The OneDrive backup was restore-tested before the Linode was
   deleted (opened in a fresh Qt wallet, balance confirmed). What remains is
   a **single-copy problem**: the encrypted `wallet.dat` exists only in
   OneDrive, and OneDrive syncs deletions — deleting the local copy by
   mistake deletes the cloud copy too, recoverable only from OneDrive's
   recycle bin and only for a limited time (30 days on personal accounts).
   Put a second copy on offline media kept somewhere physically separate,
   and restore-test that copy as well. An untested backup is not a backup.
2. **HIGH — resolve the original SSH key.** Its private half couldn't be
   found on 2026-09-22, but its public half is still authorized on the Oracle
   box, where `ubuntu` has passwordless sudo. Either confirm it is in the
   project lead's password manager and record that here, or delete its line
   from `~/.ssh/authorized_keys` (the working `burritocoin-oracle` key is
   unaffected). A ten-second fix.
3. **HIGH — add `maxtipage=315360000` to the live seed's `burritocoin.conf` and
   restart it.** Without it, once nobody has mined for 24 hours the seed
   considers itself still syncing and ignores `getheaders` from peers, so no
   new node on any OS can download the chain (reproduced with two regtest nodes
   on 2026-09-24; see §3). The repo's `contrib/oracle/burritocoin.conf.example`
   already carries the line. Confirm afterwards that the explorer's node page
   shows `"initialblockdownload": false`.
4. **HIGH — the wallet's Mine tab refuses to mine on a quiet chain.**
   `src/qt/miningutil.cpp` (`CaptureNodeHandles`) blocks both the built-in miner
   and the external-miner bridge whenever `isInitialBlockDownload()` is true —
   which, by default, is any time the newest block is over 24 hours old, however
   well synced the wallet is. So after a quiet day the GUI can't mine the block
   that would end the quiet. Until it's fixed, `/mine-windows` tells users to add
   `maxtipage` via Settings → Options → Open Configuration File. The real fix is
   to gate on "connected, and blocks == best known header" instead, then rebuild
   the Windows wallet with `.github/workflows/build-windows.yml` and republish it
   (new SHA256 on `/mine-windows#verify`, same commit).
5. **HIGH — official release binaries: Windows done, Linux and macOS
   not.** `.github/workflows/build-windows.yml` cross-builds the Qt wallet
   from `depends/` on a GitHub runner (manual dispatch, or any `v*` tag).
   Actions run `32530319035` produced the build published on 2026-08-28,
   which the site offers as a download (homepage `#download` and
   `/mine-windows`). Still to do: Linux and macOS builds — without them
   there is nothing for Tier 1 exchanges to integrate against, which gates
   the listing path — plus code signing (SmartScreen warns on every first
   launch) and publishing binaries as GitHub Release assets instead of
   committing them (each committed rebuild adds ~35 MB to history).
6. **HIGH — build the missing `burritocoin_scrypt` Python C-extension.**
   A subset of the functional test suite under `test/functional/` needs
   the Scrypt PoW callable from Python via a small C-extension. The
   extension hasn't been built yet, so those tests are currently
   skipped. Build it, wire it into the test runner, and turn the skips
   into real assertions.
7. **MEDIUM — add security headers.** Neither surface sets
   `Strict-Transport-Security`, `X-Frame-Options`, `X-Content-Type-Options`,
   `Referrer-Policy`, or a basic `Content-Security-Policy`. These now live in
   two different places: for `explorer.burritoco.in`, the nginx server block
   on the Oracle box; for `burritoco.in`, a `website/_headers` file, since
   Cloudflare Pages has no nginx to configure.
8. **MEDIUM — decide on MWEB pre-activation enforcement in
   `src/rpc/mining.cpp`.** The MWEB code path uses BIP8 signaling, and
   there is an open question about how strictly the mining RPCs should
   refuse to produce MWEB-flavored templates before activation is
   final. Resolve and document the decision. There is a deadline: the
   deployment is height-based with forced lock-in, so MWEB activates at
   block 24,192 whatever miners signal.
9. **LOW — three inflected `satoshi` stragglers in Finnish/Slovenian
   `.ts` files.** Inflected forms (`satoshia`, `satoshin`,
   `satoshijev`, `satošijev`) didn't match the `\b`-bounded regex used
   in commit `5576f7c` and need a native speaker to retranslate
   properly to the corresponding inflected forms of `burrioshi`.
10. **LOW — six `BRTO-TODO` markers in `src/chainparams.cpp`.** These
   are minor parameter-comment cleanups left behind by the rebrand.
   Walk through them and decide for each whether to clarify or
   delete.
11. **LOW — reboot the Oracle box for its staged kernel.** Logins have
   shown `*** System restart required ***` since at least 2026-09-22.
   `sudo reboot` takes the seed, explorer and ElectrumX down for about a
   minute. Check that every unit comes back on boot first:
   `systemctl is-enabled burritocoind electrumx btc-rpc-explorer nginx`.

12. **LOW — make `./configure` work on Boost 1.89+ without a flag.** Boost 1.89
    dropped the compiled Boost.System library, so the build now needs
    `--with-boost-system=no` (every website guide and the runbook's Phase 4 pass
    it; verified with full builds on Boost 1.83 and 1.90 on 2026-09-24). Boost.System has been header-only since 1.69, so
    `configure.ac` could skip `AX_BOOST_SYSTEM` when the headers are new enough.
13. **LOW — macOS build tooling is stale.** `contrib/install_db4.sh` fails on
    macOS (its savannah URLs now redirect, and BDB 4.8 needs
    `-Wno-error=implicit-function-declaration` on current Xcode), and
    `doc/build-osx.md` still recommends `berkeley-db4` and Qt 6. `/mine-mac` uses
    Homebrew's `berkeley-db@5` instead and says to trust it over the doc. Not
    executed end to end: the macOS guide (checked against Homebrew's formula
    index, Boost 1.92's headers and this repo's configure macros), the Fedora
    commands (package names checked against Fedora's index), and the Windows
    GUI walkthrough (checked against `src/qt`). Only the Ubuntu 24.04 path was
    run start to finish.

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
git pull --ff-only origin master       # fails on any clone made before the
                                       # 2026-08-28 rewrite (§5): re-clone instead
./contrib/devtools/install-hooks.sh    # install the post-commit hook
./contrib/devtools/update-changelog.sh # regenerate CHANGELOG.md (full clone only)
```

The Oracle box's own checkout, `/home/ubuntu/BurritoCoin`, was cloned on
2026-08-18 — before the rewrite — so re-clone it before building from it
again.

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
  `git log` by `contrib/devtools/update-changelog.sh` (it can lag; see §5).
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
- `doc/oracle-migration.md` — the Linode → Oracle/Cloudflare migration
  runbook, as built, including the cutover record.
- `contrib/oracle/` — systemd units, nginx config, and an example
  `burritocoin.conf` for the Oracle box.
- `contrib/vps/` — the retired Linode's provisioning scripts;
  `setup-electrumx.sh` is still how ElectrumX gets installed.
- `.github/workflows/build-windows.yml` — cross-builds the Windows wallet.
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
