// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miningreadiness.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(miningreadiness_tests, BasicTestingSetup)

// The case this replaces IsInitialBlockDownload() for: nobody has mined for a
// day, so IBD is true, yet the node has everything its peer announced.
BOOST_AUTO_TEST_CASE(synced_on_a_quiet_chain_may_mine)
{
    BOOST_CHECK(AssessMiningReadiness(10578, {10578}) == MiningReadiness::READY);
    BOOST_CHECK(AssessMiningReadiness(10578, {10578, 10578, 9000}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(ahead_of_peers_may_mine)
{
    // This node mined blocks since its peers connected.
    BOOST_CHECK(AssessMiningReadiness(10600, {10578}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(no_outbound_peer_refuses)
{
    // Disconnected: mining now would build a chain nobody else follows.
    BOOST_CHECK(AssessMiningReadiness(10578, {}) == MiningReadiness::NO_PEERS);
}

BOOST_AUTO_TEST_CASE(fresh_node_refuses)
{
    // Connected to the seed, nothing downloaded yet.
    BOOST_CHECK(AssessMiningReadiness(0, {10578}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(reopened_behind_refuses)
{
    // Reopened after others mined: still downloading what the peer announced.
    BOOST_CHECK(AssessMiningReadiness(5000, {10578}) == MiningReadiness::SYNCING);
    BOOST_CHECK(AssessMiningReadiness(10577, {10578}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(behind_any_peer_refuses)
{
    // One peer is further ahead than this node and the others.
    BOOST_CHECK(AssessMiningReadiness(10578, {10578, 10590}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(only_fresh_peers_refuses)
{
    // Every peer is itself at genesis: nobody has shown a chain, so mining
    // from genesis would fork off the network.
    BOOST_CHECK(AssessMiningReadiness(0, {0}) == MiningReadiness::SYNCING);
    BOOST_CHECK(AssessMiningReadiness(0, {0, 0}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_SUITE_END()
