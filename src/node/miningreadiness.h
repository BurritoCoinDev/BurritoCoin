// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_NODE_MININGREADINESS_H
#define BURRITOCOIN_NODE_MININGREADINESS_H

#include <algorithm>
#include <vector>

/** Whether a node is caught up enough to mine on top of its chain tip. */
enum class MiningReadiness {
    READY,    //!< connected, and caught up with what its peers have
    NO_PEERS, //!< no outbound connection, so it would mine a private chain
    SYNCING,  //!< still downloading, or behind what a peer says it has
};

/**
 * Decide whether mining should be allowed, without IsInitialBlockDownload().
 *
 * IBD also turns true whenever the newest block is older than -maxtipage
 * (24 hours by default). On a small chain that happens every time nobody has
 * mined for a day, so an IBD gate refuses to mine exactly the block that would
 * end the quiet, however well synced the node is. What mining actually needs
 * is narrower:
 *
 *  - at least one outbound peer. Outbound peers are ones this node chose;
 *    without any, it would mine a chain nobody else follows.
 *  - every header it knows about has been connected as a block.
 *  - its tip is at least as high as the best outbound peer's height. A peer's
 *    height is the larger of the height it announced in its version message
 *    and the best block it has shown since. The announced height matters: on
 *    a quiet chain a synced node receives no new headers, so the shown height
 *    alone would never prove it caught up.
 *
 * A peer height of 0 or less means no peer has shown a chain yet; that is
 * treated as still syncing, so a node that has only ever met other fresh
 * nodes can't start mining from genesis.
 *
 * Test chains skip the peer checks, as getblocktemplate does, so a single
 * regtest node can still mine.
 *
 * @param[in] is_test_chain          Params().IsTestChain()
 * @param[in] tip_height             active chain tip height
 * @param[in] best_header_height     height of the best header known
 * @param[in] outbound_peer_heights  one entry per outbound peer, as above
 */
inline MiningReadiness AssessMiningReadiness(bool is_test_chain, int tip_height, int best_header_height,
                                             const std::vector<int>& outbound_peer_heights)
{
    if (is_test_chain) return MiningReadiness::READY;
    if (outbound_peer_heights.empty()) return MiningReadiness::NO_PEERS;
    if (best_header_height > tip_height) return MiningReadiness::SYNCING;
    const int best_peer_height = *std::max_element(outbound_peer_heights.begin(), outbound_peer_heights.end());
    if (best_peer_height <= 0 || tip_height < best_peer_height) return MiningReadiness::SYNCING;
    return MiningReadiness::READY;
}

#endif // BURRITOCOIN_NODE_MININGREADINESS_H
