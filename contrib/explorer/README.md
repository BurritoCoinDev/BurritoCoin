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
- BTC-only marketing copy removed (app/coins/btcQuotes.js emptied).

## Upstream base

The patch applies cleanly on top of upstream commit 26e282a
(janoside/btc-rpc-explorer, "dependencies", tagged after v3.5.1).

## Re-applying on a fresh box

    cd /opt
    git clone https://github.com/janoside/btc-rpc-explorer.git
    cd btc-rpc-explorer
    git checkout 26e282a
    git apply /root/BurritoCoin/contrib/explorer/burritocoin-explorer.patch
    npm install
    # Copy your existing .env from the running box (RPC creds, port, etc.)
    systemctl enable --now btc-rpc-explorer

## Updating the patch after future edits

    cd /opt/btc-rpc-explorer
    git diff 26e282a HEAD --binary > /root/BurritoCoin/contrib/explorer/burritocoin-explorer.patch
    cd /root/BurritoCoin
    git add contrib/explorer/burritocoin-explorer.patch
    git commit -m "Update explorer patch"
