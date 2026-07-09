// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningutil.h>

#include <qt/walletmodel.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <node/context.h>
#include <outputtype.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <validation.h>

#include <memory>

#include <QSettings>

namespace MiningUtil {

bool ResolveCoinbaseScript(WalletModel* wallet_model, CScript& out, QString& err)
{
    if (!wallet_model) {
        err = QObject::tr("No wallet is loaded to receive the mining reward.");
        return false;
    }

    // Cache the payout address per wallet NAME (not process-global) so a
    // multi-wallet user mining into wallet A while viewing wallet B can't be
    // silently cross-credited. Reuse a previously chosen address if it is still
    // a valid destination; otherwise mint a fresh one and remember it.
    const QString wallet_name = wallet_model->getWalletName();
    QSettings settings;
    const QString key = QStringLiteral("mining/%1/payout_address").arg(wallet_name);

    CTxDestination dest;
    bool have_dest = false;
    const QString cached = settings.value(key).toString();
    if (!cached.isEmpty()) {
        dest = DecodeDestination(cached.toStdString());
        // Require not just a well-formed address but one this wallet can still
        // spend. Otherwise a wallet restored from an older backup (or a reused
        // wallet name pointing at a different file) would keep paying rewards to
        // an address the user no longer controls. If it's not spendable, fall
        // through and mint a fresh owned address (which also rewrites the cache).
        have_dest = IsValidDestination(dest) && wallet_model->wallet().isSpendable(dest);
    }
    if (!have_dest) {
        if (!wallet_model->wallet().getNewDestination(OutputType::BECH32, "Mining", dest)) {
            err = QObject::tr("Could not get a payout address from the wallet. If the wallet is "
                              "encrypted, unlock it first, then try again.");
            return false;
        }
        settings.setValue(key, QString::fromStdString(EncodeDestination(dest)));
    }

    out = GetScriptForDestination(dest);
    if (out.empty()) {
        err = QObject::tr("The wallet returned an unusable payout address.");
        return false;
    }
    return true;
}

bool CaptureNodeHandles(interfaces::Node& node, ChainstateManager*& chainman,
                        CTxMemPool*& mempool, QString& err)
{
    // Resolve the node context first. isInitialBlockDownload() dereferences the
    // active chainstate, so calling it before we know the node is fully started
    // could assert during a narrow startup window; check the context is up first.
    NodeContext* ctx = node.context();
    if (!ctx || !ctx->chainman || !ctx->mempool) {
        err = QObject::tr("Mining is unavailable right now: the node is not fully started.");
        return false;
    }

    if (node.isInitialBlockDownload()) {
        err = QObject::tr("BurritoCoin is still syncing. Mining becomes available once "
                          "the blockchain is up to date.");
        return false;
    }
    chainman = ctx->chainman;
    mempool = ctx->mempool.get();
    return true;
}

bool SubmitBlock(ChainstateManager& chainman, const CBlock& block, std::string& hashOut)
{
    bool new_block = false;
    bool accepted = false;
    try {
        // ProcessNewBlock takes cs_main itself (LOCKS_EXCLUDED) — safe to call
        // off the main thread, and we deliberately do NOT pre-lock here. This is
        // the trusted re-validation path: an externally-mined block from the
        // Stratum bridge gets the exact same full validation as an in-process one.
        auto shared_block = std::make_shared<const CBlock>(block);
        accepted = chainman.ProcessNewBlock(Params(), shared_block, /*fForceProcessing=*/true,
                                            /*fNewBlock=*/&new_block);
    } catch (const std::exception&) {
        return false;
    }
    // ProcessNewBlock returns true even for a block we ALREADY have (its
    // fAlreadyHave short-circuit returns true with fNewBlock left false) and for
    // a valid block that lands on a side branch without becoming our tip.
    // Reporting either as "found" would double-count a resent winning share or
    // miscount an orphaned sibling. Only report a genuinely-new block that is
    // now the active chain tip.
    if (!accepted || !new_block) return false;
    {
        LOCK(cs_main);
        const CBlockIndex* tip = chainman.ActiveTip();
        if (!tip || tip->GetBlockHash() != block.GetHash()) return false;
    }
    hashOut = block.GetHash().GetHex();
    return true;
}

} // namespace MiningUtil
