// Copyright (c) 2011-2026 The Bitcoin Core developers
// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_GUICONSTANTS_H
#define BURRITOCOIN_QT_GUICONSTANTS_H

#include <cstdint>

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 250;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* BurritoCoinGUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* These colors are PAINTED (model foreground roles / delegates), so the
   app-wide stylesheet can't reach them. They are tuned for the warm-dark
   brand theme (backgrounds #1a0f05/#2a1608 — see qt/brand.h). */

/* Invalid field background style */
#define STYLE_INVALID "background:#5c1a14; color:#f0e0c0; border:1px solid #c0392b"

/* Transaction list -- unconfirmed transaction (brand muted) */
#define COLOR_UNCONFIRMED QColor(184, 154, 114)
/* Transaction list -- negative amount (bright warm red, readable at 12px) */
#define COLOR_NEGATIVE QColor(255, 93, 77)
/* Transaction list -- bare address (without label) (brand muted-grey) */
#define COLOR_BAREADDRESS QColor(154, 122, 85)
/* Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(122, 160, 255)
/* Transaction list -- TX status decoration - danger, tx needs attention */
#define COLOR_TX_STATUS_DANGER QColor(224, 96, 77)
/* Transaction list -- TX status decoration - default color (brand text) */
#define COLOR_TX_STATUS_DEFAULT QColor(240, 224, 192)

/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "BurritoCoin"
#define QAPP_ORG_DOMAIN "burritoco.in"
#define QAPP_APP_NAME_DEFAULT "BurritoCoin-Qt"
#define QAPP_APP_NAME_TESTNET "BurritoCoin-Qt-testnet"
#define QAPP_APP_NAME_SIGNET "BurritoCoin-Qt-signet"
#define QAPP_APP_NAME_REGTEST "BurritoCoin-Qt-regtest"

/* One gigabyte (GB) in bytes */
static constexpr uint64_t GB_BYTES{1000000000};

// Default prune target displayed in GUI.
static constexpr int DEFAULT_PRUNE_TARGET_GB{2};

#endif // BURRITOCOIN_QT_GUICONSTANTS_H
