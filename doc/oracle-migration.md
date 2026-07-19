# Migrating off Linode to $0/month (Oracle free tier + Cloudflare)

Goal: stop paying for the Linode. End state:

| What                       | Where it ends up                          | Cost |
|----------------------------|-------------------------------------------|------|
| Marketing site             | Cloudflare Pages                          | $0   |
| Block explorer             | Oracle A1 VM, behind Cloudflare proxy     | $0   |
| Seed node (`burritocoind`) | Oracle A1 VM (validate/relay only)        | $0   |
| Linode                     | cancelled after a ~30-day parallel run    | $0   |

Hard rules for the Oracle box:

- **Never mine on it.** Oracle's Cloud Services Agreement §1.3(d) bans
  crypto *mining* on every account type, and enforcement is automated and
  disable-first. A validating/relaying node and an explorer are outside the
  ban's text — mining is not. Mining stays on your own hardware.
- **No wallets on it.** The node is built `--disable-wallet`. A box that an
  abuse bot can summarily disable must hold nothing irreplaceable — chain
  data re-syncs, wallets don't.

---

## Phase 1 — Cloudflare (do first; independent of Oracle)

1. Add `burritoco.in` to a free Cloudflare account and switch the domain's
   nameservers to Cloudflare's (registrar dashboard). Import/recreate the
   existing DNS records before switching so nothing drops.
2. **Pages (static site):** Workers & Pages → Create → Pages → connect the
   GitHub repo. Build command: *(none)*. Build output directory: `website`.
   Add custom domains `burritoco.in` and `www.burritoco.in` when prompted.
   Every push to the branch redeploys the site.
3. **Explorer proxy:** leave `explorer.burritoco.in` pointing at the Linode
   for now, but set it to **Proxied** (orange cloud). SSL/TLS mode "Full" if
   the Linode has a cert, otherwise "Flexible" until Phase 5.
4. **Seed record:** `seed.burritoco.in` must be **DNS only** (grey cloud),
   now and forever — P2P on 9227 is not HTTP and cannot go through
   Cloudflare's proxy. This record intentionally exposes the node IP; that
   is how P2P works.

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
   security list): add stateful ingress rules, source `0.0.0.0/0`, TCP
   ports `9227` (P2P), `80`, `443` (explorer). Optionally `19227`
   (testnet). Leave 22 as-is.
3. **Firewall layer 2 — the OS.** Oracle's Ubuntu images ship iptables
   REJECT rules that block everything but SSH, and rules must be inserted
   *before* the final REJECT (the classic OCI trap):

       sudo iptables -L INPUT --line-numbers   # find the REJECT line, usually 6
       sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport 9227 -j ACCEPT
       sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport 80  -j ACCEPT
       sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport 443 -j ACCEPT
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

## Phase 5 — Explorer on the Oracle box

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
