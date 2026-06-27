// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stratumjob.h>

#include <chain.h>
#include <consensus/merkle.h>
#include <hash.h>
#include <miner.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <streams.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
//! Distinctive 8-byte marker placed in the coinbase scriptSig where the
//! extranonce (en1 4 bytes + en2 4 bytes) goes, so the serialized coinbase can
//! be sliced into coinb1/coinb2 at exactly that offset.
const std::vector<unsigned char> EXTRANONCE_SENTINEL = {
    0xfa, 0xce, 0xb0, 0x0c, 0xde, 0xad, 0xbe, 0xef};

//! Double-SHA256 of two concatenated 32-byte hashes (a merkle parent node).
uint256 MerkleParent(const uint256& a, const uint256& b)
{
    return Hash(a, b);
}
} // namespace

namespace stratum {

std::vector<unsigned char> SerializeTxidPreimage(const CTransaction& tx)
{
    // Mirror CTransaction::ComputeHash exactly so coinb1||en1||en2||coinb2
    // reproduces the txid preimage byte-for-byte.
    CDataStream ss(SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS | SERIALIZE_NO_MWEB);
    ss << tx;
    return std::vector<unsigned char>(ss.begin(), ss.end());
}

std::vector<uint256> CoinbaseMerkleBranch(const std::vector<uint256>& others)
{
    // Build the full tree with a placeholder coinbase at index 0 and collect the
    // sibling at the coinbase's position on each level. The placeholder only ever
    // flows into the coinbase's own ancestors (index>>k), never into the siblings
    // we collect, so the branch is independent of the (unknown) coinbase txid.
    std::vector<uint256> branch;
    std::vector<uint256> level;
    level.reserve(others.size() + 1);
    level.push_back(uint256()); // placeholder coinbase
    level.insert(level.end(), others.begin(), others.end());

    size_t index = 0; // coinbase position, always even -> always the left child
    while (level.size() > 1) {
        if (level.size() & 1) level.push_back(level.back()); // odd-duplicate last
        branch.push_back(level[index ^ 1]);                  // sibling of the coinbase
        std::vector<uint256> next;
        next.reserve(level.size() / 2);
        for (size_t i = 0; i < level.size(); i += 2) {
            next.push_back(MerkleParent(level[i], level[i + 1]));
        }
        level = std::move(next);
        index >>= 1;
    }
    return branch;
}

uint256 FoldMerkleBranch(uint256 h, const std::vector<uint256>& branch)
{
    // The coinbase is leaf 0 -> always the left child at every level, so each
    // branch entry is its right sibling.
    for (const uint256& step : branch) {
        h = MerkleParent(h, step);
    }
    return h;
}

std::string EncodeBE32(uint32_t value)
{
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08x", value);
    return std::string(buf);
}

bool DecodeBE32(const std::string& hex, uint32_t& out)
{
    if (hex.size() != 8 || !IsHex(hex)) return false;
    out = static_cast<uint32_t>(std::strtoul(hex.c_str(), nullptr, 16));
    return true;
}

std::string EncodeHashLE(const uint256& hash)
{
    // Raw internal byte order (NOT display-reversed) — what stratum merkle
    // branch entries use.
    return HexStr(hash);
}

std::string EncodeStratumPrevhash(const uint256& hash)
{
    // 32 internal bytes regrouped into 8 four-byte words, each word byte-reversed
    // — the historical Bitcoin/Litecoin stratum word-swap that cpuminer un-swaps.
    const unsigned char* b = hash.begin();
    std::string out;
    out.reserve(64);
    for (int word = 0; word < 8; ++word) {
        for (int j = 3; j >= 0; --j) {
            char tmp[3];
            std::snprintf(tmp, sizeof(tmp), "%02x", b[word * 4 + j]);
            out += tmp;
        }
    }
    return out;
}

bool DecodeHexBytes(const std::string& hex, std::vector<unsigned char>& out)
{
    if ((hex.size() % 2) != 0 || (!hex.empty() && !IsHex(hex))) return false;
    out = ParseHex(hex);
    return true;
}

} // namespace stratum

void StratumJob::buildFromTemplate(const CBlockTemplate& tmpl, const CBlockIndex* p,
                                   uint32_t en1, const std::string& jid)
{
    job_id = jid;
    block = tmpl.block; // full snapshot: mempool txs + HogEx (vtx.back()) + mweb_block
    parent = p;
    height = p->nHeight + 1;
    extranonce1 = en1;
    version = block.nVersion;
    nbits = block.nBits;
    ntime = block.nTime;

    // Rebuild the coinbase scriptSig as BIP34 height-push + an 8-byte extranonce
    // slot (marked with a sentinel we can locate). Built from the template's
    // coinbase so its vout (subsidy + witness commitment) is preserved verbatim.
    CMutableTransaction cb(*block.vtx[0]);
    CScript sig = CScript() << height;
    sig << EXTRANONCE_SENTINEL; // pushes 0x08 followed by the 8 sentinel bytes
    assert(sig.size() <= 100);  // mirrors miner.cpp's coinbase scriptSig limit
    cb.vin[0].scriptSig = sig;
    const CTransaction sentinel_cb(cb);

    // Serialize (txid preimage) and slice at the sentinel.
    const std::vector<unsigned char> full = stratum::SerializeTxidPreimage(sentinel_cb);
    auto it = std::search(full.begin(), full.end(),
                          EXTRANONCE_SENTINEL.begin(), EXTRANONCE_SENTINEL.end());
    assert(it != full.end());
    assert(std::search(it + EXTRANONCE_SENTINEL.size(), full.end(),
                       EXTRANONCE_SENTINEL.begin(), EXTRANONCE_SENTINEL.end()) == full.end());
    coinb1.assign(full.begin(), it);
    coinb2.assign(it + EXTRANONCE_SENTINEL.size(), full.end());

    // Coinbase-relative merkle branch over the non-coinbase txids (incl. HogEx).
    std::vector<uint256> others;
    others.reserve(block.vtx.size() - 1);
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        others.push_back(block.vtx[i]->GetHash());
    }
    merkle_branch = stratum::CoinbaseMerkleBranch(others);
}

CBlock StratumJob::reconstructBlock(const std::vector<unsigned char>& en2,
                                    uint32_t nt, uint32_t nonce) const
{
    // The slot is exactly 8 bytes (en1 4 + en2 4); a non-4-byte en2 would
    // misalign the scriptSig and is rejected by the caller.
    assert(en2.size() == 4);

    std::vector<unsigned char> cbbytes = coinb1;
    // extranonce1 as 4 big-endian bytes, matching the hex we advertise in
    // mining.subscribe / mining.set_extranonce.
    cbbytes.push_back(static_cast<unsigned char>((extranonce1 >> 24) & 0xff));
    cbbytes.push_back(static_cast<unsigned char>((extranonce1 >> 16) & 0xff));
    cbbytes.push_back(static_cast<unsigned char>((extranonce1 >> 8) & 0xff));
    cbbytes.push_back(static_cast<unsigned char>(extranonce1 & 0xff));
    cbbytes.insert(cbbytes.end(), en2.begin(), en2.end());
    cbbytes.insert(cbbytes.end(), coinb2.begin(), coinb2.end());

    CMutableTransaction coinbase;
    {
        CDataStream ss(cbbytes, SER_GETHASH,
                       SERIALIZE_TRANSACTION_NO_WITNESS | SERIALIZE_NO_MWEB);
        ss >> coinbase;
    }
    // CRITICAL: segwit is active from height 1, so ConnectBlock rejects
    // "bad-witness-nonce-size" unless the coinbase scriptWitness is exactly one
    // 32-byte vector (the witness reserved value). The no-witness deserialization
    // left it empty, so reattach it. The commitment itself (in coinb2's vout) is
    // unaffected by the scriptSig rewrite, so we never re-run
    // GenerateCoinbaseCommitment.
    coinbase.vin[0].scriptWitness.stack.assign(1, std::vector<unsigned char>(32, 0x00));

    CBlock b = block;
    b.vtx[0] = MakeTransactionRef(std::move(coinbase));
    b.nVersion = version;
    b.nTime = nt;
    b.nNonce = nonce;
    b.hashMerkleRoot = BlockMerkleRoot(b);

    // The single highest-value runtime backstop against an endianness/serialization
    // bug: the coinbase folded up our branch must equal the real merkle root.
    assert(stratum::FoldMerkleBranch(b.vtx[0]->GetHash(), merkle_branch) == b.hashMerkleRoot);
    return b;
}
