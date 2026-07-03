// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_MININGPOWER_H
#define BURRITOCOIN_QT_MININGPOWER_H

#include <cstdint>

//! Small, platform-isolated helpers for the miner's auto-pause feature. Kept in
//! their own translation unit so <windows.h> never leaks into the Qt sources.
namespace MiningPower {

//! True if the system is running on battery (not AC). Returns false on AC,
//! when the state is unknown, and on platforms where this can't be detected
//! (so "pause on battery" simply never fires there).
bool OnBattery();

//! Seconds since the last user input. Returns a very large value where idle
//! detection isn't available, so "pause while you're using the computer" never
//! fires on those platforms.
int64_t IdleSeconds();

} // namespace MiningPower

#endif // BURRITOCOIN_QT_MININGPOWER_H
