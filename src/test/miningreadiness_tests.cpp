// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miningreadiness.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(miningreadiness_tests, BasicTestingSetup)

static const bool MAINNET = false;
static const bool TESTCHAIN = true;

// The case this replaces IsInitialBlockDownload() for: nobody has mined for a
// day, so IBD is true, yet the node is fully caught up with its peer. The peer
// sent no new headers (there were none), so only its announced height vouches
// for the chain — and that is enough.
BOOST_AUTO_TEST_CASE(synced_on_a_quiet_chain_may_mine)
{
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 10578, 10578, {10578}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(ahead_of_peers_may_mine)
{
    // This node mined blocks its peers haven't caught up with yet.
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 10600, 10600, {10578}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(no_outbound_peer_refuses)
{
    // Disconnected: mining now would build a chain nobody else follows.
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 10578, 10578, {}) == MiningReadiness::NO_PEERS);
}

BOOST_AUTO_TEST_CASE(fresh_node_before_headers_refuses)
{
    // Connected to the seed but nothing downloaded yet.
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 0, 0, {10578}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(headers_ahead_of_blocks_refuses)
{
    // Knows the chain exists, still downloading the blocks.
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 5000, 10578, {10578}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(behind_best_peer_refuses)
{
    // One peer is further ahead than this node.
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 10578, 10578, {10578, 10590}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(only_fresh_peers_refuses)
{
    // Every peer is itself at genesis: nobody has shown a real chain, so
    // mining from genesis would fork off the network.
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 0, 0, {0}) == MiningReadiness::SYNCING);
    BOOST_CHECK(AssessMiningReadiness(MAINNET, 0, 0, {0, -1}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(test_chains_skip_peer_checks)
{
    // Like getblocktemplate: a lone regtest node can mine.
    BOOST_CHECK(AssessMiningReadiness(TESTCHAIN, 0, 0, {}) == MiningReadiness::READY);
    BOOST_CHECK(AssessMiningReadiness(TESTCHAIN, 0, 10, {10}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_SUITE_END()
