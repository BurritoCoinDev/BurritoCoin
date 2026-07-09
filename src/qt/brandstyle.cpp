// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/brandstyle.h>

#include <qt/brand.h>

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStyle>
#include <QStyleFactory>

namespace brand {
namespace {

QPalette AppPalette()
{
    QPalette pal;

    const QColor window(WarmBg);
    const QColor text(Text);
    const QColor base(CodeBg);
    const QColor button(CardBg);
    const QColor disabled(Disabled);

    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, QColor(AltRow));
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, button);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::BrightText, QColor(255, 255, 255));
    // Selection is a muted warm brown with cream text — NOT brand gold: the
    // icon glyphs are tinted gold (platformstyle.cpp), and a gold selection
    // would swallow them on selected rows (coin-control lock, tx status).
    pal.setColor(QPalette::Highlight, QColor(Border));
    pal.setColor(QPalette::HighlightedText, text);
    pal.setColor(QPalette::ToolTipBase, QColor(CardBg));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::Link, QColor(GoldLight));
    pal.setColor(QPalette::LinkVisited, QColor(GoldDark));

    // Shades Fusion derives 3D bevels, separators and sunken lines from.
    pal.setColor(QPalette::Light, QColor(0x4a, 0x2c, 0x12));
    pal.setColor(QPalette::Midlight, QColor(Surface));
    pal.setColor(QPalette::Mid, QColor(0x24, 0x13, 0x07));
    pal.setColor(QPalette::Dark, QColor(0x12, 0x0a, 0x04));
    pal.setColor(QPalette::Shadow, QColor(0, 0, 0));

    // Disabled state: dim but still legible on the warm surfaces.
    pal.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    pal.setColor(QPalette::Disabled, QPalette::Text, disabled);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(Surface));
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(MutedGrey));

    return pal;
}

QString AppStyleSheet()
{
    QString qss = QStringLiteral(
        // Tooltips (rich-text tooltips ignore palette on some platforms)
        "QToolTip { color: @TEXT@; background-color: @CARD@; border: 1px solid @BORDER@; padding: 4px 8px; }"

        // Menu bar + menus
        "QMenuBar { background-color: @CODEBG@; color: @TEXT@; }"
        "QMenuBar::item { background: transparent; padding: 5px 10px; }"
        "QMenuBar::item:selected { background-color: @CARD@; color: @GOLDLIGHT@; }"
        "QMenuBar::item:disabled { color: @DISABLED@; }"
        "QMenu { background-color: @CARD@; color: @TEXT@; border: 1px solid @BORDER@; }"
        "QMenu::item { background: transparent; padding: 5px 26px 5px 14px; }"
        "QMenu::item:selected { background-color: @BORDER@; color: @TEXT@; }"
        "QMenu::item:disabled { color: @DISABLED@; }"
        "QMenu::separator { height: 1px; background-color: @BORDER@; margin: 4px 10px; }"

        // The tab toolbar: flat dark bar, gold underline on the active tab
        "QToolBar { background-color: @CODEBG@; border: 0; border-bottom: 1px solid @BORDER@; padding: 2px 8px 0 8px; }"
        "QToolBar QToolButton { background: transparent; color: @MUTED@; font-weight: bold;"
        "  padding: 8px 14px; border: 0; border-bottom: 3px solid transparent; }"
        "QToolBar QToolButton:hover { color: @TEXT@; }"
        "QToolBar QToolButton:checked { color: @GOLD@; border-bottom: 3px solid @GOLD@; }"
        "QToolBar QToolButton:disabled { color: @DISABLED@; }"
        "QToolBar QLabel { color: @MUTED@; }"

        // Status bar
        "QStatusBar { background-color: @CODEBG@; color: @MUTED@; }"
        "QStatusBar::item { border: none; }"

        // Buttons: dark raised surface, gold accents; brand CTAs override locally
        "QPushButton { background-color: @SURFACE@; color: @TEXT@; border: 1px solid @BORDER@;"
        "  border-radius: 6px; padding: 6px 14px; }"
        "QPushButton:hover { border-color: @GOLD@; color: @GOLDLIGHT@; }"
        "QPushButton:pressed { background-color: @CODEBG@; }"
        "QPushButton:disabled { background-color: @CARD@; color: @DISABLED@; border-color: @SURFACE@; }"
        "QPushButton:default { border: 1px solid @GOLD@; }"
        // Icon-only flat buttons (overview sync-warning, overlay icon) must not
        // grow a box. Declared last so it wins over the rules above.
        "QPushButton:flat { background: transparent; border: none; }"
        "QPushButton:flat:disabled { background: transparent; border: none; }"

        // Progress bars are deliberately NOT styled here: a stylesheet paints
        // the centered label in one color, which is unreadable over either the
        // dark trough or the gold chunk. Fusion flips the label between Text
        // and HighlightedText as the chunk passes it — the gold-chunk bars get
        // a widget-local palette (burritocoingui.cpp, modaloverlay.cpp).

        // Group boxes (options, sign/verify): gold caption on a hairline card
        "QGroupBox { border: 1px solid @BORDER@; border-radius: 8px; margin-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px;"
        "  color: @GOLD@; font-weight: bold; }"

        // Tab widgets (options dialog, node window)
        "QTabWidget::pane { border: 1px solid @BORDER@; border-radius: 4px; }"
        "QTabBar::tab { background: transparent; color: @MUTED@; padding: 7px 16px;"
        "  border-bottom: 2px solid transparent; }"
        "QTabBar::tab:hover { color: @TEXT@; }"
        "QTabBar::tab:selected { color: @GOLD@; border-bottom: 2px solid @GOLD@; }"

        // Dock (Help & FAQ panel)
        "QDockWidget { color: @GOLD@; font-weight: bold; }"
        "QDockWidget::title { background-color: @CARD@; padding: 6px 10px;"
        "  border-bottom: 1px solid @BORDER@; }"

        // Card panels on the main pages (scoped by page to avoid QFrame blast radius)
        "QWidget#OverviewPage QFrame#frame, QWidget#OverviewPage QFrame#frame_2,"
        "QDialog#SendCoinsDialog QFrame#frameCoinControl, QDialog#SendCoinsDialog QFrame#frameFee,"
        "QDialog#SendCoinsDialog QFrame#frameMWEBFeatures,"
        "QDialog#ReceiveCoinsDialog QFrame#frame, QDialog#ReceiveCoinsDialog QFrame#frame2"
        "{ background-color: @CARD@; border: 1px solid @BORDER@; border-radius: 10px; }"

        // The send-entries scroll region should blend into the page
        "QDialog#SendCoinsDialog QScrollArea { background: transparent; }"
        "QDialog#SendCoinsDialog QWidget#scrollAreaWidgetContents { background: transparent; }");

    // Placeholders are @-terminated, so replacement order does not matter
    // ("@GOLD@" is not a substring of "@GOLDLIGHT@").
    qss.replace(QStringLiteral("@TEXT@"), QString::fromLatin1(Text));
    qss.replace(QStringLiteral("@MUTED@"), QString::fromLatin1(Muted));
    qss.replace(QStringLiteral("@DISABLED@"), QString::fromLatin1(Disabled));
    qss.replace(QStringLiteral("@GOLDLIGHT@"), QString::fromLatin1(GoldLight));
    qss.replace(QStringLiteral("@GOLD@"), QString::fromLatin1(Gold));
    qss.replace(QStringLiteral("@BROWN@"), QString::fromLatin1(Brown));
    qss.replace(QStringLiteral("@CARD@"), QString::fromLatin1(CardBg));
    qss.replace(QStringLiteral("@CODEBG@"), QString::fromLatin1(CodeBg));
    qss.replace(QStringLiteral("@BORDER@"), QString::fromLatin1(Border));
    qss.replace(QStringLiteral("@SURFACE@"), QString::fromLatin1(Surface));
    return qss;
}

} // namespace

void ApplyAppTheme(QApplication& app)
{
    // Fusion draws every control (combo popups, calendars, headers, scroll
    // bars, spin boxes) from QPalette; the native Windows style ignores the
    // palette for most of them. Guarded: falls back to the native style if
    // Fusion is unavailable, leaving just the stylesheet accents.
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fusion);
    }
    QApplication::setPalette(AppPalette());
    app.setStyleSheet(AppStyleSheet());
}

} // namespace brand
