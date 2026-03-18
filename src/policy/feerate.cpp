// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/feerate.h>

#include <tinyformat.h>

static const CAmount BASE_MWEB_FEE = 100;

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nBytes_, uint64_t mweb_weight)
    : m_nFeePaid(nFeePaid), m_nBytes(nBytes_), m_weight(mweb_weight)
{
    assert(nBytes_ <= uint64_t(std::numeric_limits<int64_t>::max()));
    assert(mweb_weight <= uint64_t(std::numeric_limits<int64_t>::max()));

    CAmount mweb_fee = CAmount(mweb_weight) * BASE_MWEB_FEE;
    if (mweb_fee > 0 && nFeePaid < mweb_fee) {
        nBurrioshisPerK = 0;
    } else {
        CAmount base_fee = (nFeePaid - mweb_fee);

        int64_t nSize = int64_t(nBytes_);
        if (nSize > 0)
            nBurrioshisPerK = base_fee * 1000 / nSize;
        else
            nBurrioshisPerK = 0;
    }
}

CAmount CFeeRate::GetFee(size_t nBytes_) const
{
    assert(nBytes_ <= uint64_t(std::numeric_limits<int64_t>::max()));
    int64_t nSize = int64_t(nBytes_);

    CAmount nFee = nBurrioshisPerK * nSize / 1000;

    if (nFee == 0 && nSize != 0) {
        if (nBurrioshisPerK > 0)
            nFee = CAmount(1);
        if (nBurrioshisPerK < 0)
            nFee = CAmount(-1);
    }

    return nFee;
}

CAmount CFeeRate::GetMWEBFee(uint64_t mweb_weight) const
{
    assert(mweb_weight <= uint64_t(std::numeric_limits<int64_t>::max()));
    return CAmount(mweb_weight) * BASE_MWEB_FEE;
}

CAmount CFeeRate::GetTotalFee(size_t nBytes, uint64_t mweb_weight) const
{
    return GetFee(nBytes) + GetMWEBFee(mweb_weight);
}

bool CFeeRate::MeetsFeePerK(const CAmount& min_fee_per_k) const
{
    // (mweb_weight * BASE_MWEB_FEE) burrioshi are required as fee for MWEB transactions.
    // Anything beyond that can be used to calculate nBurrioshisPerK.
    CAmount mweb_fee = CAmount(m_weight) * BASE_MWEB_FEE;
    if (m_weight > 0 && m_nFeePaid < mweb_fee) {
        return false;
    }

    // MWEB-to-MWEB transactions don't have a size to calculate nBurrioshisPerK.
    // Since we got this far, we know the transaction meets the minimum MWEB fee, so return true.
    if (m_nBytes == 0 && m_weight > 0) {
        return true;
    }

    return nBurrioshisPerK >= min_fee_per_k;
}

std::string CFeeRate::ToString(const FeeEstimateMode& fee_estimate_mode) const
{
    switch (fee_estimate_mode) {
    case FeeEstimateMode::SAT_VB: return strprintf("%d.%03d %s/vB", nBurrioshisPerK / 1000, nBurrioshisPerK % 1000, CURRENCY_ATOM);
    default:                      return strprintf("%d.%08d %s/kvB", nBurrioshisPerK / COIN, nBurrioshisPerK % COIN, CURRENCY_UNIT);
    }
}
