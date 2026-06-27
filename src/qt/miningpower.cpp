// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningpower.h>

#ifdef WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace MiningPower {

bool OnBattery()
{
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        // ACLineStatus: 0 = offline (on battery), 1 = online (AC), 255 = unknown.
        return status.ACLineStatus == 0;
    }
    return false;
}

int64_t IdleSeconds()
{
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(lii);
    if (GetLastInputInfo(&lii)) {
        // Unsigned subtraction stays correct across the ~49.7-day GetTickCount wrap.
        const DWORD elapsed_ms = GetTickCount() - lii.dwTime;
        return static_cast<int64_t>(elapsed_ms) / 1000;
    }
    return INT64_MAX;
}

} // namespace MiningPower

#else // !WIN32 — no portable AC/idle detection, so never auto-pause.

#include <limits>

namespace MiningPower {

bool OnBattery() { return false; }
int64_t IdleSeconds() { return std::numeric_limits<int64_t>::max(); }

} // namespace MiningPower

#endif
