// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_STRATUMJOB_H
#define BURRITOCOIN_STRATUMJOB_H

#include <primitives/block.h>
#include <uint256.h>

#include <cstdint>
#include <string>
#include <vector>

class CBlockIndex;
struct CBlockTemplate;

/**
 * One unit of mining work handed to an external Stratum miner (e.g. cpuminer),
 * plus everything needed to turn a returned share back into a full, valid block
 * on our side.
 *
 * The wallet keeps the COMPLETE block (mempool txs, the MWEB HogEx as vtx.back(),
 * and mweb_block) in `block`; only the 80-byte header, the coinbase split
 * (coinb1/coinb2), and the merkle branch ever cross the wire. The external miner
 * rolls the coinbase extranonce + ntime + nonce and scrypt-hashes the header;
 * we reconstruct the coinbase, recompute the merkle root, and re-validate the
 * block through the normal ProcessNewBlock path — so the miner is never trusted.
 *
 * No Qt or networking dependency: this is pure consensus-adjacent serialization,
 * so it can be unit-tested with a boost regtest fixture.
 */
struct StratumJob {
    std::string job_id;

    //! Full block snapshot. vtx[0] is the coinbase carrying an 8-byte extranonce
    //! placeholder; vtx[1..] are the mempool txs incl. the HogEx; mweb_block rides
    //! along untouched. Used verbatim (minus the rebuilt coinbase) on reconstruct.
    CBlock block;

    const CBlockIndex* parent = nullptr; //!< the tip this job was built on
    int height = 0;                      //!< parent->nHeight + 1 (baked into coinb1)
    uint32_t extranonce1 = 0;            //!< the 4-byte server extranonce for this session

    std::vector<unsigned char> coinb1;   //!< coinbase bytes up to the extranonce slot
    std::vector<unsigned char> coinb2;   //!< coinbase bytes after the extranonce slot
    std::vector<uint256> merkle_branch;  //!< coinbase-relative branch over vtx[1..]

    int32_t version = 0;
    uint32_t nbits = 0;
    uint32_t ntime = 0;

    //! Build the job from a freshly assembled template. `parent` MUST be the
    //! template's own parent (CreateNewBlock's tip), not the live tip, so the
    //! BIP34 height baked into coinb1 always matches block.hashPrevBlock.
    void buildFromTemplate(const CBlockTemplate& tmpl, const CBlockIndex* parent,
                           uint32_t extranonce1, const std::string& job_id);

    //! Rebuild the full block from a submitted share: coinbase =
    //! coinb1 || extranonce1 || extranonce2 || coinb2, with the 32-byte witness
    //! reserved value reattached (required since segwit is active from height 1),
    //! the merkle root recomputed, and ntime/nonce applied. The caller still
    //! re-checks PoW and submits via ProcessNewBlock.
    CBlock reconstructBlock(const std::vector<unsigned char>& extranonce2,
                            uint32_t ntime, uint32_t nonce) const;
};

namespace stratum {

//! Serialize a transaction to its txid preimage bytes (NO_WITNESS | NO_MWEB).
std::vector<unsigned char> SerializeTxidPreimage(const CTransaction& tx);

//! Coinbase-relative merkle branch over the given non-coinbase txids, matching
//! ComputeMerkleRoot's odd-duplication semantics.
std::vector<uint256> CoinbaseMerkleBranch(const std::vector<uint256>& txids_after_coinbase);

//! Fold a coinbase txid up a merkle branch to the root (the inverse of the
//! branch; used to verify a reconstructed block and by the miner conceptually).
uint256 FoldMerkleBranch(uint256 coinbase_txid, const std::vector<uint256>& branch);

// --- Stratum wire byte-order helpers (Bitcoin/Litecoin stratum conventions) ---

//! mining.notify prevhash: 32 internal bytes regrouped into 8 uint32 words, each
//! word byte-reversed (the historical stratum word-swap cpuminer un-swaps).
std::string EncodeStratumPrevhash(const uint256& hash);

//! Big-endian %08x hex of a uint32 (version / nbits / ntime on the wire).
std::string EncodeBE32(uint32_t value);

//! Parse a big-endian 8-hex-char word (mining.submit ntime/nonce) to a uint32.
bool DecodeBE32(const std::string& hex, uint32_t& out);

//! Raw internal bytes of a hash as lowercase hex (merkle branch entries).
std::string EncodeHashLE(const uint256& hash);

//! Parse a hex string to bytes; returns false on odd length / non-hex.
bool DecodeHexBytes(const std::string& hex, std::vector<unsigned char>& out);

} // namespace stratum

#endif // BURRITOCOIN_STRATUMJOB_H
