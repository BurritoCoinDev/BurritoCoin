// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_NODE_MININGREADINESS_H
#define BURRITOCOIN_NODE_MININGREADINESS_H

#include <vector>

struct NodeContext;

/** Whether a node is caught up enough to mine on top of its chain tip. */
enum class MiningReadiness {
    READY,    //!< connected, and at least as high as every peer
    NO_PEERS, //!< no connection, so it would mine a private chain
    SYNCING,  //!< below a peer's height, or no peer has a chain yet
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
 *  - at least one peer that has completed its handshake. Without one, it
 *    would mine a chain nobody else follows.
 *  - its tip is at least as high as every peer's height, where a peer's
 *    height is the one it announced in its version message: its own
 *    validated tip when the connection opened. A fresh or reopened node is
 *    behind its peers' announcements until it has downloaded their blocks.
 *    On a quiet chain a synced node equals them, and stays mineable.
 *  - some peer announced a height above 0. Otherwise nobody has shown a
 *    chain yet, and a node that has only met other fresh nodes would start
 *    mining from genesis.
 * Inbound peers count the same as outbound ones. A reachable node may be
 * connected to the seed only inbound, because it won't dial an address it
 * is already connected to; ignoring inbound peers would leave it "not
 * connected", or judging itself only against whichever outbound peers it
 * happens to have.
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
 * not have stops this node mining for as long as it stays connected. The
 * failure is a refusal rather than a block on a stale chain, but on a network
 * this small most nodes may meet the same such peer, and meet it again after
 * a restart. The remedy is to ban it: the Peers window shows each peer's
 * Starting Block, and its context menu bans one.
 *
 * @param[in] tip_height        active chain tip height
 * @param[in] peer_announced    nStartingHeight of each peer past its handshake
 */
MiningReadiness AssessMiningReadiness(int tip_height, const std::vector<int>& peer_announced);

/** AssessMiningReadiness() for the running node's current chain and peers. */
MiningReadiness CheckMiningReadiness(const NodeContext& node);

#endif // BURRITOCOIN_NODE_MININGREADINESS_H
