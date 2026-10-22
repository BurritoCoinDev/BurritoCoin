// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miningreadiness.h>

#include <net.h>
#include <node/context.h>
#include <validation.h>

#include <algorithm>

MiningReadiness AssessMiningReadiness(int tip_height, const std::vector<int>& peer_announced)
{
    if (peer_announced.empty()) return MiningReadiness::NO_PEERS;
    const int best_peer_height = *std::max_element(peer_announced.begin(), peer_announced.end());
    if (best_peer_height <= 0 || tip_height < best_peer_height) return MiningReadiness::SYNCING;
    return MiningReadiness::READY;
}

MiningReadiness CheckMiningReadiness(const NodeContext& node)
{
    int tip_height;
    {
        LOCK(cs_main);
        tip_height = node.chainman->ActiveChain().Height();
    }

    std::vector<int> peer_announced;
    if (node.connman) {
        std::vector<CNodeStats> stats;
        node.connman->GetNodeStats(stats);
        for (const CNodeStats& peer : stats) {
            // nStartingHeight stays -1 until the peer's version message
            // arrives; until then it isn't a connection to count.
            if (peer.nStartingHeight >= 0) peer_announced.push_back(peer.nStartingHeight);
        }
    }
    return AssessMiningReadiness(tip_height, peer_announced);
}
