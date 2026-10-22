// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_NODE_MININGREADINESS_H
#define BURRITOCOIN_NODE_MININGREADINESS_H

#include <vector>

struct NodeContext;

/** Whether a node is caught up enough to mine on top of its chain tip. */
enum class MiningReadiness {
    READY,    //!< connected, and at least as high as every outbound peer
    NO_PEERS, //!< no outbound connection, so it would mine a private chain
    SYNCING,  //!< below an outbound peer's height, or no peer has a chain yet
};

/**
 * Decide whether mining should be allowed, without IsInitialBlockDownload().
 *
 * IBD is true from startup until a block less than -maxtipage old (24 hours by
 * default) arrives. On a small chain nobody may mine for days, and then every
 * node that starts up stays in IBD, so an IBD gate refuses to mine exactly the
 * block that would end the quiet, however well synced the node is. What
 * mining actually needs is narrower:
 *
 *  - at least one outbound peer. Outbound peers are ones this node chose;
 *    without any, it would mine a chain nobody else follows. Inbound peers
 *    are ignored, because anyone can connect and claim anything.
 *  - its tip is at least as high as every outbound peer's height, where a
 *    peer's height is the one it announced in its version message: its own
 *    validated tip when the connection opened. A fresh or reopened node is
 *    behind its peers' announcements until it has downloaded their blocks.
 *    On a quiet chain a synced node equals them, and stays mineable.
 *  - some outbound peer announced a height above 0. Otherwise nobody has
 *    shown a chain yet, and a node that has only met other fresh nodes would
 *    start mining from genesis.
 *
 * Signals deliberately not used, because each one can stop every wallet
 * mining after a single bad block, until restart or for good:
 *  - the best known header. It is not rewound for a block that fails before
 *    it is connected, nor for a header whose block never arrives.
 *  - the best block a peer has shown since connecting. Compact-block relay
 *    forwards a block before it is fully validated and does not punish the
 *    relayer when it turns out invalid, so that height can be a block every
 *    node has rejected.
 * Nor does an announcement expire after a while: on a quiet chain the other
 * nodes are in IBD and ignore getheaders, so a node that is really behind may
 * never hear the headers that would prove it.
 *
 * What remains trusted is the peer's word. A peer announcing a height it does
 * not have stops this node mining for as long as it stays connected. That is
 * a peer this node chose, the failure is a refusal rather than a block on a
 * stale chain, and restarting the wallet picks peers afresh.
 *
 * @param[in] tip_height                active chain tip height
 * @param[in] outbound_peer_announced   nStartingHeight of each outbound peer
 */
MiningReadiness AssessMiningReadiness(int tip_height, const std::vector<int>& outbound_peer_announced);

/** AssessMiningReadiness() for the running node's current chain and peers. */
MiningReadiness CheckMiningReadiness(const NodeContext& node);

#endif // BURRITOCOIN_NODE_MININGREADINESS_H
