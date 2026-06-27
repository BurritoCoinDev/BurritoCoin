// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>
#include <miner.h>
#include <pow.h>
#include <primitives/block.h>
#include <script/script.h>
#include <stratumjob.h>
#include <txmempool.h>
#include <uint256.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <algorithm>
#include <memory>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(stratum_roundtrip_tests, TestChain100Setup)

// The load-bearing test: a template -> StratumJob -> reconstructed block must be
// merkle-consistent AND accepted by ProcessNewBlock once a (low regtest
// difficulty) PoW is ground out. Acceptance proves the coinbase split, the
// hand-computed merkle branch, the reattached 32-byte witness reserved value,
// and any MWEB carry-through are all consensus-valid — not just self-consistent.
BOOST_AUTO_TEST_CASE(coinbase_split_merkle_and_submit_roundtrip)
{
    const CChainParams& params = Params();
    const Consensus::Params& consensus = params.GetConsensus();

    const CScript scriptPubKey = CScript() << OP_TRUE;
    std::unique_ptr<CBlockTemplate> tmpl(
        BlockAssembler(*m_node.mempool, params).CreateNewBlock(scriptPubKey));
    BOOST_REQUIRE(tmpl);

    const CBlockIndex* tip = nullptr;
    {
        LOCK(cs_main);
        tip = ::ChainActive().Tip();
    }
    BOOST_REQUIRE(tip);

    StratumJob job;
    job.buildFromTemplate(*tmpl, tip, 0x12345678u, "job1");

    BOOST_CHECK(!job.coinb1.empty());
    BOOST_CHECK(!job.coinb2.empty());
    BOOST_CHECK_EQUAL(job.height, tip->nHeight + 1);

    const std::vector<unsigned char> en2 = {0xaa, 0xbb, 0xcc, 0xdd};
    CBlock b = job.reconstructBlock(en2, tmpl->block.nTime, 0);

    // (1) The reconstructed coinbase folded up the branch equals the merkle root.
    BOOST_CHECK(stratum::FoldMerkleBranch(b.vtx[0]->GetHash(), job.merkle_branch) ==
                b.hashMerkleRoot);

    // (2) BIP34 height-first scriptSig within the 100-byte limit.
    BOOST_CHECK(b.vtx[0]->vin[0].scriptSig.size() <= 100u);

    // (3) Witness reserved value reattached as exactly one 32-byte item.
    BOOST_REQUIRE_EQUAL(b.vtx[0]->vin[0].scriptWitness.stack.size(), 1u);
    BOOST_CHECK_EQUAL(b.vtx[0]->vin[0].scriptWitness.stack[0].size(), 32u);

    // (4) Grind the low-difficulty PoW (merkle root is fixed by coinbase+en2, so
    // rolling nNonce alone is valid) and submit through the real validation path.
    while (!CheckProofOfWork(b.GetPoWHash(), b.nBits, consensus)) {
        ++b.nNonce;
    }
    auto shared = std::make_shared<const CBlock>(b);
    bool new_block = false;
    const bool accepted =
        m_node.chainman->ProcessNewBlock(params, shared, /*fForceProcessing=*/true, &new_block);
    BOOST_CHECK(accepted);
    BOOST_CHECK(new_block);

    // NOTE: regtest does not activate MWEB by height 100 (it is BIP9-gated, not
    // always-active, so the 100-block setup helper has no HogEx), and the mempool
    // here is empty — so the round-trip above exercises the coinbase-only,
    // empty-merkle-branch path. The MWEB HogEx/mweb_block carry-through is covered
    // by the production design and the native mainnet/testnet end-to-end run with
    // a real cpuminer (phase 5); the deterministic merkle-branch coverage that the
    // empty path misses is provided by the next test.
}

// Deterministic, chain-independent validation of the riskiest logic in this file
// — the hand-built coinbase merkle branch — against ComputeMerkleRoot (the
// consensus oracle), across non-coinbase counts that exercise odd-duplication at
// several levels (the regtest round-trip above can only ever produce an empty
// branch).
BOOST_AUTO_TEST_CASE(merkle_branch_matches_consensus_oracle)
{
    auto make_txid = [](int i) {
        std::vector<unsigned char> bytes(32, 0);
        bytes[0] = static_cast<unsigned char>(i + 1);
        bytes[31] = 0xab;
        return uint256(bytes);
    };
    const uint256 coinbase = make_txid(100);

    for (size_t n = 0; n <= 6; ++n) {
        std::vector<uint256> others;
        for (size_t i = 0; i < n; ++i) others.push_back(make_txid(static_cast<int>(i)));

        std::vector<uint256> leaves;
        leaves.push_back(coinbase);
        leaves.insert(leaves.end(), others.begin(), others.end());

        const uint256 expected = ComputeMerkleRoot(leaves);
        const std::vector<uint256> branch = stratum::CoinbaseMerkleBranch(others);
        const uint256 got = stratum::FoldMerkleBranch(coinbase, branch);
        BOOST_CHECK_MESSAGE(got == expected,
                            "merkle branch mismatch for " << n << " non-coinbase txids");
    }
}

// Pin the stratum wire byte-order helpers (the other classic break point).
BOOST_AUTO_TEST_CASE(wire_helpers_roundtrip)
{
    BOOST_CHECK_EQUAL(stratum::EncodeBE32(0xdeadbeefu), "deadbeef");
    uint32_t out = 0;
    BOOST_CHECK(stratum::DecodeBE32("deadbeef", out));
    BOOST_CHECK_EQUAL(out, 0xdeadbeefu);
    BOOST_CHECK(!stratum::DecodeBE32("zzzz", out));
    BOOST_CHECK(!stratum::DecodeBE32("dead", out)); // wrong length

    std::vector<unsigned char> bytes;
    BOOST_CHECK(stratum::DecodeHexBytes("aabbccdd", bytes));
    BOOST_REQUIRE_EQUAL(bytes.size(), 4u);
    BOOST_CHECK_EQUAL(bytes[0], 0xaa);
    BOOST_CHECK(!stratum::DecodeHexBytes("abc", bytes)); // odd length

    // Prevhash word-swap: 64 hex chars and a pure permutation of the input's 32
    // bytes (the exact word ordering is pinned against the target cpuminer build
    // during phase-5 real-miner integration; here we catch gross errors).
    uint256 h = uint256S("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    const std::string pv = stratum::EncodeStratumPrevhash(h);
    BOOST_REQUIRE_EQUAL(pv.size(), 64u);
    std::vector<unsigned char> pv_bytes;
    BOOST_REQUIRE(stratum::DecodeHexBytes(pv, pv_bytes));
    std::vector<unsigned char> in_bytes(h.begin(), h.end());
    std::sort(in_bytes.begin(), in_bytes.end());
    std::sort(pv_bytes.begin(), pv_bytes.end());
    BOOST_CHECK(in_bytes == pv_bytes);
}

BOOST_AUTO_TEST_SUITE_END()
