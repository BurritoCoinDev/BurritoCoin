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
//! wallet NAME and re-validated as spendable; a fresh one is minted (and cached)
//! when missing or no longer owned. Returns false and sets err on failure.
bool ResolveCoinbaseScript(WalletModel* wallet_model, CScript& out, QString& err);

//! Capture the node's chainstate-manager and mempool handles for mining,
//! refusing while the node is in initial block download or not fully started.
//! Returns false and sets err on failure.
bool CaptureNodeHandles(interfaces::Node& node, ChainstateManager*& chainman,
                        CTxMemPool*& mempool, QString& err);

//! Submit a solved block through ProcessNewBlock — the trusted re-validation
//! path (full CheckBlock/AcceptBlock/ConnectBlock). Returns true if accepted and
//! sets hashOut to the block hash hex. Never throws.
bool SubmitBlock(ChainstateManager& chainman, const CBlock& block, std::string& hashOut);

} // namespace MiningUtil

#endif // BURRITOCOIN_QT_MININGUTIL_H
