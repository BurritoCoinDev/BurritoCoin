// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_AMOUNT_H
#define BURRITOCOIN_AMOUNT_H

#include <stdint.h>

/** Amount in burrioshi (Can be negative) */
typedef int64_t CAmount;

static const CAmount COIN = 100000000;

/** No amount larger than this (in burrioshi) is valid.
 *
 * BurritoCoin hard cap: 21,000,000,000 BRTO total supply.
 *   - Genesis premine : 148,000,000 BRTO (block 0)
 *   - Mined supply    : 20,852,000,000 BRTO
 *       Block reward  : 10 BRTO/block (~2,102,400 BRTO/year at 2.5-min blocks)
 *       Halving intv. : 1,042,600,000 blocks (~4,960 years – effectively steady)
 *       Geometric sum : 10 × 1,042,600,000 × 2 = 20,852,000,000 BRTO
 *
 * This constant is consensus-critical; modifying it constitutes a hard fork.
 * */
static const CAmount MAX_MONEY = 21000000000LL * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif //  BURRITOCOIN_AMOUNT_H
