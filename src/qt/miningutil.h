// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_MININGUTIL_H
#define BURRITOCOIN_QT_MININGUTIL_H

#include <string>

#include <QString>

class CBlock;
class CScript;
class ChainstateManager;
class CTxMemPool;
class WalletModel;
namespace interfaces { class Node; }

//! Consensus-touching helpers shared by both mining engines (the in-process
//! MiningModel and the external-miner StratumBridge), so there is exactly one
//! copy of the payout-address, node-handle, and block-submit paths. All MIT;
//! nothing here links to or depends on any external miner.
namespace MiningUtil {

//! Resolve the per-wallet payout coinbase script. The address is cached per
//! wallet NAME and reused only while it is still spendable and no block has
//! paid it yet; otherwise a fresh one is minted (and cached). If a locked
//! wallet has used up its pre-generated keys, it asks for the passphrase so
//! the wallet can make more; if that is cancelled, a spendable cached
//! address is reused rather than refusing. Call it only when the user starts
//! mining, since it may show the unlock dialog. Returns false and sets err on
//! failure.
bool ResolveCoinbaseScript(WalletModel* wallet_model, CScript& out, QString& err);

//! Call after a block paid the current payout address: mark it used and mint
//! the next one, so every block pays a different address. Returns false, with
//! out unchanged, if the wallet can't give a new address (e.g. it is locked and
//! out of pre-generated keys); the caller should keep mining to the old one,
//! and the next ResolveCoinbaseScript() will try again.
bool RotateCoinbaseScript(WalletModel* wallet_model, CScript& out);

//! Capture the node's chainstate-manager and mempool handles for mining,
//! refusing while the node is not fully started, has no peer, or is
//! behind (see CheckMiningReadiness in node/miningreadiness.h).
//! Returns false and sets err on failure.
bool CaptureNodeHandles(interfaces::Node& node, ChainstateManager*& chainman,
                        CTxMemPool*& mempool, QString& err);

//! Submit a solved block through ProcessNewBlock — the trusted re-validation
//! path (full CheckBlock/AcceptBlock/ConnectBlock). Returns true if accepted and
//! sets hashOut to the block hash hex. Never throws.
bool SubmitBlock(ChainstateManager& chainman, const CBlock& block, std::string& hashOut);

} // namespace MiningUtil

#endif // BURRITOCOIN_QT_MININGUTIL_H
