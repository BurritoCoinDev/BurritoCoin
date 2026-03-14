// Copyright (c) 2024 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Core unit tests for BurritoCoin-specific monetary policy, chain parameters,
 * and consensus rules.
 *
 * These tests verify the constants and logic that are unique to BurritoCoin
 * and are not covered by the generic upstream test suite.
 */

#include <amount.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <validation.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(burritocoin_tests, BasicTestingSetup)

// ---------------------------------------------------------------------------
// MAX_MONEY and COIN constants
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(max_money_is_21_billion_brto)
{
    // BurritoCoin hard cap: 21,000,000,000 BRTO expressed in burrioshi.
    BOOST_CHECK_EQUAL(MAX_MONEY, CAmount(21000000000LL) * COIN);
}

BOOST_AUTO_TEST_CASE(coin_is_100_million_burrioshi)
{
    // 1 BRTO == 100,000,000 burrioshi (8 decimal places).
    BOOST_CHECK_EQUAL(COIN, CAmount(100000000));
}

BOOST_AUTO_TEST_CASE(money_range_boundaries)
{
    BOOST_CHECK(!MoneyRange(CAmount(-1)));
    BOOST_CHECK( MoneyRange(CAmount(0)));
    BOOST_CHECK( MoneyRange(CAmount(1)));
    BOOST_CHECK( MoneyRange(MAX_MONEY));
    BOOST_CHECK(!MoneyRange(MAX_MONEY + CAmount(1)));
}

// ---------------------------------------------------------------------------
// GetBlockSubsidy – genesis premine and per-block reward
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(genesis_block_subsidy_is_148_million_brto)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // Block 0 must carry the entire 148,000,000 BRTO premine.
    CAmount genesis_subsidy = GetBlockSubsidy(0, consensus);
    BOOST_CHECK_EQUAL(genesis_subsidy, CAmount(148000000) * COIN);
}

BOOST_AUTO_TEST_CASE(first_mined_block_subsidy_is_10_brto)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // Regular mining starts at block 1: reward is 10 BRTO.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, consensus), CAmount(10) * COIN);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2, consensus), CAmount(10) * COIN);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1000, consensus), CAmount(10) * COIN);
}

BOOST_AUTO_TEST_CASE(subsidy_halves_after_halving_interval)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    int64_t interval = consensus.nSubsidyHalvingInterval;

    // Last block before first halving: still 10 BRTO.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(interval - 1, consensus), CAmount(10) * COIN);

    // First block of second era: 5 BRTO.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(interval, consensus), CAmount(5) * COIN);

    // Second halving: 2.5 BRTO (250000000 burrioshi).
    BOOST_CHECK_EQUAL(GetBlockSubsidy(interval * 2, consensus), CAmount(250000000));

    // Third halving: 1.25 BRTO (125000000 burrioshi).
    BOOST_CHECK_EQUAL(GetBlockSubsidy(interval * 3, consensus), CAmount(125000000));
}

BOOST_AUTO_TEST_CASE(subsidy_is_zero_after_64_halvings)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    int64_t interval = consensus.nSubsidyHalvingInterval;

    // After 64 halvings the right-shift is undefined; implementation must
    // return 0 to avoid UB.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(interval * 64, consensus), CAmount(0));
    BOOST_CHECK_EQUAL(GetBlockSubsidy(interval * 100, consensus), CAmount(0));
}

// ---------------------------------------------------------------------------
// Halving interval constant
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(halving_interval_is_1042600000)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(consensus.nSubsidyHalvingInterval, 1042600000);
}

// ---------------------------------------------------------------------------
// Proof-of-work parameters
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pow_target_spacing_is_150_seconds)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // 2.5 minutes == 150 seconds.
    BOOST_CHECK_EQUAL(consensus.nPowTargetSpacing, 150);
}

BOOST_AUTO_TEST_CASE(pow_target_timespan_is_302400_seconds)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // 3.5 days == 302,400 seconds.
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan, 302400);
}

BOOST_AUTO_TEST_CASE(pow_target_timespan_divisible_by_spacing)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);
}

BOOST_AUTO_TEST_CASE(pow_limit_matches_genesis_nbits)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // The genesis block nBits must not exceed powLimit.
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg);
    BOOST_CHECK(!over);
    BOOST_CHECK(pow_compact != 0);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);
}

// ---------------------------------------------------------------------------
// Soft-fork activation heights (all enforced from genesis on BurritoCoin)
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(softforks_active_from_genesis)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(consensus.BIP16Height,  0);
    BOOST_CHECK_EQUAL(consensus.BIP34Height,  0);
    BOOST_CHECK_EQUAL(consensus.BIP65Height,  0);
    BOOST_CHECK_EQUAL(consensus.BIP66Height,  0);
    BOOST_CHECK_EQUAL(consensus.CSVHeight,    0);
    BOOST_CHECK_EQUAL(consensus.SegwitHeight, 0);
}

// ---------------------------------------------------------------------------
// Regtest-specific checks (fast mining for tests)
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(regtest_pow_no_retargeting)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK(consensus.fPowNoRetargeting);
    BOOST_CHECK(consensus.fPowAllowMinDifficultyBlocks);
}

BOOST_AUTO_TEST_CASE(regtest_genesis_subsidy_is_148_million_brto)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensus), CAmount(148000000) * COIN);
}

BOOST_AUTO_TEST_CASE(regtest_first_block_subsidy_is_10_brto)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, consensus), CAmount(10) * COIN);
}

BOOST_AUTO_TEST_SUITE_END()
