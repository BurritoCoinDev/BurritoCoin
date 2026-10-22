// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miningreadiness.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(miningreadiness_tests, BasicTestingSetup)

static const int64_t JUST_CONNECTED = 5;
static const int64_t LONG_CONNECTED = ANNOUNCED_HEIGHT_TRUST_SECONDS;

// The case this replaces IsInitialBlockDownload() for: nobody has mined for a
// day, so IBD is true, yet the node is fully caught up with its peer.
BOOST_AUTO_TEST_CASE(synced_on_a_quiet_chain_may_mine)
{
    // The sync peer answered our getheaders with its tip.
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, {{10578, 10578, LONG_CONNECTED}}) == MiningReadiness::READY);
    // A peer we never synced from: no headers, only its announced height.
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, {{10578, -1, JUST_CONNECTED}}) == MiningReadiness::READY);
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, {{10578, -1, LONG_CONNECTED}}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(ahead_of_peers_may_mine)
{
    // This node mined blocks its peers haven't caught up with yet.
    BOOST_CHECK(AssessMiningReadiness(10600, 10600, {{10578, 10578, LONG_CONNECTED}}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(no_outbound_peer_refuses)
{
    // Disconnected: mining now would build a chain nobody else follows.
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, {}) == MiningReadiness::NO_PEERS);
}

BOOST_AUTO_TEST_CASE(fresh_node_before_headers_refuses)
{
    // Connected to the seed a moment ago, nothing downloaded yet.
    BOOST_CHECK(AssessMiningReadiness(0, 0, {{10578, -1, JUST_CONNECTED}}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(reopened_behind_before_headers_refuses)
{
    // Reopened after others mined: tip and headers both stop at 5000 until
    // the sync peer answers, so only the announced height says we're behind.
    BOOST_CHECK(AssessMiningReadiness(5000, 5000, {{10578, -1, JUST_CONNECTED}}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(downloading_blocks_refuses)
{
    // Headers are in, blocks are still downloading.
    BOOST_CHECK(AssessMiningReadiness(5000, 10578, {{10578, 10578, JUST_CONNECTED}}) == MiningReadiness::SYNCING);
    // Still true once the announced height is no longer taken on trust,
    // because we have seen headers up to it.
    BOOST_CHECK(AssessMiningReadiness(5000, 10578, {{10578, -1, LONG_CONNECTED}}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(behind_best_peer_refuses)
{
    // One peer has shown blocks this node doesn't have.
    BOOST_CHECK(AssessMiningReadiness(10578, 10590, {{10578, 10578, LONG_CONNECTED}, {10578, 10590, LONG_CONNECTED}}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(only_fresh_peers_refuses)
{
    // Every peer is itself at genesis: nobody has shown a real chain, so
    // mining from genesis would fork off the network.
    BOOST_CHECK(AssessMiningReadiness(0, 0, {{0, -1, JUST_CONNECTED}}) == MiningReadiness::SYNCING);
    BOOST_CHECK(AssessMiningReadiness(0, 0, {{0, 0, LONG_CONNECTED}, {-1, -1, LONG_CONNECTED}}) == MiningReadiness::SYNCING);
}

BOOST_AUTO_TEST_CASE(unproven_height_is_trusted_only_for_a_while)
{
    // A peer announcing a height nobody has shown headers for.
    const std::vector<PeerHeight> liar_new{{10578, 10578, LONG_CONNECTED}, {99999999, -1, JUST_CONNECTED}};
    const std::vector<PeerHeight> liar_old{{10578, 10578, LONG_CONNECTED}, {99999999, -1, LONG_CONNECTED}};
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, liar_new) == MiningReadiness::SYNCING);
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, liar_old) == MiningReadiness::READY);
    // The boundary: trusted for exactly ANNOUNCED_HEIGHT_TRUST_SECONDS.
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, {{10579, -1, ANNOUNCED_HEIGHT_TRUST_SECONDS - 1}}) == MiningReadiness::SYNCING);
    BOOST_CHECK(AssessMiningReadiness(10578, 10578, {{10579, -1, ANNOUNCED_HEIGHT_TRUST_SECONDS}}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_CASE(header_without_a_peer_does_not_block)
{
    // The best header is above the tip, but no connected peer has that block:
    // it was invalid, or its sender withheld it and was disconnected for
    // stalling. Refusing here would refuse until restart, or for good.
    BOOST_CHECK(AssessMiningReadiness(10578, 10579, {{10578, 10578, LONG_CONNECTED}}) == MiningReadiness::READY);
}

BOOST_AUTO_TEST_SUITE_END()
