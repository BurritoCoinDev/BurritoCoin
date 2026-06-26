// Copyright (c) 2011-2026 The Bitcoin Core developers
// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/burritocoingui.h>

#include <qt/burritocoinunits.h>
#include <qt/clientmodel.h>
#include <qt/createwalletdialog.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/modaloverlay.h>
#include <qt/networkstyle.h>
#include <qt/notificator.h>
#include <qt/openuridialog.h>
#include <qt/optionsdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>
#include <qt/utilitydialog.h>

#ifdef ENABLE_WALLET
#include <qt/walletcontroller.h>
#include <qt/walletframe.h>
#include <qt/walletmodel.h>
#include <qt/walletview.h>
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include <qt/macdockiconhandler.h>
#endif

#include <chain.h>
#include <chainparams.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <node/ui_interface.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegExp>
#include <QRegExpValidator>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWindow>

#ifdef ENABLE_WALLET
namespace {
//! Native-separator on-disk locations for a loaded wallet.
struct WalletDiskPaths {
    QString folder; //!< directory that contains the wallet
    QString file;   //!< the wallet.dat file itself
    bool valid = false;
};

//! Work out where a wallet's wallet.dat lives so we can show the user, instead
//! of making them hunt for it. The default (unnamed) wallet sits directly in
//! the wallet directory; a named wallet lives in a sub-folder of that name.
WalletDiskPaths GetWalletDiskPaths(interfaces::Node& node, WalletModel* wallet_model)
{
    WalletDiskPaths paths;
    if (!wallet_model) return paths;
    const QString wallet_dir = QString::fromStdString(node.walletClient().getWalletDir());
    const QString name = wallet_model->getWalletName();
    const QString folder = name.isEmpty() ? wallet_dir : QDir(wallet_dir).filePath(name);
    const QFileInfo folder_info(folder);
    if (folder_info.isFile()) {
        // Bare single-file wallet (e.g. -wallet=foo.dat): the "name" is the file
        // itself, not a sub-folder containing wallet.dat.
        paths.file = QDir::toNativeSeparators(folder_info.absoluteFilePath());
        paths.folder = QDir::toNativeSeparators(folder_info.absolutePath());
    } else {
        // Directory-style wallet (the usual case): wallet.dat lives in the folder.
        paths.folder = QDir::toNativeSeparators(folder);
        paths.file = QDir::toNativeSeparators(QDir(folder).filePath(QStringLiteral("wallet.dat")));
    }
    paths.valid = true;
    return paths;
}

//! Wallet dir root via the Qt-safe node interface (never call GetWalletDir()
//! directly from src/qt).
static QString RestoreWalletDir(interfaces::Node& node)
{
    return QString::fromStdString(node.walletClient().getWalletDir());
}

//! Cheap 16-byte signature sniff so we can refuse "that's a JPEG" before we
//! copy. Recognises both Berkeley DB legacy wallets and SQLite descriptor wallets.
static bool LooksLikeWalletDat(const QString& path, QString* why_not)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (why_not) *why_not = QObject::tr("The backup file could not be opened for reading.");
        return false;
    }
    const QByteArray head = f.read(16);
    f.close();
    if (head.size() < 16) {
        if (why_not) *why_not = QObject::tr("The backup file is too small to be a wallet.");
        return false;
    }
    if (head.startsWith("SQLite format 3")) return true;
    // BDB BTREE magic 0x00053162. Berkeley DB writes its meta-page magic in the
    // writing host's native byte order, so accept BOTH endians, mirroring the
    // canonical check in src/wallet/bdb.cpp.
    if (static_cast<quint8>(head[12]) == 0x62 && static_cast<quint8>(head[13]) == 0x31 &&
        static_cast<quint8>(head[14]) == 0x05 && static_cast<quint8>(head[15]) == 0x00) {
        return true;
    }
    if (static_cast<quint8>(head[12]) == 0x00 && static_cast<quint8>(head[13]) == 0x05 &&
        static_cast<quint8>(head[14]) == 0x31 && static_cast<quint8>(head[15]) == 0x62) {
        return true;
    }
    if (why_not) *why_not = QObject::tr("This file does not look like a wallet.dat "
                                        "(no Berkeley DB or SQLite signature).");
    return false;
}

//! True iff `name` collides with a name we must not let through to mkpath/copy.
//! Covers our wallet.dat sentinel, POSIX dot-names, and Windows DOS device names.
//! The QRegExpValidator already blocks slashes / backslashes / dots at the
//! keystroke level; this catches anything pasted past it. Windows reserved names
//! are refused on every platform so restored wallets stay portable.
static bool IsReservedWalletName(const QString& name)
{
    if (name.compare(QStringLiteral("wallet.dat"), Qt::CaseInsensitive) == 0) return true;
    if (name == QStringLiteral(".") || name == QStringLiteral("..")) return true;
    static const char* const kWinReserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM0", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT0", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    };
    for (const char* w : kWinReserved) {
        if (name.compare(QLatin1String(w), Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}
} // namespace
#endif // ENABLE_WALLET


const std::string BurritoCoinGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

BurritoCoinGUI::BurritoCoinGUI(interfaces::Node& node, const PlatformStyle *_platformStyle, const NetworkStyle *networkStyle, QWidget *parent) :
    QMainWindow(parent),
    m_node(node),
    trayIconMenu{new QMenu()},
    platformStyle(_platformStyle),
    m_network_style(networkStyle)
{
    QSettings settings;
    if (!restoreGeometry(settings.value("MainWindowGeometry").toByteArray())) {
        // Restore failed (perhaps missing setting), center the window
        move(QGuiApplication::primaryScreen()->availableGeometry().center() - frameGeometry().center());
    }

#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    QApplication::setWindowIcon(m_network_style->getTrayAndWindowIcon());
    setWindowIcon(m_network_style->getTrayAndWindowIcon());
    updateWindowTitle();

    rpcConsole = new RPCConsole(node, _platformStyle, nullptr);
    helpMessageDialog = new HelpMessageDialog(this, false);
#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(_platformStyle, this);
        setCentralWidget(walletFrame);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
        Q_EMIT consoleShown(rpcConsole);
    }

    modalOverlay = new ModalOverlay(enableWallet, this->centralWidget());

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Right-side Help & FAQ panel. Created before the menu bar so its toggle
    // action can be added to the Window menu.
    createFaqPanel();

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        createTrayIcon();
    }
    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);

    // Create status bar
    statusBar();

    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    unitDisplayControl = new UnitDisplayStatusBarControl(platformStyle);
    labelWalletEncryptionIcon = new QLabel();
    labelWalletHDStatusIcon = new QLabel();
    m_backup_status_label = new GUIUtil::ClickableLabel();
    m_backup_status_label->hide();
    labelProxyIcon = new GUIUtil::ClickableLabel();
    connectionsControl = new GUIUtil::ClickableLabel();
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if(enableWallet)
    {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(m_backup_status_label);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    frameBlocksLayout->addWidget(labelProxyIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(connectionsControl);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://doc.qt.io/qt-5/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // Rotating wallet-safety reminder ("back up your wallet.dat", etc.)
    createSafetyTipBanner();

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    connect(connectionsControl, &GUIUtil::ClickableLabel::clicked, [this] {
        m_node.setNetworkActive(!m_node.getNetworkActive());
    });
    connect(labelProxyIcon, &GUIUtil::ClickableLabel::clicked, [this] {
        openOptionsDialogWithTab(OptionsDialog::TAB_NETWORK);
    });

    connect(labelBlocksIcon, &GUIUtil::ClickableLabel::clicked, this, &BurritoCoinGUI::showModalOverlay);
#ifdef ENABLE_WALLET
    // Guarded: WalletFrame is only a complete type when wallet support is built.
    connect(m_backup_status_label, &GUIUtil::ClickableLabel::clicked, [this] {
        if (walletFrame) walletFrame->backupWallet();
    });
#endif // ENABLE_WALLET
    connect(progressBar, &GUIUtil::ClickableProgressBar::clicked, this, &BurritoCoinGUI::showModalOverlay);
#ifdef ENABLE_WALLET
    if(enableWallet) {
        connect(walletFrame, &WalletFrame::requestedSyncWarningInfo, this, &BurritoCoinGUI::showModalOverlay);
    }
#endif

#ifdef Q_OS_MAC
    m_app_nap_inhibitor = new CAppNapInhibitor;
#endif

    GUIUtil::handleCloseWindowShortcut(this);
}

BurritoCoinGUI::~BurritoCoinGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    QSettings settings;
    settings.setValue("MainWindowGeometry", saveGeometry());
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete m_app_nap_inhibitor;
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}

void BurritoCoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);
    connect(modalOverlay, &ModalOverlay::triggered, tabGroup, &QActionGroup::setEnabled);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a BurritoCoin address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and burritocoin: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    mineAction = new QAction(platformStyle->SingleColorIcon(":/icons/tx_mined"), tr("&Mine"), this);
    mineAction->setStatusTip(tr("Mine BurritoCoin into this wallet"));
    mineAction->setToolTip(mineAction->statusTip());
    mineAction->setCheckable(true);
    mineAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(mineAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(overviewAction, &QAction::triggered, this, &BurritoCoinGUI::gotoOverviewPage);
    connect(sendCoinsAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(sendCoinsAction, &QAction::triggered, [this]{ gotoSendCoinsPage(); });
    connect(sendCoinsMenuAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(sendCoinsMenuAction, &QAction::triggered, [this]{ gotoSendCoinsPage(); });
    connect(receiveCoinsAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(receiveCoinsAction, &QAction::triggered, this, &BurritoCoinGUI::gotoReceiveCoinsPage);
    connect(receiveCoinsMenuAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(receiveCoinsMenuAction, &QAction::triggered, this, &BurritoCoinGUI::gotoReceiveCoinsPage);
    connect(historyAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(historyAction, &QAction::triggered, this, &BurritoCoinGUI::gotoHistoryPage);
    connect(mineAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(mineAction, &QAction::triggered, this, &BurritoCoinGUI::gotoMiningPage);
#endif // ENABLE_WALLET

    quitAction = new QAction(tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(tr("&About %1").arg(PACKAGE_NAME), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(PACKAGE_NAME));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setEnabled(false);
    aboutQtAction = new QAction(tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(PACKAGE_NAME));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);
    toggleHideAction = new QAction(tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    m_show_wallet_location_action = new QAction(tr("Show Wallet &File Location..."), this);
    m_show_wallet_location_action->setStatusTip(tr("Show where this wallet's wallet.dat file is stored on your computer"));
    m_verify_backup_key_action = new QAction(tr("Verify Backup &Key..."), this);
    m_verify_backup_key_action->setStatusTip(tr("Check, on this computer only, that a private key you wrote down belongs to this wallet"));
    changePassphraseAction = new QAction(tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your BurritoCoin addresses to prove you own them"));
    verifyMessageAction = new QAction(tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified BurritoCoin addresses"));
    m_load_psbt_action = new QAction(tr("&Load PSBT from file..."), this);
    m_load_psbt_action->setStatusTip(tr("Load Partially Signed BurritoCoin Transaction"));
    m_load_psbt_clipboard_action = new QAction(tr("Load PSBT from clipboard..."), this);
    m_load_psbt_clipboard_action->setStatusTip(tr("Load Partially Signed BurritoCoin Transaction from clipboard"));

    openRPCConsoleAction = new QAction(tr("Node window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open node debugging and diagnostic console"));
    // initially disable the debug window menu item
    openRPCConsoleAction->setEnabled(false);
    openRPCConsoleAction->setObjectName("openRPCConsoleAction");

    usedSendingAddressesAction = new QAction(tr("&Sending addresses"), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(tr("&Receiving addresses"), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a burritocoin: URI"));

    m_open_wallet_action = new QAction(tr("Open Wallet"), this);
    m_open_wallet_action->setEnabled(false);
    m_open_wallet_action->setStatusTip(tr("Open a wallet"));
    m_open_wallet_menu = new QMenu(this);

    m_close_wallet_action = new QAction(tr("Close Wallet..."), this);
    m_close_wallet_action->setStatusTip(tr("Close wallet"));
    
    m_create_wallet_action = new QAction(tr("Create Wallet..."), this);
    m_create_wallet_action->setEnabled(false);
    m_create_wallet_action->setStatusTip(tr("Create a new wallet"));

    m_restore_wallet_action = new QAction(tr("&Restore Wallet from Backup..."), this);
    m_restore_wallet_action->setEnabled(false);
    m_restore_wallet_action->setStatusTip(tr("Import an existing wallet.dat backup file into a new wallet on this computer"));

    m_close_all_wallets_action = new QAction(tr("Close All Wallets..."), this);
    m_close_all_wallets_action->setStatusTip(tr("Close all wallets"));

    showHelpMessageAction = new QAction(tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible BurritoCoin command-line options").arg(PACKAGE_NAME));

    m_mask_values_action = new QAction(tr("&Mask values"), this);
    m_mask_values_action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M));
    m_mask_values_action->setStatusTip(tr("Mask the values in the Overview tab"));
    m_mask_values_action->setCheckable(true);

    connect(quitAction, &QAction::triggered, qApp, QApplication::quit);
    connect(aboutAction, &QAction::triggered, this, &BurritoCoinGUI::aboutClicked);
    connect(aboutQtAction, &QAction::triggered, qApp, QApplication::aboutQt);
    connect(optionsAction, &QAction::triggered, this, &BurritoCoinGUI::optionsClicked);
    connect(toggleHideAction, &QAction::triggered, this, &BurritoCoinGUI::toggleHidden);
    connect(showHelpMessageAction, &QAction::triggered, this, &BurritoCoinGUI::showHelpMessageClicked);
    connect(openRPCConsoleAction, &QAction::triggered, this, &BurritoCoinGUI::showDebugWindow);
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, &QAction::triggered, rpcConsole, &QWidget::hide);

#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, &QAction::triggered, walletFrame, &WalletFrame::encryptWallet);
        connect(backupWalletAction, &QAction::triggered, walletFrame, &WalletFrame::backupWallet);
        connect(m_show_wallet_location_action, &QAction::triggered, this, &BurritoCoinGUI::showWalletLocation);
        connect(m_verify_backup_key_action, &QAction::triggered, this, &BurritoCoinGUI::showVerifyBackupKey);
        connect(changePassphraseAction, &QAction::triggered, walletFrame, &WalletFrame::changePassphrase);
        connect(signMessageAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
        connect(signMessageAction, &QAction::triggered, [this]{ gotoSignMessageTab(); });
        connect(m_load_psbt_action, &QAction::triggered, [this]{ gotoLoadPSBT(); });
        connect(m_load_psbt_clipboard_action, &QAction::triggered, [this]{ gotoLoadPSBT(true); });
        connect(verifyMessageAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
        connect(verifyMessageAction, &QAction::triggered, [this]{ gotoVerifyMessageTab(); });
        connect(usedSendingAddressesAction, &QAction::triggered, walletFrame, &WalletFrame::usedSendingAddresses);
        connect(usedReceivingAddressesAction, &QAction::triggered, walletFrame, &WalletFrame::usedReceivingAddresses);
        connect(openAction, &QAction::triggered, this, &BurritoCoinGUI::openClicked);
        connect(m_open_wallet_menu, &QMenu::aboutToShow, [this] {
            m_open_wallet_menu->clear();
            for (const std::pair<const std::string, bool>& i : m_wallet_controller->listWalletDir()) {
                const std::string& path = i.first;
                QString name = path.empty() ? QString("["+tr("default wallet")+"]") : QString::fromStdString(path);
                // Menu items remove single &. Single & are shown when && is in
                // the string, but only the first occurrence. So replace only
                // the first & with &&.
                name.replace(name.indexOf(QChar('&')), 1, QString("&&"));
                QAction* action = m_open_wallet_menu->addAction(name);

                if (i.second) {
                    // This wallet is already loaded
                    action->setEnabled(false);
                    continue;
                }

                connect(action, &QAction::triggered, [this, path] {
                    auto activity = new OpenWalletActivity(m_wallet_controller, this);
                    connect(activity, &OpenWalletActivity::opened, this, &BurritoCoinGUI::setCurrentWallet);
                    connect(activity, &OpenWalletActivity::finished, activity, &QObject::deleteLater);
                    activity->open(path);
                });
            }
            if (m_open_wallet_menu->isEmpty()) {
                QAction* action = m_open_wallet_menu->addAction(tr("No wallets available"));
                action->setEnabled(false);
            }
        });
        connect(m_close_wallet_action, &QAction::triggered, [this] {
            m_wallet_controller->closeWallet(walletFrame->currentWalletModel(), this);
        });
        connect(m_create_wallet_action, &QAction::triggered, [this] {
            auto activity = new CreateWalletActivity(m_wallet_controller, this);
            connect(activity, &CreateWalletActivity::created, this, &BurritoCoinGUI::setCurrentWallet);
            connect(activity, &CreateWalletActivity::finished, activity, &QObject::deleteLater);
            activity->create();
        });
        connect(m_restore_wallet_action, &QAction::triggered, this, &BurritoCoinGUI::restoreWalletFromBackup);
        connect(m_close_all_wallets_action, &QAction::triggered, [this] {
            m_wallet_controller->closeAllWallets(this);
        });
        connect(m_mask_values_action, &QAction::toggled, this, &BurritoCoinGUI::setPrivacy);
    }
#endif // ENABLE_WALLET

    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this), &QShortcut::activated, this, &BurritoCoinGUI::showDebugWindowActivateConsole);
    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this), &QShortcut::activated, this, &BurritoCoinGUI::showDebugWindow);
}

void BurritoCoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        file->addAction(m_create_wallet_action);
        file->addAction(m_open_wallet_action);
        file->addAction(m_restore_wallet_action);
        file->addAction(m_close_wallet_action);
        file->addAction(m_close_all_wallets_action);
        file->addSeparator();
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(m_show_wallet_location_action);
        file->addAction(m_verify_backup_key_action);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addAction(m_load_psbt_action);
        file->addAction(m_load_psbt_clipboard_action);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addSeparator();
        settings->addAction(m_mask_values_action);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    QMenu* window_menu = appMenuBar->addMenu(tr("&Window"));

    QAction* minimize_action = window_menu->addAction(tr("Minimize"));
    minimize_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));
    connect(minimize_action, &QAction::triggered, [] {
        QApplication::activeWindow()->showMinimized();
    });
    connect(qApp, &QApplication::focusWindowChanged, [minimize_action] (QWindow* window) {
        minimize_action->setEnabled(window != nullptr && (window->flags() & Qt::Dialog) != Qt::Dialog && window->windowState() != Qt::WindowMinimized);
    });

#ifdef Q_OS_MAC
    QAction* zoom_action = window_menu->addAction(tr("Zoom"));
    connect(zoom_action, &QAction::triggered, [] {
        QWindow* window = qApp->focusWindow();
        if (window->windowState() != Qt::WindowMaximized) {
            window->showMaximized();
        } else {
            window->showNormal();
        }
    });

    connect(qApp, &QApplication::focusWindowChanged, [zoom_action] (QWindow* window) {
        zoom_action->setEnabled(window != nullptr);
    });
#endif

    if (walletFrame) {
#ifdef Q_OS_MAC
        window_menu->addSeparator();
        QAction* main_window_action = window_menu->addAction(tr("Main Window"));
        connect(main_window_action, &QAction::triggered, [this] {
            GUIUtil::bringToFront(this);
        });
#endif
        window_menu->addSeparator();
        window_menu->addAction(usedSendingAddressesAction);
        window_menu->addAction(usedReceivingAddressesAction);
    }

    if (m_faq_dock) {
        window_menu->addSeparator();
        window_menu->addAction(m_faq_dock->toggleViewAction());
    }

    window_menu->addSeparator();
    for (RPCConsole::TabTypes tab_type : rpcConsole->tabs()) {
        QAction* tab_action = window_menu->addAction(rpcConsole->tabTitle(tab_type));
        tab_action->setShortcut(rpcConsole->tabShortcut(tab_type));
        connect(tab_action, &QAction::triggered, [this, tab_type] {
            rpcConsole->setTabFocus(tab_type);
            showDebugWindow();
        });
    }

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    if (walletFrame) {
        help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BurritoCoinGUI::createToolBars()
{
    if(walletFrame)
    {
        QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
        appToolBar = toolbar;
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        toolbar->setMovable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);
        toolbar->addAction(mineAction);
        overviewAction->setChecked(true);

#ifdef ENABLE_WALLET
        QWidget *spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        toolbar->addWidget(spacer);

        m_wallet_selector = new QComboBox();
        m_wallet_selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        connect(m_wallet_selector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &BurritoCoinGUI::setCurrentWalletBySelectorIndex);

        m_wallet_selector_label = new QLabel();
        m_wallet_selector_label->setText(tr("Wallet:") + " ");
        m_wallet_selector_label->setBuddy(m_wallet_selector);

        m_wallet_selector_label_action = appToolBar->addWidget(m_wallet_selector_label);
        m_wallet_selector_action = appToolBar->addWidget(m_wallet_selector);

        m_wallet_selector_label_action->setVisible(false);
        m_wallet_selector_action->setVisible(false);
#endif
    }
}

void BurritoCoinGUI::setClientModel(ClientModel *_clientModel, interfaces::BlockAndHeaderTipInfo* tip_info)
{
    this->clientModel = _clientModel;
    if(_clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(_clientModel, &ClientModel::numConnectionsChanged, this, &BurritoCoinGUI::setNumConnections);
        connect(_clientModel, &ClientModel::networkActiveChanged, this, &BurritoCoinGUI::setNetworkActive);

        modalOverlay->setKnownBestHeight(tip_info->header_height, QDateTime::fromTime_t(tip_info->header_time));
        setNumBlocks(tip_info->block_height, QDateTime::fromTime_t(tip_info->block_time), tip_info->verification_progress, false, SynchronizationState::INIT_DOWNLOAD);
        connect(_clientModel, &ClientModel::numBlocksChanged, this, &BurritoCoinGUI::setNumBlocks);

        // Receive and report messages from client model
        connect(_clientModel, &ClientModel::message, [this](const QString &title, const QString &message, unsigned int style){
            this->message(title, message, style);
        });

        // Show progress dialog
        connect(_clientModel, &ClientModel::showProgress, this, &BurritoCoinGUI::showProgress);

        rpcConsole->setClientModel(_clientModel, tip_info->block_height, tip_info->block_time, tip_info->verification_progress);

        updateProxyIcon();

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(_clientModel->getOptionsModel());

        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if (optionsModel && trayIcon) {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel, &OptionsModel::hideTrayIconChanged, this, &BurritoCoinGUI::setTrayIconVisible);

            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(nullptr);
    }
}

#ifdef ENABLE_WALLET
void BurritoCoinGUI::setWalletController(WalletController* wallet_controller)
{
    assert(!m_wallet_controller);
    assert(wallet_controller);

    m_wallet_controller = wallet_controller;

    m_create_wallet_action->setEnabled(true);
    m_open_wallet_action->setEnabled(true);
    m_open_wallet_action->setMenu(m_open_wallet_menu);
    m_restore_wallet_action->setEnabled(true);

    connect(wallet_controller, &WalletController::walletAdded, this, &BurritoCoinGUI::addWallet);
    connect(wallet_controller, &WalletController::walletRemoved, this, &BurritoCoinGUI::removeWallet);

    for (WalletModel* wallet_model : m_wallet_controller->getOpenWallets()) {
        addWallet(wallet_model);
    }

    // First-run onboarding for brand-new setups with no wallet yet. Deferred so
    // it appears after the main window is visible.
    QTimer::singleShot(600, this, &BurritoCoinGUI::maybeShowOnboarding);
}

WalletController* BurritoCoinGUI::getWalletController()
{
    return m_wallet_controller;
}

void BurritoCoinGUI::maybeShowOnboarding()
{
    if (!enableWallet || !walletFrame || !m_wallet_controller) return;

    QSettings settings;
    if (settings.value(QStringLiteral("onboarding/done"), false).toBool()) return;

    // Anyone who already has a wallet open is set up — record that and move on.
    if (!m_wallet_controller->getOpenWallets().empty()) {
        settings.setValue(QStringLiteral("onboarding/done"), true);
        return;
    }

    // Mark done up front so we never nag again, whatever the user chooses here.
    settings.setValue(QStringLiteral("onboarding/done"), true);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Welcome to BurritoCoin"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr(
        "<p style='font-size:13pt;'><b>\xf0\x9f\x8c\xaf Welcome to BurritoCoin!</b></p>"
        "<p>This app is your wallet \xe2\x80\x94 it lets you receive and send BRTO and keeps your "
        "coins safe on your own computer.</p>"
        "<p>Let's set you up:</p>"
        "<ol>"
        "<li><b>Create a wallet</b> (you can protect it with a passphrase).</li>"
        "<li><b>Back it up</b> \xe2\x80\x94 we'll remind you right after and show you exactly where "
        "your wallet file lives.</li>"
        "</ol>"
        "<p>You can always find guidance in the <b>Help &amp; FAQ</b> panel on the right.</p>"));
    QPushButton* create_btn = box.addButton(tr("Create My Wallet"), QMessageBox::AcceptRole);
    box.addButton(tr("I'll do it later"), QMessageBox::RejectRole);
    box.setDefaultButton(create_btn);
    box.exec();

    if (box.clickedButton() == create_btn) {
        // Reuse the standard wallet-creation flow. Once the wallet is created and
        // made current, the existing first-run backup reminder guides the backup.
        auto* activity = new CreateWalletActivity(m_wallet_controller, this);
        connect(activity, &CreateWalletActivity::created, this, &BurritoCoinGUI::setCurrentWallet);
        connect(activity, &CreateWalletActivity::finished, activity, &QObject::deleteLater);
        activity->create();
    }
}

void BurritoCoinGUI::addWallet(WalletModel* walletModel)
{
    if (!walletFrame) return;
    if (!walletFrame->addWallet(walletModel)) return;
    rpcConsole->addWallet(walletModel);
    if (m_wallet_selector->count() == 0) {
        setWalletActionsEnabled(true);
    } else if (m_wallet_selector->count() == 1) {
        m_wallet_selector_label_action->setVisible(true);
        m_wallet_selector_action->setVisible(true);
    }
    const QString display_name = walletModel->getDisplayName();
    m_wallet_selector->addItem(display_name, QVariant::fromValue(walletModel));
}

void BurritoCoinGUI::removeWallet(WalletModel* walletModel)
{
    if (!walletFrame) return;

    labelWalletHDStatusIcon->hide();
    labelWalletEncryptionIcon->hide();
    if (m_backup_status_label) m_backup_status_label->hide();

    int index = m_wallet_selector->findData(QVariant::fromValue(walletModel));
    m_wallet_selector->removeItem(index);
    if (m_wallet_selector->count() == 0) {
        setWalletActionsEnabled(false);
        overviewAction->setChecked(true);
    } else if (m_wallet_selector->count() == 1) {
        m_wallet_selector_label_action->setVisible(false);
        m_wallet_selector_action->setVisible(false);
    }
    rpcConsole->removeWallet(walletModel);
    walletFrame->removeWallet(walletModel);
    updateWindowTitle();
    // Removing a wallet may have switched the active wallet; refresh the backup
    // badge for whichever one is current now (or hide it when none remain).
    setBackupStatus(walletFrame->currentWalletModel());
}

void BurritoCoinGUI::setCurrentWallet(WalletModel* wallet_model)
{
    if (!walletFrame) return;
    walletFrame->setCurrentWallet(wallet_model);
    for (int index = 0; index < m_wallet_selector->count(); ++index) {
        if (m_wallet_selector->itemData(index).value<WalletModel*>() == wallet_model) {
            m_wallet_selector->setCurrentIndex(index);
            break;
        }
    }
    updateWindowTitle();

    // Once per launch, gently remind the user to keep a backup. Deferred so the
    // main window finishes painting before any reminder dialog appears.
    if (!m_safety_reminders_done && wallet_model) {
        m_safety_reminders_done = true;
        QPointer<WalletModel> wm(wallet_model);
        QTimer::singleShot(900, this, [this, wm] {
            if (wm) showWalletSafetyReminders(wm);
        });
    }
}

void BurritoCoinGUI::setCurrentWalletBySelectorIndex(int index)
{
    WalletModel* wallet_model = m_wallet_selector->itemData(index).value<WalletModel*>();
    if (wallet_model) setCurrentWallet(wallet_model);
}

void BurritoCoinGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}

void BurritoCoinGUI::showWalletLocation()
{
    if (!walletFrame) return;
    WalletModel* const wallet_model = walletFrame->currentWalletModel();
    if (!wallet_model) {
        QMessageBox::information(this, tr("Wallet File Location"),
            tr("No wallet is loaded yet, so there is no wallet file to show."));
        return;
    }

    const WalletDiskPaths paths = GetWalletDiskPaths(m_node, wallet_model);
    const bool exists = QFile::exists(paths.file);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Wallet File Location"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr("<p>This wallet's keys are stored in a file named <b>wallet.dat</b>.</p>"
                   "<p><b>Folder:</b><br><code>%1</code></p>"
                   "<p><b>File:</b><br><code>%2</code></p>"
                   "<p>If you ever lose this file <i>and</i> your backups, the coins are gone for "
                   "good. Keep a copy somewhere safe \xe2\x80\x94 a USB stick or external drive, not "
                   "just this computer.</p>")
                   .arg(paths.folder.toHtmlEscaped(), paths.file.toHtmlEscaped()));
    if (!exists) {
        box.setInformativeText(tr("Note: no file was found at the path above yet. The wallet may "
                                  "store its data under a different name, or it may not have been "
                                  "written to disk yet."));
    }

    QPushButton* open_btn = box.addButton(tr("Open Folder"), QMessageBox::ActionRole);
    QPushButton* copy_btn = box.addButton(tr("Copy Path"), QMessageBox::ActionRole);
    QPushButton* backup_btn = box.addButton(tr("Back Up Now..."), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.setDefaultButton(open_btn);
    box.exec();

    QAbstractButton* const clicked = box.clickedButton();
    if (clicked == open_btn) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(paths.folder));
    } else if (clicked == copy_btn) {
        QApplication::clipboard()->setText(paths.file);
    } else if (clicked == backup_btn) {
        walletFrame->backupWallet();
    }
}

void BurritoCoinGUI::showVerifyBackupKey()
{
    if (!walletFrame) return;
    WalletModel* const wallet_model = walletFrame->currentWalletModel();
    if (!wallet_model) {
        QMessageBox::information(this, tr("Verify Backup Key"),
            tr("Open or create a wallet first, then you can check a backed-up key against it."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Verify Backup Key"));
    dlg.setMinimumWidth(480);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* intro = new QLabel(tr(
        "<p><b>Most users don't need this.</b> Your real backup is the "
        "<code>wallet.dat</code> file \xe2\x80\x94 it already contains every key in this wallet.</p>"
        "<p>Use this dialog only if you've separately exported a single private key (WIF, a long "
        "string starting with <b>P</b>) via <code>dumpprivkey</code> in the console and want to confirm "
        "your written or printed copy is correct. The check runs <b>entirely on this computer</b>; "
        "the key is never sent anywhere and is not saved or logged.</p>"), &dlg);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    layout->addWidget(intro);

    QLineEdit* key_edit = new QLineEdit(&dlg);
    key_edit->setEchoMode(QLineEdit::Password);
    key_edit->setPlaceholderText(tr("Private key (WIF) \xe2\x80\x94 not your wallet passphrase"));
    layout->addWidget(key_edit);

    QCheckBox* show_key = new QCheckBox(tr("Show key"), &dlg);
    layout->addWidget(show_key);
    connect(show_key, &QCheckBox::toggled, key_edit, [key_edit](bool on) {
        key_edit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });

    QLabel* result = new QLabel(&dlg);
    result->setWordWrap(true);
    result->setTextFormat(Qt::RichText);
    result->setMinimumHeight(44);
    layout->addWidget(result);

    QDialogButtonBox* buttons = new QDialogButtonBox(&dlg);
    QPushButton* verify_btn = buttons->addButton(tr("Verify"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    auto do_verify = [wallet_model, key_edit, result] {
        const QString wif = key_edit->text().trimmed();
        if (wif.isEmpty()) {
            result->setText(tr("<span style='color:#c47d0e;'>Enter a private key to check.</span>"));
            return;
        }
        switch (wallet_model->checkPrivKeyOwnership(wif)) {
        case WalletModel::KeyInWallet:
            result->setText(tr("<span style='color:#1e8e3e;'>\xe2\x9c\x94 This key belongs to your wallet. "
                               "Your backup is good.</span>"));
            break;
        case WalletModel::KeyNotInWallet:
            result->setText(tr("<span style='color:#c0392b;'>\xe2\x9c\x98 This is a valid private key, but it "
                               "is <b>not</b> in the wallet you have open. Make sure you backed up the right "
                               "wallet, or open the matching wallet and try again.</span>"));
            break;
        case WalletModel::KeyInvalid:
            result->setText(tr("<span style='color:#c0392b;'>\xe2\x9c\x98 That doesn't look like a valid "
                               "BurritoCoin private key (WIF) for this network. Check for typos, or a key from a "
                               "different network.</span>"));
            break;
        }
    };
    connect(verify_btn, &QPushButton::clicked, &dlg, do_verify);
    connect(key_edit, &QLineEdit::returnPressed, &dlg, do_verify);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    dlg.exec();
    // Best-effort scrub of the QLineEdit's heap buffer before clearing.
    // Mirrors SecureClearQLineEdit() in askpassphrasedialog.cpp; intermediate
    // QString copies (e.g. from .trimmed()) are not wiped, but this matches the
    // codebase's established pattern for sensitive input fields.
    key_edit->setText(QString(QLatin1Char(' ')).repeated(key_edit->text().size()));
    key_edit->clear();
}

void BurritoCoinGUI::restoreWalletFromBackup()
{
    if (!walletFrame || !m_wallet_controller) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Restore Wallet from Backup"));
    dlg.setMinimumWidth(560);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* intro = new QLabel(tr(
        "<p>Restore a previously saved <code>wallet.dat</code> backup so it shows up "
        "alongside your other wallets. Your original backup file is <b>not modified</b> \xe2\x80\x94 "
        "a copy is placed in your wallets folder under the name you choose.</p>"
        "<p>If the backup is encrypted, BurritoCoin will ask for the passphrase the first "
        "time you spend from it. The passphrase is <b>not</b> entered here.</p>"), &dlg);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    layout->addWidget(intro);

    // --- Row 1: source backup file -------------------------------------
    QHBoxLayout* file_row = new QHBoxLayout;
    QLabel* file_label = new QLabel(tr("Backup file:"), &dlg);
    QLineEdit* file_edit = new QLineEdit(&dlg);
    file_edit->setPlaceholderText(tr("Path to wallet.dat backup..."));
    QPushButton* browse_btn = new QPushButton(tr("Browse..."), &dlg);
    file_row->addWidget(file_label);
    file_row->addWidget(file_edit, 1);
    file_row->addWidget(browse_btn);
    layout->addLayout(file_row);

    // --- Row 2: target wallet name -------------------------------------
    QHBoxLayout* name_row = new QHBoxLayout;
    QLabel* name_label = new QLabel(tr("Name for this wallet:"), &dlg);
    QLineEdit* name_edit = new QLineEdit(&dlg);
    name_edit->setPlaceholderText(tr("e.g. restored-2026-06"));
    // First char alnum or _ (no leading space/dash/dot); rest may include space;
    // 1..64 chars. Blocks slashes, backslashes and dots at the keystroke level.
    name_edit->setValidator(new QRegExpValidator(
        QRegExp(QStringLiteral("[A-Za-z0-9_][A-Za-z0-9 _-]{0,63}")), &dlg));
    name_row->addWidget(name_label);
    name_row->addWidget(name_edit, 1);
    layout->addLayout(name_row);

    // --- Live red/amber/green feedback (same idiom as showVerifyBackupKey) ---
    QLabel* feedback = new QLabel(&dlg);
    feedback->setWordWrap(true);
    feedback->setTextFormat(Qt::RichText);
    feedback->setMinimumHeight(44);
    layout->addWidget(feedback);

    QDialogButtonBox* buttons = new QDialogButtonBox(&dlg);
    QPushButton* restore_btn = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    restore_btn->setEnabled(false);
    layout->addWidget(buttons);

    QDialog* const dlg_ptr = &dlg;

    // --- Browse handler ------------------------------------------------
    connect(browse_btn, &QPushButton::clicked, dlg_ptr,
            [this, dlg_ptr, file_edit, name_edit]() {
        const QString picked = QFileDialog::getOpenFileName(
            dlg_ptr, tr("Select wallet.dat backup"), QDir::homePath(),
            tr("Wallet backups (wallet.dat *.dat);;All files (*)"));
        if (picked.isEmpty()) return;   // user cancelled the file dialog
        file_edit->setText(QDir::toNativeSeparators(picked));
        if (name_edit->text().isEmpty()) {
            QFileInfo fi(picked);
            QString suggested = fi.dir().dirName();   // parent dir is often the original wallet name
            if (suggested.isEmpty() || suggested == QLatin1String(".")) {
                suggested = QStringLiteral("restored-") +
                            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
            }
            suggested.replace(QRegExp(QStringLiteral("[^A-Za-z0-9 _-]")), QStringLiteral("-"));
            // First char must satisfy the validator (alnum/_).
            if (!suggested.isEmpty() &&
                !QRegExp(QStringLiteral("[A-Za-z0-9_]")).exactMatch(suggested.left(1))) {
                suggested[0] = QLatin1Char('_');
            }
            name_edit->setText(suggested.left(64));
        }
    });

    // --- Live validation: every keystroke ------------------------------
    auto revalidate = [this, file_edit, name_edit, feedback, restore_btn]() {
        restore_btn->setEnabled(false);
        const QString src = file_edit->text().trimmed();
        const QString name = name_edit->text().trimmed();
        if (src.isEmpty() || name.isEmpty()) {
            feedback->clear();
            return;
        }
        // Belt-and-braces against reserved names that slipped past the validator
        // (e.g. paste). Covers wallet.dat, dot-names and Windows DOS device names.
        if (IsReservedWalletName(name)) {
            feedback->setText(tr("<span style='color:#c0392b;'>That name is reserved. Pick another.</span>"));
            return;
        }
        QFileInfo sinfo(src);
        if (!sinfo.exists() || !sinfo.isFile()) {
            feedback->setText(tr("<span style='color:#c0392b;'>That backup file does not exist.</span>"));
            return;
        }
        if (!sinfo.isReadable()) {
            feedback->setText(tr("<span style='color:#c0392b;'>The backup file is not readable. Check its permissions.</span>"));
            return;
        }
        // Collision: currently-LOADED wallets.
        for (WalletModel* wm : m_wallet_controller->getOpenWallets()) {
            if (wm && wm->getWalletName() == name) {
                feedback->setText(tr("<span style='color:#c0392b;'>A wallet named <b>%1</b> is already open. Choose a different name.</span>").arg(name.toHtmlEscaped()));
                return;
            }
        }
        // Collision: ON-DISK. Check listWalletDir() (the recognized-wallet subset)
        // AND QDir::exists (any folder, including stray empty ones from a prior
        // failed restore). Without the second check a non-wallet folder would
        // pass live validation and only trip the accept-time re-check, producing
        // a misleading "appeared while this dialog was open" message.
        const QString wallet_dir = RestoreWalletDir(m_node);
        if (m_wallet_controller->listWalletDir().count(name.toStdString()) ||
            QDir(wallet_dir).exists(name)) {
            feedback->setText(tr("<span style='color:#c0392b;'>A folder named <b>%1</b> already exists in your wallets directory. Pick a different name (the existing wallet will not be overwritten).</span>").arg(name.toHtmlEscaped()));
            return;
        }
        // Signature sniff.
        QString why_not;
        if (!LooksLikeWalletDat(src, &why_not)) {
            feedback->setText(QStringLiteral("<span style='color:#c47d0e;'>%1 %2</span>")
                              .arg(why_not.toHtmlEscaped(), tr("Restore is disabled.").toHtmlEscaped()));
            return;
        }
        feedback->setText(tr("<span style='color:#1e8e3e;'>Ready: a copy of this file will be placed at the new wallet location and loaded.</span>"));
        restore_btn->setEnabled(true);
    };
    connect(file_edit, &QLineEdit::textChanged, dlg_ptr, revalidate);
    connect(name_edit, &QLineEdit::textChanged, dlg_ptr, revalidate);

    // --- Accept handler: re-check, mkpath, copy, hand off ---------------
    connect(buttons, &QDialogButtonBox::accepted, dlg_ptr,
            [this, dlg_ptr, file_edit, name_edit, restore_btn, buttons]() {
        // Disable the whole button row immediately so a double-click / Enter+click
        // can't queue a second accepted signal that re-enters this lambda while
        // QApplication::processEvents() drains during the copy. Re-enable on every
        // error return so the user can fix the input and retry without reopening.
        restore_btn->setEnabled(false);
        buttons->setEnabled(false);
        auto rearm = [restore_btn, buttons]() {
            buttons->setEnabled(true);
            restore_btn->setEnabled(true);
        };

        const QString src = file_edit->text().trimmed();
        const QString name = name_edit->text().trimmed();
        const QString wallet_dir = RestoreWalletDir(m_node);
        QDir wallet_dir_qd(wallet_dir);
        const QString target_folder = wallet_dir_qd.filePath(name);
        const QString target_file = QDir(target_folder).filePath(QStringLiteral("wallet.dat"));

        // Belt-and-braces: re-check reserved name at accept time too, in case the
        // user pasted past the live check.
        if (IsReservedWalletName(name)) {
            QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"),
                tr("That name is reserved. Pick another."));
            rearm();
            return;
        }
        // Re-check: on-disk collision (covers both pre-existing folders the live
        // sniff missed and a folder that genuinely appeared during the dialog).
        if (wallet_dir_qd.exists(name) ||
            m_wallet_controller->listWalletDir().count(name.toStdString())) {
            QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"),
                tr("A folder named <b>%1</b> already exists in your wallets directory. Pick a different name (the existing wallet will not be overwritten).").arg(name.toHtmlEscaped()));
            rearm();
            return;
        }
        // Re-check: loaded-wallet collision (a parallel Open Wallet could have completed).
        for (WalletModel* wm : m_wallet_controller->getOpenWallets()) {
            if (wm && wm->getWalletName() == name) {
                QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"),
                    tr("A wallet named <b>%1</b> was opened while this dialog was running. Pick a different name.").arg(name.toHtmlEscaped()));
                rearm();
                return;
            }
        }
        // Re-check: source still readable and still has a wallet signature.
        const QFileInfo sinfo(src);
        if (!sinfo.exists() || !sinfo.isFile() || !sinfo.isReadable()) {
            QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"),
                tr("The backup file <code>%1</code> is no longer readable.")
                    .arg(QDir::toNativeSeparators(src).toHtmlEscaped()));
            rearm();
            return;
        }
        QString why_not;
        if (!LooksLikeWalletDat(src, &why_not)) {
            QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"), why_not);
            rearm();
            return;
        }
        // Create wallets/<name>/.
        if (!wallet_dir_qd.mkpath(name)) {
            QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"),
                tr("Could not create the folder <code>%1</code>. Check that you have write permission to the wallets directory.")
                    .arg(QDir::toNativeSeparators(target_folder).toHtmlEscaped()));
            rearm();
            return;
        }
        // Copy under an indeterminate progress indicator so multi-MB backups don't
        // look like a UI freeze. QFile::copy refuses to overwrite -- exactly what
        // we want. Buttons are already disabled, so the processEvents() below
        // cannot re-enter this accept handler.
        QProgressDialog progress(tr("Copying backup..."), QString(), 0, 0, dlg_ptr);
        progress.setWindowModality(Qt::ApplicationModal);
        progress.setCancelButton(nullptr);   // copy is not interruptible; hide the button
        progress.setMinimumDuration(0);
        progress.show();
        QApplication::processEvents();
        const bool copied = QFile::copy(src, target_file);
        progress.close();
        if (!copied) {
            // Best-effort rollback: remove the empty folder stub mkpath() created so
            // a future restore with the same name isn't permanently blocked. Use
            // rmdir() (NOT removeRecursively): it only removes an EMPTY directory, so
            // in the exotic case where something raced a real wallet.dat into the
            // folder during the copy, this is a harmless no-op rather than data loss.
            wallet_dir_qd.rmdir(name);
            QMessageBox::critical(dlg_ptr, tr("Restore wallet failed"),
                tr("Could not copy the backup to <code>%1</code>. Your original backup file was not modified.")
                    .arg(QDir::toNativeSeparators(target_file).toHtmlEscaped()));
            rearm();
            return;
        }

        dlg_ptr->accept();   // close wizard BEFORE kicking off the async load

        // Hand off to the exact same activity the Open Wallet submenu uses. Any
        // loadWallet() error is surfaced by OpenWalletActivity::finish() with its
        // own QMessageBox::critical, so we don't layer another dialog. The copied
        // wallets/<name>/wallet.dat is intentionally left on disk on load failure
        // so the user can inspect/retry.
        auto* activity = new OpenWalletActivity(m_wallet_controller, this);
        connect(activity, &OpenWalletActivity::opened, this, &BurritoCoinGUI::setCurrentWallet);
        connect(activity, &OpenWalletActivity::finished, activity, &QObject::deleteLater);
        activity->open(name.toStdString());
    });

    connect(buttons, &QDialogButtonBox::rejected, dlg_ptr, &QDialog::reject);

    dlg.exec();   // Cancel / Esc / window close -> nothing written, no rollback needed.
}

void BurritoCoinGUI::showWalletSafetyReminders(WalletModel* wallet_model)
{
    if (!wallet_model) return;

    // Count how many times a wallet has been opened so we can pace the prompts.
    QSettings settings;
    const int opens = settings.value("safety/walletOpenCount", 0).toInt() + 1;
    settings.setValue("safety/walletOpenCount", opens);

    const WalletDiskPaths paths = GetWalletDiskPaths(m_node, wallet_model);
    // Only assert the exact wallet.dat path if it really exists on disk; otherwise
    // point at the folder, so the reminder never names a file that isn't there.
    const bool file_exists = paths.valid && QFile::exists(paths.file);
    const QString file_html = file_exists ? paths.file.toHtmlEscaped()
                            : (paths.valid ? paths.folder.toHtmlEscaped() : tr("(unknown)"));

    const bool first_run = (opens == 1);
    // Back-up nudge on the very first open, then every third open after that.
    const bool backup_due = first_run || (opens % 3 == 0);
    // Paper-seed nudge every fourth open, but only for spendable HD wallets.
    const bool seed_due = (opens % 4 == 0) && wallet_model->wallet().hdEnabled()
                          && !wallet_model->wallet().privateKeysDisabled();

    if (backup_due) {
        QMessageBox box(this);
        box.setIcon(first_run ? QMessageBox::Information : QMessageBox::Warning);
        box.setWindowTitle(first_run ? tr("Welcome \xe2\x80\x94 keep your coins safe")
                                     : tr("Have you backed up your wallet?"));
        box.setTextFormat(Qt::RichText);
        box.setText(tr("<p><b>Your coins live in a file named wallet.dat.</b> If you lose that file "
                       "and have no backup, the coins are gone for good \xe2\x80\x94 there is no "
                       "password reset and no support line that can bring them back.</p>"
                       "<p>Your wallet file is here:</p><p><code>%1</code></p>"
                       "<p>Copy it somewhere safe \xe2\x80\x94 ideally a USB stick or external drive "
                       "kept away from this computer. If your wallet is encrypted, write the "
                       "passphrase on paper too; the backup is useless without it.</p>").arg(file_html));
        QPushButton* backup_btn = box.addButton(tr("Back Up Now..."), QMessageBox::AcceptRole);
        QPushButton* locate_btn = box.addButton(tr("Show Me the File"), QMessageBox::ActionRole);
        QPushButton* verify_btn = box.addButton(tr("Verify a Key..."), QMessageBox::ActionRole);
        // "I've Already Backed Up" must be an explicit click — keep it ActionRole.
        // The RejectRole "Remind Me Later" button is what Esc / window-close maps
        // to, so dismissing the dialog never silently marks the wallet backed up.
        QPushButton* acked_btn = box.addButton(tr("I've Already Backed Up"), QMessageBox::ActionRole);
        box.addButton(tr("Remind Me Later"), QMessageBox::RejectRole);
        box.setDefaultButton(backup_btn);
        box.exec();

        QAbstractButton* const clicked = box.clickedButton();
        if (clicked == backup_btn) {
            if (walletFrame) walletFrame->backupWallet();
        } else if (clicked == locate_btn && paths.valid) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(paths.folder));
        } else if (clicked == verify_btn) {
            showVerifyBackupKey();
        } else if (clicked == acked_btn) {
            // Take the user at their word and reflect it in the status badge.
            QSettings settings;
            settings.setValue(QStringLiteral("wallet/") + wallet_model->getWalletName() + QStringLiteral("/backup_done"), true);
            setBackupStatus(wallet_model);
        }
    }

    if (seed_due) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("Save a paper backup of your recovery seed"));
        box.setTextFormat(Qt::RichText);
        box.setText(tr("<p>For an extra layer of safety, keep an <b>offline paper backup</b> of your "
                       "wallet's recovery secret. A printed or hand-written copy survives a dead hard "
                       "drive, a lost laptop, or ransomware.</p>"
                       "<p><b>How to export it:</b> open <i>Help \xe2\x80\xba Node window</i>, choose "
                       "the <i>Console</i> tab, and run <code>dumpwallet</code> with a full path to a "
                       "file you choose \xe2\x80\x94 for example:</p>"
                       "<p><code>dumpwallet \"C:\\Users\\You\\burritocoin-backup.txt\"</code> (Windows)<br>"
                       "<code>dumpwallet \"/home/you/burritocoin-backup.txt\"</code> (macOS/Linux)</p>"
                       "<p>That file contains your private keys and HD seed. Print it, store it "
                       "somewhere safe and offline, then delete the file from your computer. Anyone "
                       "who reads it can take your coins, so treat it like cash.</p>"));
        QPushButton* console_btn = box.addButton(tr("Open Node Window"), QMessageBox::ActionRole);
        box.addButton(tr("I've Saved It"), QMessageBox::AcceptRole);
        box.addButton(tr("Remind Me Later"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == console_btn) {
            showDebugWindowActivateConsole();
        }
    }
}
#endif // ENABLE_WALLET

void BurritoCoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    mineAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    m_show_wallet_location_action->setEnabled(enabled);
    m_verify_backup_key_action->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
    m_close_wallet_action->setEnabled(enabled);
    m_close_all_wallets_action->setEnabled(enabled);
}

void BurritoCoinGUI::createSafetyTipBanner()
{
    // Only meaningful when a wallet can be loaded.
    if (!enableWallet) return;

    // The set of rotating reminders. Static so the timer lambda below can keep
    // referencing them for the lifetime of the window.
    static const QStringList tips = {
        tr("\xf0\x9f\x8c\xaf  Losing your wallet.dat file means losing access to your coins \xe2\x80\x94 back it up."),
        tr("\xf0\x9f\x94\x92  Write your wallet passphrase on paper. Forget it and no one can recover your coins."),
        tr("\xf0\x9f\x93\x84  Keep an offline backup of your recovery seed \xe2\x80\x94 it can restore your whole wallet."),
        tr("\xf0\x9f\x92\xbe  Back up wallet.dat to a USB stick or external drive, not just this computer."),
        tr("\xf0\x9f\x97\x82  File \xe2\x80\xba Show Wallet File Location shows you exactly where wallet.dat lives."),
    };

    m_safety_tip_label = new QLabel(tips.first(), statusBar());
    m_safety_tip_label->setObjectName("safetyTipLabel");
    m_safety_tip_label->setTextFormat(Qt::PlainText);
    m_safety_tip_label->setToolTip(tr("Tips for keeping your BurritoCoin safe"));
    m_safety_tip_label->setStyleSheet("QLabel#safetyTipLabel { color: #c47d0e; padding-left: 6px; }");
    m_safety_tip_label->setProperty("tipIndex", 0);
    // Let the tip clip rather than widen the window's minimum size.
    m_safety_tip_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_safety_tip_label->setMinimumWidth(0);
    statusBar()->addWidget(m_safety_tip_label, 1);

    QTimer* tip_timer = new QTimer(this);
    tip_timer->setInterval(12000);
    connect(tip_timer, &QTimer::timeout, m_safety_tip_label, [this] {
        if (!m_safety_tip_label) return;
        const int next = (m_safety_tip_label->property("tipIndex").toInt() + 1) % tips.size();
        m_safety_tip_label->setProperty("tipIndex", next);
        m_safety_tip_label->setText(tips.at(next));
    });
    tip_timer->start();
}

void BurritoCoinGUI::createFaqPanel()
{
    // Only meaningful when a wallet can be loaded.
    if (!enableWallet) return;

    m_faq_dock = new QDockWidget(tr("Help & FAQ"), this);
    m_faq_dock->setObjectName("FAQDock");
    m_faq_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // The dock's window title is reused verbatim for toggleViewAction()'s text,
    // and the menu renderer strips '&' as a mnemonic prefix — escape it as '&&'
    // so the Window menu entry shows "Help & FAQ" instead of "Help  FAQ".
    m_faq_dock->toggleViewAction()->setText(tr("Help && FAQ"));

    // NOTE: this depends-Qt is built without the 'textbrowser' feature, so we use
    // a read-only QTextEdit (the 'textedit' feature is enabled) for the rich text.
    QTextEdit* faq = new QTextEdit(m_faq_dock);
    faq->setReadOnly(true);
    faq->setFrameStyle(QFrame::NoFrame);
    faq->setStyleSheet(
        "QTextEdit { background: #1a0f05; color: #d6c4a3; border: none; padding: 12px; }");
    faq->setHtml(faqHtml());

    m_faq_dock->setWidget(faq);
    m_faq_dock->setMinimumWidth(280);
    addDockWidget(Qt::RightDockWidgetArea, m_faq_dock);
    // Give it a sensible starting width so it does not crowd the wallet view.
    // resizeDocks() only takes effect once the main-window layout is active, so
    // defer it until after the window has been shown.
    QTimer::singleShot(0, this, [this] {
        if (m_faq_dock) resizeDocks({m_faq_dock}, {340}, Qt::Horizontal);
    });
}

QString BurritoCoinGUI::faqHtml() const
{
    // Q&A content fact-checked against the wallet source: legacy HD wallets,
    // no BIP39 mnemonic, the HD seed only restores HD-derived keys (not imported
    // keys) and needs a rescan, encryption is irreversible and does not rotate
    // the seed, and only a current wallet.dat is a complete backup.
    struct QA { const char* q; const char* a; };
    const QA items[] = {
        {QT_TR_NOOP("Where is my wallet.dat file saved?"),
         QT_TR_NOOP("Your coins are controlled by keys inside a file called <b>wallet.dat</b>. "
            "Use <b>File &#8250; Show Wallet File Location\xe2\x80\xa6</b> to see the exact folder and "
            "to open it or copy the path. On Windows it is normally under "
            "<code>%APPDATA%\\BurritoCoin</code>, on macOS under "
            "<code>~/Library/Application Support/BurritoCoin</code>, and on Linux under "
            "<code>~/.burritocoin</code> (named wallets live in a <code>wallets/&lt;name&gt;</code> "
            "sub-folder).")},
        {QT_TR_NOOP("What happens if I lose wallet.dat?"),
         QT_TR_NOOP("If you lose wallet.dat and have no backup, the coins are gone for good \xe2\x80\x94 "
            "no one, including the BurritoCoin team, can recover them. The coins still exist on the "
            "blockchain, but without the keys nobody can spend them. This is why a current backup "
            "matters so much.")},
        {QT_TR_NOOP("If I lose wallet.dat, can my recovery seed rebuild the wallet?"),
         QT_TR_NOOP("Only partly. New wallets are HD, so the single master <b>HD seed</b> can "
            "re-derive the addresses the wallet made automatically \xe2\x80\x94 but you must load the "
            "seed back in and then run a blockchain <b>rescan</b> to find past transactions. The seed "
            "only looks ahead a limited number of addresses (the keypool, default 1000), so a "
            "heavily-used wallet may need repeated top-ups. Most importantly, the seed does <b>not</b> "
            "restore keys you imported yourself, watch-only addresses, or your labels. For that reason "
            "a full wallet.dat backup is the only complete backup.")},
        {QT_TR_NOOP("What is the HD seed, and how do I make a paper backup?"),
         QT_TR_NOOP("The HD seed is one master private key stored inside wallet.dat. BurritoCoin does "
            "<b>not</b> use a 12- or 24-word phrase \xe2\x80\x94 there is no BIP39 mnemonic. To make a "
            "readable backup, open <b>Help &#8250; Node window &#8250; Console</b> and run "
            "<code>dumpwallet</code> with a full path to a file you choose &#8212; for example "
            "<code>dumpwallet \"C:\\Users\\You\\burritocoin-backup.txt\"</code> on Windows, or "
            "<code>dumpwallet \"/home/you/burritocoin-backup.txt\"</code> on macOS/Linux. That file "
            "lists all your private keys plus the seed; print it, store it offline, then delete the "
            "file. Treat it like cash \xe2\x80\x94 anyone who reads it can take your coins.")},
        {QT_TR_NOOP("What does encrypting my wallet do? What if I forget the passphrase?"),
         QT_TR_NOOP("Encryption locks your keys with a passphrase, so coins can't be spent until you "
            "unlock the wallet. There is no back door: if you forget the passphrase, the coins are "
            "<b>permanently lost</b>. Encrypting also does not change your seed, so any <i>older, "
            "unencrypted</i> wallet.dat copy can still spend your coins with no passphrase \xe2\x80\x94 "
            "after you encrypt, destroy or re-secure old backups and make a fresh backup of the "
            "encrypted file.")},
        {QT_TR_NOOP("My wallet is gone \xe2\x80\x94 how do I restore from a wallet.dat backup?"),
         QT_TR_NOOP("Quit BurritoCoin Core completely (the file is locked while it is running). In "
            "Explorer / Finder open the folder shown by <b>File &#8250; Show Wallet File Location\xe2\x80\xa6</b> "
            "and make a new sibling sub-folder there with any name (for example "
            "<code>MyWallet-Restored</code>). Copy your backup file into it and rename the copy to "
            "exactly <code>wallet.dat</code>. Relaunch BurritoCoin Core, choose <b>File &#8250; Open "
            "Wallet</b> and pick the new folder. If the wallet was encrypted, enter the original "
            "passphrase \xe2\x80\x94 the passphrase was never inside the file.")},
        {QT_TR_NOOP("How do I know the restore actually worked?"),
         QT_TR_NOOP("Open the restored wallet and check the obvious things: your <b>balance</b> matches "
            "what you remember, your previously-created addresses are listed under the <b>Receive</b> "
            "tab with their labels, and your history appears on the <b>Transactions</b> tab. If something "
            "looks missing, open <b>Help &#8250; Node window &#8250; Console</b> and run "
            "<code>rescanblockchain</code> \xe2\x80\x94 then wait for the scan to finish.")},
        {QT_TR_NOOP("I have wallet.dat but forgot the passphrase. Can I recover the coins?"),
         QT_TR_NOOP("<b>No.</b> BurritoCoin Core's encryption has no back door and there is no recovery "
            "service. Anyone offering to \"recover\" or \"reset\" a forgotten passphrase is running a "
            "scam \xe2\x80\x94 do not send them your wallet file, screenshots, or any payment. The only "
            "real hope is remembering enough fragments to brute-force the passphrase locally with a tool "
            "you trust, on a computer you control.")},
        {QT_TR_NOOP("I only have paper backups of private keys (WIFs) \xe2\x80\x94 how do I restore?"),
         QT_TR_NOOP("This is the advanced path. Create a fresh empty wallet, open <b>Help &#8250; Node "
            "window &#8250; Console</b>, unlock if encrypted, then for each key run "
            "<code>importprivkey \"&lt;WIF&gt;\" \"&lt;label&gt;\" true</code> \xe2\x80\x94 the final "
            "<code>true</code> triggers a rescan to find that key's past transactions. If your backup "
            "is a <code>dumpwallet</code> text file (a file listing every key plus the seed), use "
            "<code>importwallet \"&lt;full-path&gt;\"</code> once. These steps need a full (un-pruned) "
            "blockchain.")},
        {QT_TR_NOOP("Should I use \"Verify Backup Key\" to test my wallet.dat backup?"),
         QT_TR_NOOP("No \xe2\x80\x94 that dialog is for a different, narrow scenario. It only checks a "
            "single private key (WIF) you wrote down separately. Most people never have those: a "
            "<code>wallet.dat</code> backup already contains every key. The dialog is useful if you "
            "exported one specific key via <code>dumpprivkey</code> and want to confirm your written or "
            "printed copy is correct \xe2\x80\x94 the check runs entirely on your own computer, without "
            "re-exposing the key. To verify a <code>wallet.dat</code> backup instead, just open it and "
            "see if your balance, addresses and history are there.")},
        {QT_TR_NOOP("How do I make a good backup, and where should I keep it?"),
         QT_TR_NOOP("Back up the whole, current wallet.dat \xe2\x80\x94 it includes your seed, every "
            "imported key, and your labels. Make a fresh backup after you import a key, after you "
            "encrypt, or after anything important. Keep at least two copies in separate safe places "
            "(for example an encrypted USB drive plus a printed copy), keep them offline, and re-secure "
            "any old unencrypted copies.")},
    };

    QString body =
        "<div style='font-size:13px;'>"
        "<h2 style='color:#f5a623; margin-bottom:2px;'>\xf0\x9f\x8c\xaf BurritoCoin \xe2\x80\x94 Help &amp; FAQ</h2>"
        "<p style='color:#c47d0e; margin-top:0;'>Keeping your coins safe.</p>";
    for (const QA& item : items) {
        body += "<p style='color:#f5a623; font-weight:bold; margin-bottom:2px;'>"
              + tr(item.q) + "</p>";
        body += "<p style='color:#d6c4a3; margin-top:0;'>" + tr(item.a) + "</p>";
    }
    body += "</div>";
    return body;
}

void BurritoCoinGUI::createTrayIcon()
{
    assert(QSystemTrayIcon::isSystemTrayAvailable());

#ifndef Q_OS_MAC
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon = new QSystemTrayIcon(m_network_style->getTrayAndWindowIcon(), this);
        QString toolTip = tr("%1 client").arg(PACKAGE_NAME) + " " + m_network_style->getTitleAddText();
        trayIcon->setToolTip(toolTip);
    }
#endif
}

void BurritoCoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-macOSes)
    if (!trayIcon)
        return;

    trayIcon->setContextMenu(trayIconMenu.get());
    connect(trayIcon, &QSystemTrayIcon::activated, this, &BurritoCoinGUI::trayIconActivated);
#else
    // Note: On macOS, the Dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, &MacDockIconHandler::dockIconClicked, this, &BurritoCoinGUI::macosDockIconActivated);
    trayIconMenu->setAsDockMenu();
#endif

    // Configuration of the tray icon (or Dock icon) menu
#ifndef Q_OS_MAC
    // Note: On macOS, the Dock icon's menu already has Show / Hide action.
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
#endif
    if (enableWallet) {
        trayIconMenu->addAction(sendCoinsMenuAction);
        trayIconMenu->addAction(receiveCoinsMenuAction);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(signMessageAction);
        trayIconMenu->addAction(verifyMessageAction);
        trayIconMenu->addSeparator();
    }
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on macOS
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BurritoCoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#else
void BurritoCoinGUI::macosDockIconActivated()
{
    show();
    activateWindow();
}
#endif

void BurritoCoinGUI::optionsClicked()
{
    openOptionsDialogWithTab(OptionsDialog::TAB_MAIN);
}

void BurritoCoinGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void BurritoCoinGUI::showDebugWindow()
{
    GUIUtil::bringToFront(rpcConsole);
    Q_EMIT consoleShown(rpcConsole);
}

void BurritoCoinGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TabTypes::CONSOLE);
    showDebugWindow();
}

void BurritoCoinGUI::showHelpMessageClicked()
{
    GUIUtil::bringToFront(helpMessageDialog);
}

#ifdef ENABLE_WALLET
void BurritoCoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void BurritoCoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void BurritoCoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void BurritoCoinGUI::gotoMiningPage()
{
    mineAction->setChecked(true);
    if (walletFrame) walletFrame->gotoMiningPage();
}

void BurritoCoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void BurritoCoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void BurritoCoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void BurritoCoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
void BurritoCoinGUI::gotoLoadPSBT(bool from_clipboard)
{
    if (walletFrame) walletFrame->gotoLoadPSBT(from_clipboard);
}
#endif // ENABLE_WALLET

void BurritoCoinGUI::updateNetworkState()
{
    int count = clientModel->getNumConnections();
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }

    QString tooltip;

    if (m_node.getNetworkActive()) {
        tooltip = tr("%n active connection(s) to BurritoCoin network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_disabled";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");
    connectionsControl->setToolTip(tooltip);

    connectionsControl->setPixmap(platformStyle->SingleColorIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
}

void BurritoCoinGUI::setNumConnections(int count)
{
    updateNetworkState();
}

void BurritoCoinGUI::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void BurritoCoinGUI::updateHeadersSyncProgressLabel()
{
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus().nPowTargetSpacing;
    if (estHeadersLeft > HEADER_HEIGHT_DELTA_SYNC)
        progressBarLabel->setText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 / (headersTipHeight+estHeadersLeft)*headersTipHeight, 'f', 1)));
}

void BurritoCoinGUI::openOptionsDialogWithTab(OptionsDialog::Tab tab)
{
    if (!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setCurrentTab(tab);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BurritoCoinGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header, SynchronizationState sync_state)
{
// Disabling macOS App Nap on initial sync, disk and reindex operations.
#ifdef Q_OS_MAC
    if (sync_state == SynchronizationState::POST_INIT) {
        m_app_nap_inhibitor->enableAppNap();
    } else {
        m_app_nap_inhibitor->disableAppNap();
    }
#endif

    if (modalOverlay)
    {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BlockSource::NETWORK:
            if (header) {
                updateHeadersSyncProgressLabel();
                return;
            }
            progressBarLabel->setText(tr("Synchronizing with network..."));
            updateHeadersSyncProgressLabel();
            break;
        case BlockSource::DISK:
            if (header) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
            } else {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
            }
            break;
        case BlockSource::REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BlockSource::NONE:
            if (header) {
                return;
            }
            progressBarLabel->setText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    if (secs < MAX_BLOCK_TIME_GAP) {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);
        }
#endif // ENABLE_WALLET

        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
                ":/animation/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
            modalOverlay->showHide();
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BurritoCoinGUI::message(const QString& title, QString message, unsigned int style, bool* ret, const QString& detailed_message)
{
#ifdef ENABLE_WALLET
    // When a wallet backup succeeds (from any path — the menu, a reminder, or
    // the status-bar badge), record it so the backup badge flips to "Backed up".
    // The title originates in WalletView, so match that translation context.
    if (walletFrame && title == QCoreApplication::translate("WalletView", "Backup Successful")) {
        if (WalletModel* wm = walletFrame->currentWalletModel()) {
            QSettings settings;
            settings.setValue(QStringLiteral("wallet/") + wm->getWalletName() + QStringLiteral("/backup_done"), true);
            setBackupStatus(wm);
        }
    }
#endif // ENABLE_WALLET
    // Default title. On macOS, the window title is ignored (as required by the macOS Guidelines).
    QString strTitle{PACKAGE_NAME};
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            message = tr("Error: %1").arg(message);
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            message = tr("Warning: %1").arg(message);
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            // No need to prepend the prefix here.
            break;
        default:
            break;
        }
    }

    if (!msgType.isEmpty()) {
        strTitle += " - " + msgType;
    }

    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox(static_cast<QMessageBox::Icon>(nMBoxIcon), strTitle, message, buttons, this);
        mBox.setTextFormat(Qt::PlainText);
        mBox.setDetailedText(detailed_message);
        int r = mBox.exec();
        if (ret != nullptr)
            *ret = r == QMessageBox::Ok;
    } else {
        notificator->notify(static_cast<Notificator::Class>(nNotifyIcon), strTitle, message);
    }
}

void BurritoCoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, &BurritoCoinGUI::hide);
                e->ignore();
            }
            else if((wsevt->oldState() & Qt::WindowMinimized) && !isMinimized())
            {
                QTimer::singleShot(0, this, &BurritoCoinGUI::show);
                e->ignore();
            }
        }
    }
#endif
}

void BurritoCoinGUI::closeEvent(QCloseEvent *event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}

void BurritoCoinGUI::showEvent(QShowEvent *event)
{
    // enable the debug window when the main window shows up
    openRPCConsoleAction->setEnabled(true);
    aboutAction->setEnabled(true);
    optionsAction->setEnabled(true);
}

#ifdef ENABLE_WALLET
void BurritoCoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(BurritoCoinUnits::formatWithUnit(unit, amount, true));
    if (m_node.walletClient().getWallets().size() > 1 && !walletName.isEmpty()) {
        msg += tr("Wallet: %1\n").arg(walletName);
    }
    msg += tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void BurritoCoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BurritoCoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        for (const QUrl &uri : event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool BurritoCoinGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool BurritoCoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void BurritoCoinGUI::setHDStatus(bool privkeyDisabled, int hdEnabled)
{
    labelWalletHDStatusIcon->setPixmap(platformStyle->SingleColorIcon(privkeyDisabled ? ":/icons/eye" : hdEnabled ? ":/icons/hd_enabled" : ":/icons/hd_disabled").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelWalletHDStatusIcon->setToolTip(privkeyDisabled ? tr("Private key <b>disabled</b>") : hdEnabled ? tr("HD key generation is <b>enabled</b>") : tr("HD key generation is <b>disabled</b>"));
    labelWalletHDStatusIcon->show();
    // eventually disable the QLabel to set its opacity to 50%
    labelWalletHDStatusIcon->setEnabled(hdEnabled);
}

void BurritoCoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false);
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false);
        break;
    }
}

void BurritoCoinGUI::updateWalletStatus()
{
    if (!walletFrame) {
        return;
    }
    WalletView * const walletView = walletFrame->currentWalletView();
    if (!walletView) {
        return;
    }
    WalletModel * const walletModel = walletView->getWalletModel();
    setEncryptionStatus(walletModel->getEncryptionStatus());
    setHDStatus(walletModel->wallet().privateKeysDisabled(), walletModel->wallet().hdEnabled());
    setBackupStatus(walletModel);
}

void BurritoCoinGUI::setBackupStatus(WalletModel* wallet_model)
{
    if (!m_backup_status_label) return;

    // Watch-only / private-keys-disabled wallets hold no spendable keys to lose,
    // so a backup badge would only confuse — hide it for those.
    if (!wallet_model || wallet_model->wallet().privateKeysDisabled()) {
        m_backup_status_label->hide();
        return;
    }

    QSettings settings;
    const QString key = QStringLiteral("wallet/") + wallet_model->getWalletName() + QStringLiteral("/backup_done");
    const bool backed_up = settings.value(key, false).toBool();

    if (backed_up) {
        m_backup_status_label->setText(tr("\xe2\x9c\x94 Backed up"));
        m_backup_status_label->setStyleSheet("color:#1e8e3e; padding:0 6px;");
        m_backup_status_label->setToolTip(tr("This wallet has been backed up. Keep your backup somewhere safe."));
        m_backup_status_label->setCursor(Qt::ArrowCursor);
    } else {
        m_backup_status_label->setText(tr("\xe2\x9a\xa0 Back up wallet"));
        m_backup_status_label->setStyleSheet("color:#c0392b; font-weight:bold; padding:0 6px;");
        m_backup_status_label->setToolTip(tr("Your wallet is not backed up yet. Click here to save a backup of wallet.dat."));
        m_backup_status_label->setCursor(Qt::PointingHandCursor);
    }
    m_backup_status_label->show();
}
#endif // ENABLE_WALLET

void BurritoCoinGUI::updateProxyIcon()
{
    std::string ip_port;
    bool proxy_enabled = clientModel->getProxyInfo(ip_port);

    if (proxy_enabled) {
        if (labelProxyIcon->pixmap() == nullptr) {
            QString ip_port_q = QString::fromStdString(ip_port);
            labelProxyIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/proxy").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            labelProxyIcon->setToolTip(tr("Proxy is <b>enabled</b>: %1").arg(ip_port_q));
        } else {
            labelProxyIcon->show();
        }
    } else {
        labelProxyIcon->hide();
    }
}

void BurritoCoinGUI::updateWindowTitle()
{
    QString window_title = PACKAGE_NAME;
#ifdef ENABLE_WALLET
    if (walletFrame) {
        WalletModel* const wallet_model = walletFrame->currentWalletModel();
        if (wallet_model && !wallet_model->getWalletName().isEmpty()) {
            window_title += " - " + wallet_model->getDisplayName();
        }
    }
#endif
    if (!m_network_style->getTitleAddText().isEmpty()) {
        window_title += " - " + m_network_style->getTitleAddText();
    }
    setWindowTitle(window_title);
}

void BurritoCoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    if (!isHidden() && !isMinimized() && !GUIUtil::isObscured(this) && fToggleHidden) {
        hide();
    } else {
        GUIUtil::bringToFront(this);
    }
}

void BurritoCoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BurritoCoinGUI::detectShutdown()
{
    if (m_node.shutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void BurritoCoinGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, QString(), 0, 100);
        GUIUtil::PolishProgressDialog(progressDialog);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
            progressDialog = nullptr;
        }
    } else if (progressDialog) {
        progressDialog->setValue(nProgress);
    }
}

void BurritoCoinGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void BurritoCoinGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}

static bool ThreadSafeMessageBox(BurritoCoinGUI* gui, const bilingual_str& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;

    QString detailed_message; // This is original message, in English, for googling and referencing.
    if (message.original != message.translated) {
        detailed_message = BurritoCoinGUI::tr("Original message:") + "\n" + QString::fromStdString(message.original);
    }

    // In case of modal message, use blocking connection to wait for user to click a button
    bool invoked = QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message.translated)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret),
                               Q_ARG(QString, detailed_message));
    assert(invoked);
    return ret;
}

void BurritoCoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_message_box = m_node.handleMessageBox(std::bind(ThreadSafeMessageBox, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    m_handler_question = m_node.handleQuestion(std::bind(ThreadSafeMessageBox, this, std::placeholders::_1, std::placeholders::_3, std::placeholders::_4));
}

void BurritoCoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_message_box->disconnect();
    m_handler_question->disconnect();
}

bool BurritoCoinGUI::isPrivacyModeActivated() const
{
    assert(m_mask_values_action);
    return m_mask_values_action->isChecked();
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl(const PlatformStyle *platformStyle) :
    optionsModel(nullptr),
    menu(nullptr)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
    QList<BurritoCoinUnits::Unit> units = BurritoCoinUnits::availableUnits();
    int max_width = 0;
    const QFontMetrics fm(font());
    for (const BurritoCoinUnits::Unit unit : units)
    {
        max_width = qMax(max_width, GUIUtil::TextWidth(fm, BurritoCoinUnits::longName(unit)));
    }
    setMinimumSize(max_width, 0);
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setStyleSheet(QString("QLabel { color : %1 }").arg(platformStyle->SingleColor().name()));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent *event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu(this);
    for (const BurritoCoinUnits::Unit u : BurritoCoinUnits::availableUnits())
    {
        QAction *menuAction = new QAction(QString(BurritoCoinUnits::longName(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu, &QMenu::triggered, this, &UnitDisplayStatusBarControl::onMenuSelection);
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel *_optionsModel)
{
    if (_optionsModel)
    {
        this->optionsModel = _optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(_optionsModel, &OptionsModel::displayUnitChanged, this, &UnitDisplayStatusBarControl::updateDisplayUnit);

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(_optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setText(BurritoCoinUnits::longName(newUnits));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action)
    {
        optionsModel->setDisplayUnit(action->data());
    }
}
