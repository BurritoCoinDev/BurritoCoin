// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miningreadiness.h>

#include <net.h>
#include <net_processing.h>
#include <node/context.h>
#include <util/time.h>
#include <validation.h>

#include <algorithm>

MiningReadiness AssessMiningReadiness(int tip_height, int best_header_height,
                                      const std::vector<PeerHeight>& outbound_peers)
{
    if (outbound_peers.empty()) return MiningReadiness::NO_PEERS;
    int best_peer_height = -1;
    for (const PeerHeight& peer : outbound_peers) {
        int announced = peer.announced;
        if (peer.seconds_connected >= ANNOUNCED_HEIGHT_TRUST_SECONDS) {
            announced = std::min(announced, best_header_height);
        }
        best_peer_height = std::max({best_peer_height, announced, peer.shown});
    }
    if (best_peer_height <= 0 || tip_height < best_peer_height) return MiningReadiness::SYNCING;
    return MiningReadiness::READY;
}

MiningReadiness CheckMiningReadiness(const NodeContext& node)
{
    int tip_height;
    int best_header_height;
    {
        LOCK(cs_main);
        tip_height = node.chainman->ActiveChain().Height();
        best_header_height = pindexBestHeader ? pindexBestHeader->nHeight : tip_height;
    }

    std::vector<PeerHeight> outbound_peers;
    if (node.connman) {
        std::vector<CNodeStats> stats;
        node.connman->GetNodeStats(stats);
        const int64_t now = GetSystemTimeInSeconds(); // the clock nTimeConnected uses
        for (const CNodeStats& peer : stats) {
            if (peer.fInbound) continue;
            CNodeStateStats state;
            const int shown = GetNodeStateStats(peer.nodeid, state) ? state.nSyncHeight : -1;
            outbound_peers.push_back({peer.nStartingHeight, shown, now - peer.nTimeConnected});
        }
    }
    return AssessMiningReadiness(tip_height, best_header_height, outbound_peers);
}
