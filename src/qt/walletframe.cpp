// Copyright (c) 2011-2026 The Bitcoin Core developers
// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletframe.h>

#include <qt/burritocoingui.h>
#include <qt/createwalletdialog.h>
#include <qt/overviewpage.h>
#include <qt/walletcontroller.h>
#include <qt/walletmodel.h>
#include <qt/walletview.h>

#include <cassert>

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

WalletFrame::WalletFrame(const PlatformStyle* _platformStyle, BurritoCoinGUI* _gui)
    : QFrame(_gui),
      gui(_gui),
      platformStyle(_platformStyle),
      m_size_hint(OverviewPage{platformStyle, nullptr}.sizeHint())
{
    // Leave HBox hook for adding a list view later
    QHBoxLayout *walletFrameLayout = new QHBoxLayout(this);
    setContentsMargins(0,0,0,0);
    walletStack = new QStackedWidget(this);
    walletFrameLayout->setContentsMargins(0,0,0,0);
    walletFrameLayout->addWidget(walletStack);

    // hbox for no wallet
    QGroupBox* no_wallet_group = new QGroupBox(walletStack);
    no_wallet_group->setStyleSheet("QGroupBox { border: none; background: #1a0f05; }");

    QVBoxLayout* no_wallet_layout = new QVBoxLayout(no_wallet_group);
    no_wallet_layout->setContentsMargins(40, 40, 40, 40);
    no_wallet_layout->setSpacing(0);
    no_wallet_layout->addStretch(2);

    // Big title
    QLabel* noWalletTitle = new QLabel(tr("\xf0\x9f\x8c\xaf  BurritoCoin"));
    noWalletTitle->setAlignment(Qt::AlignCenter);
    {
        QFont f = noWalletTitle->font();
        f.setPointSize(qMax(28, f.pointSize() * 3));
        f.setBold(true);
        noWalletTitle->setFont(f);
        noWalletTitle->setStyleSheet("color: #f5a623;");
    }
    no_wallet_layout->addWidget(noWalletTitle);

    QLabel* noWalletTag = new QLabel(tr("No wallet is loaded."));
    noWalletTag->setAlignment(Qt::AlignCenter);
    {
        QFont f = noWalletTag->font();
        f.setPointSize(qMax(14, (f.pointSize() * 3) / 2));
        noWalletTag->setFont(f);
        noWalletTag->setStyleSheet("color: #c47d0e; margin-top: 6px;");
    }
    no_wallet_layout->addWidget(noWalletTag);

    no_wallet_layout->addSpacing(28);

    QLabel* noWalletHint = new QLabel(tr("Create a new wallet to start receiving BurritoCoin, "
                                          "or open an existing one with <b>File &#8250; Open Wallet</b>."));
    noWalletHint->setAlignment(Qt::AlignCenter);
    noWalletHint->setWordWrap(true);
    noWalletHint->setTextFormat(Qt::RichText);
    noWalletHint->setMaximumWidth(560);
    {
        QFont f = noWalletHint->font();
        f.setPointSize(qMax(11, (f.pointSize() * 11) / 10));
        noWalletHint->setFont(f);
        noWalletHint->setStyleSheet("color: #d6c4a3;");
    }
    no_wallet_layout->addWidget(noWalletHint, 0, Qt::AlignHCenter);

    no_wallet_layout->addSpacing(36);

    // A button for create wallet dialog
    QPushButton* create_wallet_button = new QPushButton(tr("Create a new wallet"), walletStack);
    create_wallet_button->setCursor(Qt::PointingHandCursor);
    create_wallet_button->setMinimumSize(260, 52);
    {
        QFont f = create_wallet_button->font();
        f.setPointSize(qMax(13, (f.pointSize() * 13) / 10));
        f.setBold(true);
        create_wallet_button->setFont(f);
    }
    create_wallet_button->setStyleSheet(
        "QPushButton { background-color: #f5a623; color: #3b1f0a;"
        " border: none; border-radius: 8px; padding: 10px 28px; }"
        "QPushButton:hover { background-color: #ffc04d; }"
        "QPushButton:pressed { background-color: #c47d0e; }");
    connect(create_wallet_button, &QPushButton::clicked, [this] {
        auto activity = new CreateWalletActivity(gui->getWalletController(), this);
        connect(activity, &CreateWalletActivity::finished, activity, &QObject::deleteLater);
        activity->create();
    });
    no_wallet_layout->addWidget(create_wallet_button, 0, Qt::AlignHCenter);

    no_wallet_layout->addStretch(3);
    no_wallet_group->setLayout(no_wallet_layout);

    walletStack->addWidget(no_wallet_group);
}

WalletFrame::~WalletFrame()
{
}

void WalletFrame::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    for (auto i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i) {
        i.value()->setClientModel(_clientModel);
    }
}

bool WalletFrame::addWallet(WalletModel *walletModel)
{
    if (!gui || !clientModel || !walletModel) return false;

    if (mapWalletViews.count(walletModel) > 0) return false;

    WalletView *walletView = new WalletView(platformStyle, this);
    walletView->setClientModel(clientModel);
    walletView->setWalletModel(walletModel);
    walletView->showOutOfSyncWarning(bOutOfSync);
    walletView->setPrivacy(gui->isPrivacyModeActivated());

    WalletView* current_wallet_view = currentWalletView();
    if (current_wallet_view) {
        walletView->setCurrentIndex(current_wallet_view->currentIndex());
    } else {
        walletView->gotoOverviewPage();
    }

    walletStack->addWidget(walletView);
    mapWalletViews[walletModel] = walletView;

    connect(walletView, &WalletView::outOfSyncWarningClicked, this, &WalletFrame::outOfSyncWarningClicked);
    connect(walletView, &WalletView::transactionClicked, gui, &BurritoCoinGUI::gotoHistoryPage);
    connect(walletView, &WalletView::coinsSent, gui, &BurritoCoinGUI::gotoHistoryPage);
    connect(walletView, &WalletView::message, [this](const QString& title, const QString& message, unsigned int style) {
        gui->message(title, message, style);
    });
    connect(walletView, &WalletView::encryptionStatusChanged, gui, &BurritoCoinGUI::updateWalletStatus);
    connect(walletView, &WalletView::incomingTransaction, gui, &BurritoCoinGUI::incomingTransaction);
    connect(walletView, &WalletView::hdEnabledStatusChanged, gui, &BurritoCoinGUI::updateWalletStatus);
    connect(gui, &BurritoCoinGUI::setPrivacy, walletView, &WalletView::setPrivacy);

    return true;
}

void WalletFrame::setCurrentWallet(WalletModel* wallet_model)
{
    if (mapWalletViews.count(wallet_model) == 0) return;

    // Stop the effect of hidden widgets on the size hint of the shown one in QStackedWidget.
    WalletView* view_about_to_hide = currentWalletView();
    if (view_about_to_hide) {
        QSizePolicy sp = view_about_to_hide->sizePolicy();
        sp.setHorizontalPolicy(QSizePolicy::Ignored);
        view_about_to_hide->setSizePolicy(sp);
    }

    WalletView *walletView = mapWalletViews.value(wallet_model);
    assert(walletView);

    // Set or restore the default QSizePolicy which could be set to QSizePolicy::Ignored previously.
    QSizePolicy sp = walletView->sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Preferred);
    walletView->setSizePolicy(sp);
    walletView->updateGeometry();

    walletStack->setCurrentWidget(walletView);
    walletView->updateEncryptionStatus();
}

void WalletFrame::removeWallet(WalletModel* wallet_model)
{
    if (mapWalletViews.count(wallet_model) == 0) return;

    WalletView *walletView = mapWalletViews.take(wallet_model);
    walletStack->removeWidget(walletView);
    delete walletView;
}

void WalletFrame::removeAllWallets()
{
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        walletStack->removeWidget(i.value());
    mapWalletViews.clear();
}

bool WalletFrame::handlePaymentRequest(const SendCoinsRecipient &recipient)
{
    WalletView *walletView = currentWalletView();
    if (!walletView)
        return false;

    return walletView->handlePaymentRequest(recipient);
}

void WalletFrame::showOutOfSyncWarning(bool fShow)
{
    bOutOfSync = fShow;
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->showOutOfSyncWarning(fShow);
}

void WalletFrame::gotoOverviewPage()
{
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoOverviewPage();
}

void WalletFrame::gotoHistoryPage()
{
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoHistoryPage();
}

void WalletFrame::gotoReceiveCoinsPage()
{
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoReceiveCoinsPage();
}

void WalletFrame::gotoSendCoinsPage(QString addr)
{
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoSendCoinsPage(addr);
}

void WalletFrame::gotoMiningPage()
{
    QMap<WalletModel*, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoMiningPage();
}

void WalletFrame::gotoSignMessageTab(QString addr)
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->gotoSignMessageTab(addr);
}

void WalletFrame::gotoVerifyMessageTab(QString addr)
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->gotoVerifyMessageTab(addr);
}

void WalletFrame::gotoLoadPSBT(bool from_clipboard)
{
    WalletView *walletView = currentWalletView();
    if (walletView) {
        walletView->gotoLoadPSBT(from_clipboard);
    }
}

void WalletFrame::encryptWallet(bool status)
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->encryptWallet(status);
}

void WalletFrame::backupWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->backupWallet();
}

void WalletFrame::changePassphrase()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->changePassphrase();
}

void WalletFrame::unlockWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->unlockWallet();
}

void WalletFrame::usedSendingAddresses()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->usedSendingAddresses();
}

void WalletFrame::usedReceivingAddresses()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->usedReceivingAddresses();
}

WalletView* WalletFrame::currentWalletView() const
{
    return qobject_cast<WalletView*>(walletStack->currentWidget());
}

WalletModel* WalletFrame::currentWalletModel() const
{
    WalletView* wallet_view = currentWalletView();
    return wallet_view ? wallet_view->getWalletModel() : nullptr;
}

void WalletFrame::outOfSyncWarningClicked()
{
    Q_EMIT requestedSyncWarningInfo();
}
