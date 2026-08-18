# Migrating off Linode to $0/month (Oracle free tier + Cloudflare)

Goal: stop paying for the Linode. End state:

| What                       | Where it ends up                          | Cost | Status |
|----------------------------|-------------------------------------------|------|--------|
| Marketing site             | Cloudflare Pages                          | $0   | **DONE 2026-08-18** |
| DNS                        | Cloudflare (from WordPress.com)           | $0   | **DONE 2026-08-18** |
| Block explorer             | Oracle A1 VM, behind Cloudflare proxy     | $0   | pending |
| ElectrumX                  | Oracle A1 VM                              | $0   | pending |
| Seed node (`burritocoind`) | Oracle A1 VM (validate/relay only)        | $0   | pending |
| Second (loopback) daemon   | Oracle A1 VM                              | $0   | pending |
| Mining                     | **owner's Windows PC — never a cloud box**| $0   | **BLOCKER, see Phase 0** |
| Linode                     | cancelled after a ~30-day parallel run    | $0   | pending |

The Linode runs **four** services, not two: `burritocoind` (main),
`burritocoind` (loopback peer, `-datadir=/root/.burritocoin-peer`),
ElectrumX, and the Node explorer. All four move.

Hard rules for the Oracle box:

- **Never mine on it.** Oracle's Cloud Services Agreement §1.3(d) bans
  crypto *mining* on every account type, and enforcement is automated and
  disable-first. A validating/relaying node and an explorer are outside the
  ban's text — mining is not. Mining stays on your own hardware.
- **No wallets on it.** The node is built `--disable-wallet`. A box that an
  abuse bot can summarily disable must hold nothing irreplaceable — chain
  data re-syncs, wallets don't.

---

## Phase 0 — Retire the VPS miner (BLOCKS the Oracle cutover)

Discovered 2026-08-18: the Linode has been CPU-mining since Jul 31 via
`/usr/local/bin/brto-miner.sh`, which loops

    burritocoin-cli -rpcclienttimeout=0 -rpcwallet=vps-mining \
        generatetoaddress 1 <addr> 1000000

That is the box's ~92% CPU load. `generatetoaddress` runs inside an RPC
http-worker thread; the client times out at 60 s and disconnects (hence
`socket send error Broken pipe` once a minute in debug.log) while the server
keeps mining, so all four http workers stay saturated.

Two consequences:

1. **This must not follow the migration.** Running it on OCI violates CSA
   §1.3(d) outright — not merely the heuristic risk that a busy node might be
   mistaken for a miner.
2. **It is currently the network's only miner.** The owner's Windows PC is
   not mining. At ~1.6 kH/s against difficulty 0.0039 a block takes ~2.8 h
   (vs. the 150 s target), which matches the "Potential stale tip detected"
   entries. Kill the script with nothing to replace it and the chain stops
   advancing.

Order of operations, therefore:

1. Start mining on the Windows PC (see `website/mine-windows.html`).
2. Confirm blocks are landing — `burritocoin-cli getblockcount` climbing, and
   `getmininginfo` showing a healthier `networkhashps`.
3. Only then: `pkill -f brto-miner.sh`, and delete
   `/usr/local/bin/brto-miner.sh` so it cannot be resurrected by a reboot.
4. The `vps-mining` wallet holds real mined BRTO. It goes on the
   copy-off-before-decommission list next to the premine wallet.

---

## Phase 1 — Cloudflare (do first; independent of Oracle)

**Completed 2026-08-18.** What was actually done, and the gotchas hit:

1. **Pages first, DNS second.** The project was created and verified on
   `burritocoin.pages.dev` *before* any DNS moved, so the nameserver switch
   was a flip to something already proven. Settings: framework preset
   *None*, build command *empty*, build output directory `website`,
   production branch `master`. Every push to `master` redeploys in <1 min.
2. **The dashboard buries Pages under the Workers wizard.** "Create an
   application" lands in the Worker flow (tell-tale: a `npx wrangler deploy`
   deploy command). The Pages flow is the "Looking to deploy Pages?
   Get started" link, or `/workers-and-pages/create/pages` directly.
3. **DNS was hosted at WordPress.com**, not the Linode. Nameservers were
   swapped there (Domains → the domain → Name servers → turn off "Use
   WordPress.com name servers"). There are **no MX records** — the domain
   receives no mail — so there was no mail-outage risk.
4. **Cloudflare's zone scan missed the subdomains.** It imported only 5 of
   the 9 records: `explorer`, `seed`, and both `wpcloud*._domainkey` CNAMEs
   had to be added by hand. Always diff the scan against the old provider's
   list before switching nameservers. The `google-site-verification` TXT is
   load-bearing (Search Console) — verify it survives.
5. **Everything stayed grey-cloud during the switch** so behaviour was
   identical before and after; Pages flipped the apex and `www` to proxied
   when the custom domains were attached.
6. **`explorer.burritoco.in` is DNS only (grey cloud)**, pointing at the
   Linode. That preserves the existing Let's Encrypt cert path and keeps the
   zone-wide SSL mode (**Full (strict)**, required by Pages — never
   *Flexible*, which loops with Pages) from affecting it.
7. **`seed.burritoco.in` must be DNS only (grey cloud), now and forever** —
   P2P on 9227 is not HTTP and cannot traverse Cloudflare's proxy. This
   record intentionally exposes the node IP; that is how P2P works.
8. **www → apex redirect is a zone-level Redirect Rule, not `_redirects`.**
   Cloudflare Pages matches `_redirects` against paths only, so a rule whose
   source contains a hostname silently never fires. Use Rules → Redirect
   Rules → template "Redirect from WWW to root" (wildcard `https://www.*` →
   `https://${1}`, 301, *Preserve query string* checked). Cloudflare warns
   "this rule may not apply — www may not be proxied"; that is a false
   positive for Pages-managed CNAMEs. Ignore it; do **not** let it create a
   second DNS record.

Verified after cutover: all pages 200, branded 404 served, `www` 301s to the
apex preserving path and query, apex does not redirect, and `explorer` and
`seed` still resolve to the Linode.

## Phase 2 — Oracle account

1. Sign up at oracle.com/cloud/free. **The home region is chosen at signup
   and can never be changed**, and Always Free resources only exist in the
   home region. Pick for A1 capacity, not latency — avoid the busiest US
   regions; historically less-contested choices (e.g. a secondary US region
   or eu-frankfurt-1) fill A1 requests faster.
2. **Upgrade the account to Pay As You Go** (card on file). You are still
   $0 while inside Always Free limits. This matters three ways: it exempts
   instances from the 7-day idle-reclamation rule, it gives A1 capacity
   priority (largely ends "out of host capacity"), and it gets you real
   support. Set a budget alert at $1 so any surprise charge emails you.
3. Free allowance since 2026-06-15: **2 OCPU / 12 GB RAM** of A1 (1,500
   OCPU-hrs + 9,000 GB-hrs/month), 200 GB total block storage, 10 TB/month
   egress. Ample for this stack. (PAYG accounts may retain the old 4/24 —
   Oracle support has said both yes and no; assume 2/12 and be pleasantly
   surprised.)

## Phase 3 — Create the VM and open the firewall

1. Compute → Instances → Create. Shape `VM.Standard.A1.Flex`, **2 OCPU /
   12 GB**. Image: Ubuntu 24.04 (aarch64). Boot volume: **150 GB**. Add
   your SSH public key. If creation fails "out of host capacity", retry
   other availability domains / times (rare on PAYG).
2. **Firewall layer 1 — VCN security list** (Networking → your VCN →
   security list): add stateful ingress rules, source `0.0.0.0/0`, TCP:

   | Port    | Service                                  | Public? |
   |---------|------------------------------------------|---------|
   | `9227`  | mainnet P2P                              | yes     |
   | `50001` | ElectrumX (Electrum wallet clients)       | yes     |
   | `80`    | explorer / ACME http-01                  | yes     |
   | `443`   | explorer (TLS)                           | yes     |
   | `9226`  | mainnet RPC                              | **NO — localhost only** |
   | `29226` | loopback peer RPC                        | **NO — localhost only** |
   | `8000`  | ElectrumX admin RPC                      | **NO — localhost only** |

   Leave 22 as-is. Do not expose the RPC ports: `burritocoin.conf` binds
   them to 127.0.0.1, and the security list should not contradict that.
3. **Firewall layer 2 — the OS.** Oracle's Ubuntu images ship iptables
   REJECT rules that block everything but SSH, and rules must be inserted
   *before* the final REJECT (the classic OCI trap — opening the security
   list alone is not enough, and this is the #1 reason a port "doesn't
   work" on OCI):

       sudo iptables -L INPUT --line-numbers   # find the REJECT line, usually 6
       for p in 9227 50001 80 443; do
         sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport $p -j ACCEPT
       done
       sudo apt install -y iptables-persistent && sudo netfilter-persistent save

## Phase 4 — Build and run the node (aarch64, no wallet)

    sudo apt update
    sudo apt install -y build-essential libtool autotools-dev automake \
        pkg-config bzip2 curl git python3 bison
    sudo useradd -m -s /bin/bash burrito
    sudo -iu burrito git clone https://github.com/BurritoCoinDev/BurritoCoin.git
    cd /home/burrito/BurritoCoin

    # Pinned deps built natively for ARM. ~30-60 min on 2 OCPU; NO_QT/NO_WALLET
    # skip everything the headless node doesn't need.
    make -C depends -j"$(nproc)" NO_QT=1 NO_WALLET=1

    ./autogen.sh
    CONFIG_SITE=$PWD/depends/aarch64-unknown-linux-gnu/share/config.site \
        ./configure --disable-wallet --without-gui --disable-tests --disable-bench
    make -j"$(nproc)"
    sudo install -m 755 src/burritocoind src/burritocoin-cli /usr/local/bin/

(`ls depends/` if the triplet directory name differs.)

`/home/burrito/.burritocoin/burritocoin.conf` — generate the `rpcauth` line
with `share/rpcauth/rpcauth.py <user>`:

    server=1
    txindex=1
    listen=1
    dbcache=1024
    rpcbind=127.0.0.1
    rpcallowip=127.0.0.1
    rpcauth=<paste from rpcauth.py>
    # Parallel-run only — mesh with the Linode node; remove after cutover:
    addnode=50.116.17.170:9227

Install the systemd unit and start:

    sudo cp contrib/oracle/burritocoind.service /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable --now burritocoind
    burritocoin-cli getblockcount   # should climb to the live height

`txindex=1` is required by the explorer. The chain re-syncs from the Linode
node in minutes-to-hours at current chain size.

## Phase 5 — Explorer, ElectrumX, and the loopback peer

> The Linode runs four services. Alongside the main node (Phase 4) and the
> explorer below, port `contrib/vps/setup-second-peer.sh` (loopback sibling
> daemon, keeps peer count off zero so `getblocktemplate` keeps working) and
> `contrib/vps/setup-electrumx.sh` (ElectrumX with a BurritoCoin Coin class,
> wired into btc-rpc-explorer). Both scripts are in the repo and are the
> source of truth for those two services. Neither carries a miner — keep it
> that way.

### Explorer

Follow `contrib/explorer/README.md` ("Re-applying on a fresh box") — clone
upstream `26e282a`, apply `burritocoin-explorer.patch`, run
`fix-mining-summary-dedup.js`, copy `btcQuotes.js`, `npm install`. Notes
for this box:

- Install Node.js LTS arm64 (NodeSource). Match the major version the
  Linode runs (`node --version` there) to avoid surprises. Native-module
  builds (node-gyp) use the compilers installed in Phase 4.
- Copy `.env` from the Linode (`/opt/btc-rpc-explorer/.env`), updating the
  RPC password to match the new `rpcauth`.
- Install `contrib/oracle/btc-rpc-explorer.service`, then nginx:

      sudo apt install -y nginx
      sudo cp contrib/oracle/nginx-explorer.conf /etc/nginx/sites-available/explorer
      sudo ln -s /etc/nginx/sites-available/explorer /etc/nginx/sites-enabled/
      sudo rm -f /etc/nginx/sites-enabled/default
      sudo nginx -t && sudo systemctl reload nginx

- Sanity-check by browsing `http://<oracle-ip>/` directly before touching
  DNS.

## Phase 6 — Parallel run (~30 days)

Flip `explorer.burritoco.in`'s A record to the Oracle IP (keep it Proxied;
set SSL/TLS to "Full" with a free Cloudflare Origin cert in nginx, or
"Flexible" to defer that). Add a **second** A record on
`seed.burritoco.in` for the Oracle IP alongside the Linode one — DNS then
serves both and wallets use whichever answers, so the network has two seeds
for the duration.

Weekly checklist:

- [ ] `burritocoin-cli getblockcount` matches on both boxes
- [ ] `getpeerinfo` on Oracle shows the Linode node + outside peers
- [ ] explorer.burritoco.in loads pages, search works, tip is current
- [ ] A desktop wallet with no special config finds peers (DNS seed works)
- [ ] Oracle console shows no reclamation/abuse notices; $0.00 billed

## Phase 7 — Cutover and cancel

1. Remove the Linode A record from `seed.burritoco.in` (Oracle record
   stays). Remove the `addnode=50.116.17.170` line from the Oracle node's
   conf.
2. Update `src/chainparamsseeds.h` (regenerate via
   `contrib/seeds/generate-seeds.py`, or edit the BIP155 bytes) so the
   fixed-seed fallback is the Oracle IP, and cut a wallet release. Old
   binaries keep working meanwhile via the DNS seed.
3. **Before destroying the Linode:**
   - [ ] Copy every wallet file off it (`~/.burritocoin/wallets/` and any
         `wallet.dat`), verify the copies open elsewhere, store offline.
         **This is the premine — do not skip the verify step.**
   - [ ] Save `/opt/btc-rpc-explorer/.env`, nginx configs, crontabs.
   - [ ] Final `burritocoin-cli stop`; snapshot anything else worth keeping.
4. Cancel the Linode. Monthly bill: **$0**.

## Ongoing cautions

- The free tier can change without notice (the 2026-06-15 halving was
  silent). The PAYG upgrade + budget alert is the early-warning system.
- If Oracle ever flags the box ("coinmining activity" false positives are
  documented), it is disabled first and argued about later. Everything on
  it is rebuildable from this repo + the chain; treat the VM as disposable.
- Keep at least one off-site copy of this repo current — it *is* the
  disaster-recovery plan.
