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
#include <util/moneystr.h>
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

// ---------------------------------------------------------------------------
// Network message-start magic ("BRTO" / "BRTN" / "BRTG")
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(mainnet_message_start_is_BRTO)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const CMessageHeader::MessageStartChars& magic = chainParams->MessageStart();

    // The four bytes spell out B-R-T-O in ASCII.
    BOOST_CHECK_EQUAL(magic[0], 0x42); // 'B'
    BOOST_CHECK_EQUAL(magic[1], 0x52); // 'R'
    BOOST_CHECK_EQUAL(magic[2], 0x54); // 'T'
    BOOST_CHECK_EQUAL(magic[3], 0x4f); // 'O'
}

BOOST_AUTO_TEST_CASE(testnet_message_start_differs_from_mainnet)
{
    const auto mainParams    = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto testnetParams = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);

    const CMessageHeader::MessageStartChars& mainMagic    = mainParams->MessageStart();
    const CMessageHeader::MessageStartChars& testnetMagic = testnetParams->MessageStart();

    // First three bytes are shared (B-R-T); only the fourth differs.
    BOOST_CHECK_EQUAL(testnetMagic[0], mainMagic[0]);
    BOOST_CHECK_EQUAL(testnetMagic[1], mainMagic[1]);
    BOOST_CHECK_EQUAL(testnetMagic[2], mainMagic[2]);
    BOOST_CHECK(testnetMagic[3] != mainMagic[3]);

    // Testnet fourth byte is 'N' (0x4e).
    BOOST_CHECK_EQUAL(testnetMagic[3], 0x4e);
}

BOOST_AUTO_TEST_CASE(regtest_message_start_differs_from_mainnet)
{
    const auto mainParams   = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto regtestParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);

    const CMessageHeader::MessageStartChars& mainMagic    = mainParams->MessageStart();
    const CMessageHeader::MessageStartChars& regtestMagic = regtestParams->MessageStart();

    // Regtest fourth byte is 'G' (0x47).
    BOOST_CHECK_EQUAL(regtestMagic[0], mainMagic[0]);
    BOOST_CHECK_EQUAL(regtestMagic[1], mainMagic[1]);
    BOOST_CHECK_EQUAL(regtestMagic[2], mainMagic[2]);
    BOOST_CHECK_EQUAL(regtestMagic[3], 0x47); // 'G'
}

// ---------------------------------------------------------------------------
// Default P2P port numbers
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(mainnet_default_port_is_9227)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 9227);
}

BOOST_AUTO_TEST_CASE(testnet_default_port_is_19227)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 19227);
}

BOOST_AUTO_TEST_CASE(regtest_default_port_is_19554)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 19554);
}

// ---------------------------------------------------------------------------
// Miner confirmation window and rule-change activation threshold
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(mainnet_miner_confirmation_window_is_8064)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // 8064 = (nPowTargetTimespan / nPowTargetSpacing) * 4
    BOOST_CHECK_EQUAL(consensus.nMinerConfirmationWindow, 8064);
}

BOOST_AUTO_TEST_CASE(mainnet_rule_change_threshold_is_75_percent)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // 75% of 8064 = 6048
    BOOST_CHECK_EQUAL(consensus.nRuleChangeActivationThreshold, 6048);
    BOOST_CHECK_EQUAL(consensus.nRuleChangeActivationThreshold * 4,
                      consensus.nMinerConfirmationWindow * 3);
}

BOOST_AUTO_TEST_CASE(confirmation_window_equals_four_retarget_periods)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    int64_t retarget_blocks = consensus.nPowTargetTimespan / consensus.nPowTargetSpacing;
    BOOST_CHECK_EQUAL(consensus.nMinerConfirmationWindow, retarget_blocks * 4);
}

// ---------------------------------------------------------------------------
// Taproot and MWEB deployment timeout heights
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(taproot_active_from_genesis_on_mainnet)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // Taproot uses BIP9 ALWAYS_ACTIVE (-1); verify via the time-based start field.
    // Use a local copy to avoid ODR-use of the static constexpr member.
    const int64_t always_active = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
    BOOST_CHECK_EQUAL(
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime,
        always_active);
}

BOOST_AUTO_TEST_CASE(taproot_is_always_active_on_mainnet)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // Taproot is a core BurritoCoin feature deployed via BIP9 ALWAYS_ACTIVE
    // (not a height-based BIP8 timeout). Use local copies to avoid ODR-use.
    const int64_t always_active = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
    const int64_t no_timeout    = Consensus::BIP9Deployment::NO_TIMEOUT;
    BOOST_CHECK_EQUAL(
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime,
        always_active);
    BOOST_CHECK_EQUAL(
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout,
        no_timeout);
}

BOOST_AUTO_TEST_CASE(mweb_active_from_genesis_on_mainnet)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartHeight, 0);
}

BOOST_AUTO_TEST_CASE(mweb_timeout_height_is_one_window)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // MWEB uses BIP8 height-based activation. nTimeoutHeight forces lock-in at
    // the end of the first confirmation window (1 * nMinerConfirmationWindow = 8064).
    BOOST_CHECK_EQUAL(
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeoutHeight,
        consensus.nMinerConfirmationWindow);
}

// ---------------------------------------------------------------------------
// Mainnet Base58 address prefixes
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(mainnet_pubkey_address_prefix_produces_B_addresses)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);

    // Version byte 25 → base58check addresses starting with 'B' (BurritoCoin-specific).
    const auto& prefix = chainParams->Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    BOOST_REQUIRE_EQUAL(prefix.size(), 1u);
    BOOST_CHECK_EQUAL(prefix[0], 25);
}

BOOST_AUTO_TEST_CASE(mainnet_script_address_prefix_is_5)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);

    const auto& prefix = chainParams->Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    BOOST_REQUIRE_EQUAL(prefix.size(), 1u);
    BOOST_CHECK_EQUAL(prefix[0], 5);
}

BOOST_AUTO_TEST_CASE(mainnet_secret_key_prefix_is_153)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);

    // WIF private key prefix 153 → compressed keys starting with 'P' (BurritoCoin-specific).
    const auto& prefix = chainParams->Base58Prefix(CChainParams::SECRET_KEY);
    BOOST_REQUIRE_EQUAL(prefix.size(), 1u);
    BOOST_CHECK_EQUAL(prefix[0], 153);
}

// ---------------------------------------------------------------------------
// FormatMoney / ParseMoney with BRTO-scale amounts
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(format_money_premine_amount)
{
    // 148,000,000 BRTO should format without scientific notation.
    CAmount premine = CAmount(148000000) * COIN;
    BOOST_CHECK_EQUAL(FormatMoney(premine), "148000000.00");
}

BOOST_AUTO_TEST_CASE(format_money_max_supply)
{
    // Full 21,000,000,000 BRTO supply.
    BOOST_CHECK_EQUAL(FormatMoney(MAX_MONEY), "21000000000.00");
}

BOOST_AUTO_TEST_CASE(parse_money_max_supply_roundtrip)
{
    CAmount parsed = 0;
    BOOST_CHECK(ParseMoney("21000000000.00", parsed));
    BOOST_CHECK_EQUAL(parsed, MAX_MONEY);
}

BOOST_AUTO_TEST_CASE(parse_money_rejects_above_max_supply)
{
    CAmount parsed = 0;
    // One burrioshi above the hard cap must be rejected.
    BOOST_CHECK(!ParseMoney("21000000000.00000001", parsed));
}

BOOST_AUTO_TEST_CASE(parse_money_rejects_12_digit_whole_part)
{
    CAmount parsed = 0;
    // 12-digit whole part exceeds the 11-digit guard in moneystr.cpp.
    BOOST_CHECK(!ParseMoney("100000000000.00", parsed));
}

BOOST_AUTO_TEST_CASE(format_parse_money_block_reward_roundtrip)
{
    // 10 BRTO block reward.
    CAmount reward = CAmount(10) * COIN;
    CAmount parsed = 0;
    BOOST_CHECK(ParseMoney(FormatMoney(reward), parsed));
    BOOST_CHECK_EQUAL(parsed, reward);
}

// ---------------------------------------------------------------------------
// nMinimumChainWork is zero on a new chain
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(mainnet_minimum_chain_work_is_zero)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = chainParams->GetConsensus();

    // New chain: no accumulated work yet.
    BOOST_CHECK(consensus.nMinimumChainWork == uint256S("0x00"));
}

BOOST_AUTO_TEST_SUITE_END()
