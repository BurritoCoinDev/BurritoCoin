# BurritoCoin block explorer

The site at https://explorer.burritoco.in runs btc-rpc-explorer
(https://github.com/janoside/btc-rpc-explorer), a generic Bitcoin-style
explorer, re-skinned for BurritoCoin.

burritocoin-explorer.patch is a binary-safe git diff capturing every
customization that turns vanilla btc-rpc-explorer into the BRTO explorer:

- Coin registration (app/coins.js, app/coins/brto.js)
- Branding: logos, favicons, icon assets under public/img/network-mainnet/
- Currency labels: visible amount unit reads BRTO instead of BTC
  (app/currencies.js, views/layout.pug, views/includes/shared-mixins.pug)
- Page titles ("BurritoCoin Explorer" instead of "BTC Explorer")
  (views/layout.pug, views/layout-iframe.pug)
- Static "BTC" -> "BRTO" copy on UTXO-set, block-analysis, snippets
  (views/utxo-set.pug, views/block-analysis.pug, views/snippets/utxo-set.pug)
- Genesis-coinbase render fix so block 105 (the premine-split tx) loads
  instead of returning "Failed loading block". The vendor code
  short-circuits getrawtransaction for the genesis coinbase and reads a
  hardcoded genesisCoinbaseTransactionsByNetwork object that does not
  exist for BRTO. Fix guards the branch so it falls through to the live
  RPC, which serves the tx (BRTO, unlike upstream Bitcoin, does index
  the genesis coinbase). Files: app/api/rpcApi.js, routes/baseRouter.js.
- Rotating quotes rewritten from Satoshi/Bitcoin lore to BurritoCoin bits
  (app/coins/btcQuotes.js). The canonical, extensible list lives at
  contrib/explorer/btcQuotes.js and is copied over app/coins/btcQuotes.js on
  deploy (see below) — add new quotes there, not in the binary patch.

The Mining Summary dup-row fix is kept OUT of the binary patch (see below):

- Mining Summary dup-row fix (views/mining-summary.pug): the vendor page
  leaks its 125ms status-poll timer between loads and re-renders by appending
  rows without a reset, so toggling the 1d/3d range buttons makes one build's
  miner rows render two-or-more times (doubled rows, inflated "Total"). Two
  idempotency guards fix it. Applied by fix-mining-summary-dedup.js instead of
  the binary patch, so it is immune to upstream whitespace drift and safe to
  re-run. (The donut's >1%-revenue slice threshold is left as-is — that is
  vendor-intended, not a bug: sub-1% miners fold into "Other" in the chart but
  still appear in the Data table.)

## Upstream base

The patch applies cleanly on top of upstream commit 26e282a
(janoside/btc-rpc-explorer, "dependencies", tagged after v3.5.1).

## Re-applying on a fresh box

    cd /opt
    git clone https://github.com/janoside/btc-rpc-explorer.git
    cd btc-rpc-explorer
    git checkout 26e282a
    git apply /root/BurritoCoin/contrib/explorer/burritocoin-explorer.patch
    node /root/BurritoCoin/contrib/explorer/fix-mining-summary-dedup.js   # Mining Summary dup-row fix (idempotent)
    cp /root/BurritoCoin/contrib/explorer/btcQuotes.js app/coins/btcQuotes.js   # canonical rotating quotes
    npm install
    # Copy your existing .env from the running box (RPC creds, port, etc.)
    systemctl enable --now btc-rpc-explorer

## Updating the patch after future edits

    cd /opt/btc-rpc-explorer
    git diff 26e282a HEAD --binary > /root/BurritoCoin/contrib/explorer/burritocoin-explorer.patch
    cd /root/BurritoCoin
    git add contrib/explorer/burritocoin-explorer.patch
    git commit -m "Update explorer patch"

The Mining Summary dup-row fix is maintained as fix-mining-summary-dedup.js
rather than in the binary patch. It is idempotent (a no-op if already
present), so regenerating the patch above is safe whether or not that edit was
committed into the explorer checkout first.

## Adding rotating quotes

Quotes live in contrib/explorer/btcQuotes.js (the canonical list), not in the
binary patch. To add more: edit that file, then on the box copy it into place
and restart:

    cp /root/BurritoCoin/contrib/explorer/btcQuotes.js /opt/btc-rpc-explorer/app/coins/btcQuotes.js
    systemctl restart btc-rpc-explorer
