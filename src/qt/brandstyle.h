// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_BRANDSTYLE_H
#define BURRITOCOIN_QT_BRANDSTYLE_H

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QApplication;
QT_END_NAMESPACE

namespace brand {

//! Install the BurritoCoin warm-dark theme: the Fusion QStyle (which, unlike
//! the native Windows/macOS styles, draws every control from QPalette), the
//! brand palette from brand.h, and a small app-wide stylesheet for the pieces
//! Fusion can't express (toolbar tabs, gold progress chunk, menus, cards).
//!
//! Must be called BEFORE PlatformStyle is instantiated (it snapshots the
//! application palette exactly once to pick icon tint colors) and before the
//! first dialog is created (the first-run Intro precedes the main window).
void ApplyAppTheme(QApplication& app);

} // namespace brand

#endif // BURRITOCOIN_QT_BRANDSTYLE_H
