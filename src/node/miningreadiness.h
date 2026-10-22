// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_NODE_MININGREADINESS_H
#define BURRITOCOIN_NODE_MININGREADINESS_H

#include <cstdint>
#include <vector>

struct NodeContext;

/** Whether a node is caught up enough to mine on top of its chain tip. */
enum class MiningReadiness {
    READY,    //!< connected, and caught up with what its peers have
    NO_PEERS, //!< no outbound connection, so it would mine a private chain
    SYNCING,  //!< behind what an outbound peer has, or no peer has a chain yet
};

/** What one outbound peer has told us about its chain. */
struct PeerHeight {
    int announced;            //!< height from its version message (CNodeStats::nStartingHeight)
    int shown;                //!< best block it has sent us headers for, -1 if none (CNodeStateStats::nSyncHeight)
    int64_t seconds_connected;
};

/**
 * How long a peer's announced height is believed on its word alone. Longer
 * than the 15-minute headers-sync timeout in net_processing, so by the time it
 * runs out an honest chain has reached us as headers from some peer.
 */
static constexpr int64_t ANNOUNCED_HEIGHT_TRUST_SECONDS{20 * 60};

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
 *    without any, it would mine a chain nobody else follows. Inbound peers
 *    are ignored throughout, because a stranger can connect, claim any
 *    height, and reconnect to claim it again.
 *  - its tip is at least as high as every outbound peer's height.
 *
 * A peer's height is the best block it has shown us headers for, or the
 * height it announced in its version message if that is higher. Shown blocks
 * are proven by proof of work. The announced height is not, but it is needed:
 * a fresh node must not mine in the seconds before the first headers arrive,
 * and on a quiet chain a synced node's non-sync peers send no headers at all.
 * So an announced height is taken on trust only for the first
 * ANNOUNCED_HEIGHT_TRUST_SECONDS of a connection. After that it counts only
 * as far as headers this node has actually seen, which means a peer lying
 * about its height can delay mining but not block it indefinitely.
 *
 * If no outbound peer has a height above 0, nobody has shown a chain yet; that
 * is treated as still syncing, so a node that has only ever met other fresh
 * nodes can't start mining from genesis.
 *
 * There is deliberately no "best header above the tip" test. The best-header
 * pointer is not rewound when a block turns out invalid, and a header can
 * arrive without its block ever following, so that test would refuse to mine
 * until restart (or for good) after one bad block. A peer that does have the
 * blocks shows them as headers, which the peer test already covers.
 *
 * @param[in] tip_height          active chain tip height
 * @param[in] best_header_height  height of the best header known
 * @param[in] outbound_peers      one entry per outbound peer
 */
MiningReadiness AssessMiningReadiness(int tip_height, int best_header_height,
                                      const std::vector<PeerHeight>& outbound_peers);

/** AssessMiningReadiness() for the running node's current chain and peers. */
MiningReadiness CheckMiningReadiness(const NodeContext& node);

#endif // BURRITOCOIN_NODE_MININGREADINESS_H
