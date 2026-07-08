// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_BRAND_H
#define BURRITOCOIN_QT_BRAND_H

//! Canonical BurritoCoin design tokens — the single source of truth for brand
//! colors in the wallet, mirroring website/styles.css :root so the app and the
//! site read as one product. Use these instead of ad-hoc hex literals.
namespace brand {

// Brand golds
constexpr const char* Gold      = "#f5a623"; // primary brand color
constexpr const char* GoldDark  = "#c47d0e";
constexpr const char* GoldLight = "#ffd170"; // hover / highlight

// Warm neutrals
constexpr const char* Brown  = "#3b1f0a"; // text on gold (e.g. button labels)
constexpr const char* WarmBg = "#1a0f05"; // deepest background
constexpr const char* CardBg = "#2a1608"; // panel / card surface
constexpr const char* Text   = "#f0e0c0"; // body text on warm bg
constexpr const char* Muted  = "#b89a72"; // secondary text (AA on CardBg)
constexpr const char* Border = "#5a3010";
constexpr const char* CodeBg = "#0f0803"; // monospace / address surfaces

// Status (used for the exact safety copy newcomers rely on)
constexpr const char* Danger  = "#c0392b"; // irreversible / dangerous
constexpr const char* Success = "#1e8e3e"; // done / safe / confirmed
constexpr const char* Warning = "#c47d0e"; // caution (brand gold-dark)
constexpr const char* MutedGrey = "#9a7a55"; // de-emphasized notes

// Dark-theme derivatives (small text on WarmBg/CardBg needs brighter status
// hues than the tokens above, which were tuned for large/bold copy)
constexpr const char* Surface      = "#38220e"; // raised control surface (buttons)
constexpr const char* AltRow       = "#221105"; // alternate table row on CodeBg
constexpr const char* Hover        = "#ffc04d"; // gold hover (established in walletframe)
constexpr const char* Disabled     = "#7a6444"; // disabled text on warm surfaces
constexpr const char* DangerLight  = "#e0604d"; // danger text on dark backgrounds
constexpr const char* SuccessLight = "#2eb85c"; // success text on dark backgrounds

} // namespace brand

#endif // BURRITOCOIN_QT_BRAND_H
