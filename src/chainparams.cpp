// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <hash.h> // for signet block challenge hash
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <arith_uint256.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. The genesis coinbase IS spendable — it pays to a
 * P2PK output whose private key is held by the project founders. Its outputs
 * are added to the UTXO set when the block is connected.
 *
 * BurritoCoin mainnet genesis block (scrypt PoW, mined 2026-04-11):
 *   nTime       = 1773844916
 *   nNonce      = 1958489
 *   nBits       = 0x1e0ffff0
 *   PoW Hash    = 000001a63fd5f6448e30f1708d19c15c32cee5bb7aeffdd69eca02452e2db11e
 *   Block Hash  = 44615751d966cf772a051f65b8df4f3987adc48be1749a699369a18517418dce
 *   Merkle Root = d347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83
 *
 * The 148,000,000 BRTO genesis premine becomes spendable after 100
 * confirmations, like any other coinbase output.
 *
 * To re-mine the genesis nonces, build with -DMINE_GENESIS:
 *   ./configure CXXFLAGS="-DMINE_GENESIS" --without-gui --disable-tests --disable-bench
 *   make -j1
 *   ./src/burritocoind   # prints nNonce, hashes, then exits
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "WSJ 18/Mar/2026 Finance Bros to Tech Bros: Don’t Mess With My Bloomberg Terminal";
    const CScript genesisOutputScript = CScript() << ParseHex("047c70e6f341e7dc32dc92a84435cdd3a845f6d242b01b71b1940fe5174f2c054ce2a54c9ce551f31516512fb3bcf2e87e2e1ddff863a332b91f7c9004a74fe8a9") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // Halving interval chosen so that total mined supply converges to
        // 20,852,000,000 BRTO (10 BRTO/block × 1,042,600,000 × 2),
        // bringing the total supply to 21,000,000,000 BRTO including
        // the 148,000,000 BRTO genesis premine.
        consensus.nSubsidyHalvingInterval = 1042600000;
        // Enforce all pre-Taproot soft forks from genesis on a new chain.
        consensus.BIP16Height = 0;  // P2SH always enforced
        consensus.BIP34Height = 0;  // coinbase height always required
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 0;  // OP_CHECKLOCKTIMEVERIFY always active
        consensus.BIP66Height = 0;  // strict DER always required
        consensus.CSVHeight = 0;    // BIP68/112/113 CSV always active
        consensus.SegwitHeight = 0; // SegWit always active (required for MWEB)
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 6048; // 75% of 8064
        consensus.nMinerConfirmationWindow = 8064; // ~3.5 days at 2.5 min/block
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of Taproot (BIPs 340-342)
        // Taproot is a core BurritoCoin feature active from genesis on all networks.
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of MWEB
        // MWEB is a core BurritoCoin feature. ALWAYS_ACTIVE is intentionally avoided:
        // it mandates a HogEx in every block from genesis, which breaks the standard
        // 100-block test-setup helpers. Instead, signaling starts at height 0 and
        // nTimeoutHeight forces lock-in (BIP8-style mandatory activation) at the end
        // of the first confirmation window (~14 days) if miners have not yet signaled.
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartHeight = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeoutHeight = 8064; // 1 * nMinerConfirmationWindow (8064)

        // New chain: no accumulated work yet; set to zero so the node
        // considers itself synced from genesis and can form a network.
        // BRTO-TODO: update once the chain has significant work.
        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x42; // 'B'
        pchMessageStart[1] = 0x52; // 'R'
        pchMessageStart[2] = 0x54; // 'T'
        pchMessageStart[3] = 0x4f; // 'O'
        nDefaultPort = 9227;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 40;
        m_assumed_chain_state_size = 2;

        // Genesis block carries the 148,000,000 BRTO premine (spendable).
        genesis = CreateGenesisBlock(1773844916, 1958489, 0x1e0ffff0, 1, 148000000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x44615751d966cf772a051f65b8df4f3987adc48be1749a699369a18517418dce"));
        assert(genesis.hashMerkleRoot == uint256S("0xd347dbef904ecdb3653e4eaf2fdcfa7fdc287db36c9e287102b2c757947d7d83"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("dnsseed.burritocointools.com");
        vSeeds.emplace_back("dnsseed.burritocoinpool.org");

        // BurritoCoin-specific prefixes (unique, not shared with Bitcoin or Litecoin):
        //   PUBKEY_ADDRESS = 25  → P2PKH addresses start with 'B'
        //   SECRET_KEY     = 153 → WIF private keys start with 'P' (compressed)
        //   SCRIPT_ADDRESS2 = 28 → Legacy P2SH-2 addresses start with 'C'
        //   EXT_PUBLIC_KEY  0x0188D9CE → HD public keys encode as "Ktub..."
        //   EXT_SECRET_KEY  0x0188D26A → HD private keys encode as "Ktpv..."
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,25);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,28);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,153);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x01, 0x88, 0xD9, 0xCE};
        base58Prefixes[EXT_SECRET_KEY] = {0x01, 0x88, 0xD2, 0x6A};

        bech32_hrp = "brto";
        mweb_hrp = "brtomweb";

        vFixedSeeds.clear(); // No fixed seeds yet — BurritoCoin mainnet has not launched.

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                // No checkpoints yet — BurritoCoin genesis block has not been mined.
                // BRTO-TODO: add checkpoints once the chain is live.
            }
        };

        chainTxData = ChainTxData{
            // BRTO-TODO: update once chain is live.
            /* nTime    */ 0,
            /* nTxCount */ 0,
            /* dTxRate  */ 0.0
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 1042600000;
        // Enforce all pre-Taproot soft forks from genesis on testnet.
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 0;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 0;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of Taproot (BIPs 340-342)
        // Taproot is a core BurritoCoin feature active from genesis on all networks.
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of MWEB
        // Signaling starts at height 0; nTimeoutHeight forces lock-in (BIP8-style
        // mandatory activation) at the end of the first confirmation window (~3.5 days)
        // if miners have not yet signaled.
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartHeight = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeoutHeight = 2016; // 1 * nMinerConfirmationWindow (2016)

        // New chain: no accumulated work yet.
        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0x42; // 'B'
        pchMessageStart[1] = 0x52; // 'R'
        pchMessageStart[2] = 0x54; // 'T'
        pchMessageStart[3] = 0x4e; // 'N' (BurriTo testNet)
        nDefaultPort = 19227;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 4;
        m_assumed_chain_state_size = 1;

        // Genesis block carries the 148,000,000 BRTO testnet premine (spendable).
        genesis = CreateGenesisBlock(1773844917, 0, 0x1e0ffff0, 1, 148000000 * COIN);
#ifdef MINE_GENESIS
        {
            arith_uint256 bnTarget;
            bool fNeg, fOvf;
            bnTarget.SetCompact(genesis.nBits, &fNeg, &fOvf);
            printf("Mining testnet genesis block...\n");
            printf("Merkle Root: %s\n", genesis.hashMerkleRoot.ToString().c_str());
            while (UintToArith256(genesis.GetPoWHash()) > bnTarget) {
                ++genesis.nNonce;
                if (genesis.nNonce % 1000000 == 0)
                    printf("  nNonce=%u ...\n", genesis.nNonce);
            }
            printf("TESTNET GENESIS MINED!\n");
            printf("  nNonce=%u\n", genesis.nNonce);
            printf("  PoW Hash: %s\n", genesis.GetPoWHash().ToString().c_str());
            printf("  Block Hash: %s\n", genesis.GetHash().ToString().c_str());
            printf("  Merkle Root: %s\n", genesis.hashMerkleRoot.ToString().c_str());
            fflush(stdout);
            exit(0);
        }
#endif
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("testnet-seed.burritocointools.com");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tbrto";
        mweb_hrp = "tbrtomweb";

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                // No checkpoints yet — BurritoCoin testnet has not been mined.
                // BRTO-TODO: add checkpoints once testnet is live.
            }
        };

        chainTxData = ChainTxData{
            // BRTO-TODO: update once testnet is live.
            /* nTime    */ 0,
            /* nTxCount */ 0,
            /* dTxRate  */ 0.0
        };
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID =  CBaseChainParams::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 1042600000;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        consensus.CSVHeight = 432; // CSV activated on regtest (Used in rpc activation tests)
        consensus.SegwitHeight = 0; // SEGWIT is always activated on regtest unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 8064 mainnet)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of MWEB
        // Epoch-start time so signaling begins from block 1. ALWAYS_ACTIVE is avoided
        // because it requires a HogEx in every block from genesis, which breaks the
        // standard 100-block test-setup helpers.
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartTime = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x42; // 'B'
        pchMessageStart[1] = 0x52; // 'R'
        pchMessageStart[2] = 0x54; // 'T'
        pchMessageStart[3] = 0x47; // 'G' (BurriTo reGtest)
        nDefaultPort = 19554;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateActivationParametersFromArgs(args);

        genesis = CreateGenesisBlock(1296688602, 0, 0x207fffff, 1, 148000000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rbrto";
        mweb_hrp = "rbrtomweb";
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nStartHeight, int64_t nTimeoutHeight)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
        consensus.vDeployments[d].nStartHeight = nStartHeight;
        consensus.vDeployments[d].nTimeoutHeight = nTimeoutHeight;
    }
    void UpdateActivationParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateActivationParametersFromArgs(const ArgsManager& args)
{
    if (args.IsArgSet("-segwitheight")) {
        int64_t height = args.GetArg("-segwitheight", consensus.SegwitHeight);
        if (height < -1 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Activation height %ld for segwit is out of valid range. Use -1 to disable segwit.", height));
        } else if (height == -1) {
            LogPrintf("Segwit disabled for testing\n");
            height = std::numeric_limits<int>::max();
        }
        consensus.SegwitHeight = static_cast<int>(height);
    }

    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() < 3 || 5 < vDeploymentParams.size()) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end[:heightstart:heightend]");
        }
        int64_t nStartTime, nTimeout, nStartHeight, nTimeoutHeight;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        if (vDeploymentParams.size() > 3 && !ParseInt64(vDeploymentParams[3], &nStartHeight)) {
            throw std::runtime_error(strprintf("Invalid nStartHeight (%s)", vDeploymentParams[3]));
        }
        if (vDeploymentParams.size() > 4 && !ParseInt64(vDeploymentParams[4], &nTimeoutHeight)) {
            throw std::runtime_error(strprintf("Invalid nTimeoutHeight (%s)", vDeploymentParams[4]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout, nStartHeight, nTimeoutHeight);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld, start_height=%d, timeout_height=%d\n", vDeploymentParams[0], nStartTime, nTimeout, nStartHeight, nTimeoutHeight);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN) {
        return std::unique_ptr<CChainParams>(new CMainParams());
    } else if (chain == CBaseChainParams::TESTNET) {
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    } else if (chain == CBaseChainParams::SIGNET) {
        // BRTO-TODO: Implement a proper CSignetParams class with BurritoCoin-specific
        // signet challenge script and block-signing parameters. Until then, signet is
        // explicitly unsupported — do not silently fall back to testnet rules.
        throw std::runtime_error("Signet is not yet supported by BurritoCoin. Use testnet instead.");
    } else if (chain == CBaseChainParams::REGTEST) {
        return std::unique_ptr<CChainParams>(new CRegTestParams(args));
    }
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(gArgs, network);
}
