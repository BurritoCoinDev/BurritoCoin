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
#include <node/miningreadiness.h>
#include <outputtype.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <validation.h>

#include <memory>

#include <QSettings>

namespace MiningUtil {

namespace {

QString PayoutKey(WalletModel* wallet_model, const char* field)
{
    return QStringLiteral("mining/%1/%2").arg(wallet_model->getWalletName(), QLatin1String(field));
}

//! Mint a fresh payout address, cache it as the next one to use, and return its script.
bool MintCoinbaseScript(WalletModel* wallet_model, CScript& out, QString& err)
{
    CTxDestination dest;
    if (!wallet_model->wallet().getNewDestination(OutputType::BECH32, "Mining", dest)) {
        err = QObject::tr("Could not get a payout address from the wallet. If the wallet is "
                          "encrypted, unlock it first, then try again.");
        return false;
    }
    CScript script = GetScriptForDestination(dest);
    if (script.empty()) {
        err = QObject::tr("The wallet returned an unusable payout address.");
        return false;
    }
    QSettings settings;
    settings.setValue(PayoutKey(wallet_model, "payout_address"), QString::fromStdString(EncodeDestination(dest)));
    settings.setValue(PayoutKey(wallet_model, "payout_unused"), true);
    out = script;
    return true;
}

} // namespace

bool ResolveCoinbaseScript(WalletModel* wallet_model, CScript& out, QString& err)
{
    if (!wallet_model) {
        err = QObject::tr("No wallet is loaded to receive the mining reward.");
        return false;
    }

    // Cache the payout address per wallet NAME (not process-global) so a
    // multi-wallet user mining into wallet A while viewing wallet B can't be
    // silently cross-credited. Reuse the cached address only while no block has
    // paid it yet: each block then pays a different address, which keeps one
    // miner's blocks from all reading as the same payee on the explorer.
    // "payout_unused" is set when an address is minted and cleared once a block
    // pays it. Builds before it never set it, so their cached address (which
    // their blocks did pay) is replaced on the first start.
    QSettings settings;
    CScript cached_script;
    const QString cached = settings.value(PayoutKey(wallet_model, "payout_address")).toString();
    if (!cached.isEmpty()) {
        const CTxDestination dest = DecodeDestination(cached.toStdString());
        // Require not just a well-formed address but one this wallet can still
        // spend. Otherwise a wallet restored from an older backup (or a reused
        // wallet name pointing at a different file) would keep paying rewards to
        // an address the user no longer controls.
        if (IsValidDestination(dest) && wallet_model->wallet().isSpendable(dest)) {
            cached_script = GetScriptForDestination(dest);
        }
    }
    if (!cached_script.empty() && settings.value(PayoutKey(wallet_model, "payout_unused"), false).toBool()) {
        out = cached_script;
        return true;
    }
    if (MintCoinbaseScript(wallet_model, out, err)) return true;
    // An encrypted legacy wallet (what Create Wallet makes here) that stays
    // locked can't make addresses beyond the ~1000 it made in advance, and
    // each block uses one. This runs when the
    // user presses Start, so ask for the passphrase once: unlocked, the wallet
    // makes a fresh batch, then the context locks it again. Only ask if that
    // can work: a wallet with no HD seed (made blank, or restored from a
    // recovery file) can't make addresses even when unlocked, and
    // canGetAddresses() says so whether or not it is locked.
    if (wallet_model->getEncryptionStatus() == WalletModel::Locked && wallet_model->wallet().canGetAddresses()) {
        WalletModel::UnlockContext unlock(wallet_model->requestUnlock());
        if (unlock.isValid() && MintCoinbaseScript(wallet_model, out, err)) return true;
    }
    // Still no fresh address (unlock cancelled). Paying the last address again
    // beats refusing to mine, which builds before per-block addresses never did.
    if (!cached_script.empty()) {
        out = cached_script;
        return true;
    }
    if (!wallet_model->wallet().canGetAddresses()) {
        // Unlocking can't help this wallet, so don't say it would.
        err = QObject::tr("This wallet can't make new addresses, because it has no recovery key "
                          "(for example, it was restored from a recovery file, or made blank or "
                          "watch-only). Mine into a wallet that has one, such as a standard new "
                          "wallet or one restored from a recovery key.");
    }
    return false;
}

bool RotateCoinbaseScript(WalletModel* wallet_model, CScript& out)
{
    if (!wallet_model) return false;
    // The current address is used now whatever happens next, so a later
    // ResolveCoinbaseScript() must not hand it out again.
    QSettings().setValue(PayoutKey(wallet_model, "payout_unused"), false);
    QString err;
    return MintCoinbaseScript(wallet_model, out, err);
}

bool CaptureNodeHandles(interfaces::Node& node, ChainstateManager*& chainman,
                        CTxMemPool*& mempool, QString& err)
{
    // Resolve the node context first. Reading the active chain dereferences the
    // chainstate, so doing it before we know the node is fully started could
    // assert during a narrow startup window; check the context is up first.
    NodeContext* ctx = node.context();
    if (!ctx || !ctx->chainman || !ctx->mempool) {
        err = QObject::tr("Mining is unavailable right now: the node is not fully started.");
        return false;
    }

    // Deliberately not node.isInitialBlockDownload(): after startup that stays
    // true until a block less than a day old arrives, so after a quiet day it
    // refused to mine the very block that would end the quiet. CheckMiningReadiness
    // (node/miningreadiness.h) asks the narrower question mining needs. Test
    // chains skip it, as getblocktemplate does, so a lone regtest node can mine.
    const MiningReadiness readiness = Params().IsTestChain() ? MiningReadiness::READY : CheckMiningReadiness(*ctx);
    switch (readiness) {
    case MiningReadiness::READY:
        break;
    case MiningReadiness::NO_PEERS:
        err = QObject::tr("Not connected to the BurritoCoin network yet. Mining becomes available "
                          "once the wallet connects to it.");
        return false;
    case MiningReadiness::SYNCING:
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
