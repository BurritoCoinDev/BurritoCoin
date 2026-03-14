// Copyright (c) 2014-2019 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <net.h>
#include <signet.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    int maxHalvings = 64;
    // BurritoCoin regular block reward: 10 BRTO.
    // Height 0 is special (genesis premine) and tested separately.
    CAmount nInitialSubsidy = 10 * COIN;

    // nPreviousSubsidy starts at 2× so the first loop iteration (halvings=0)
    // verifies: nSubsidy(1) == nInitialSubsidy == nPreviousSubsidy/2.
    CAmount nPreviousSubsidy = nInitialSubsidy * 2;
    for (int nHalvings = 0; nHalvings < maxHalvings; nHalvings++) {
        // Add 1 to avoid height 0 (genesis premine special case).
        int64_t nHeight = (int64_t)nHalvings * consensusParams.nSubsidyHalvingInterval + 1;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= nInitialSubsidy);
        BOOST_CHECK_EQUAL(nSubsidy, nPreviousSubsidy / 2);
        nPreviousSubsidy = nSubsidy;
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy((int64_t)maxHalvings * consensusParams.nSubsidyHalvingInterval + 1, consensusParams), 0);
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    TestBlockSubsidyHalvings(chainParams->GetConsensus()); // As in main
    TestBlockSubsidyHalvings(150); // As in regtest
    TestBlockSubsidyHalvings(1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    // Genesis block carries the 148,000,000 BRTO premine.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, params), 148000000 * COIN);

    // Regular blocks: 10 BRTO each; no halvings occur within the first 56,000,000
    // blocks (halving interval is 1,042,600,000 blocks, ~4,960 years).
    // Start at height 1000 to avoid the genesis special case; accumulate
    // nSubsidy * 1000 per step (each step represents 1000 blocks).
    CAmount nSum = 0;
    for (int nHeight = 1000; nHeight < 56000000; nHeight += 1000) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, params);
        BOOST_CHECK(nSubsidy <= 10 * COIN);
        nSum += nSubsidy * 1000;
        BOOST_CHECK(MoneyRange(nSum));
    }
    // 55,999 steps × 10 COIN × 1000 blocks = 559,990,000 COIN in burrioshi.
    BOOST_CHECK_EQUAL(nSum, CAmount{559990000LL * COIN});
}

BOOST_AUTO_TEST_SUITE_END()
