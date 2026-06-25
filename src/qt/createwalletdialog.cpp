// Copyright (c) 2019-2026 The Bitcoin Core developers
// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/burritocoin-config.h>
#endif

#include <qt/createwalletdialog.h>
#include <qt/forms/ui_createwalletdialog.h>

#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

namespace {
void showInfoPopup(QWidget* parent, const QString& title, const QString& body)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setTextFormat(Qt::RichText);
    box.setText(body);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}
} // namespace

CreateWalletDialog::CreateWalletDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::CreateWalletDialog)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui->wallet_name_line_edit->setFocus(Qt::ActiveWindowFocusReason);

    connect(ui->wallet_name_line_edit, &QLineEdit::textEdited, [this](const QString& text) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
    });

    connect(ui->encrypt_wallet_checkbox, &QCheckBox::toggled, [this](bool checked) {
        // Disable the disable_privkeys_checkbox when isEncryptWalletChecked is
        // set to true, enable it when isEncryptWalletChecked is false.
        ui->disable_privkeys_checkbox->setEnabled(!checked);

        // When the disable_privkeys_checkbox is disabled, uncheck it.
        if (!ui->disable_privkeys_checkbox->isEnabled()) {
            ui->disable_privkeys_checkbox->setChecked(false);
        }
    });

    connect(ui->disable_privkeys_checkbox, &QCheckBox::toggled, [this](bool checked) {
        // Disable the encrypt_wallet_checkbox when isDisablePrivateKeysChecked is
        // set to true, enable it when isDisablePrivateKeysChecked is false.
        ui->encrypt_wallet_checkbox->setEnabled(!checked);

        // Wallets without private keys start out blank
        if (checked) {
            ui->blank_wallet_checkbox->setChecked(true);
        }

        // When the encrypt_wallet_checkbox is disabled, uncheck it.
        if (!ui->encrypt_wallet_checkbox->isEnabled()) {
            ui->encrypt_wallet_checkbox->setChecked(false);
        }
    });

    ui->descriptor_checkbox->setToolTip(tr("Coming Soon"));
    ui->descriptor_checkbox->setEnabled(false);
    ui->descriptor_checkbox->setChecked(false);

    // Info popups: each "?" button explains the option in plain language
    // with a real-world example, so people can choose without guessing.
    connect(ui->encrypt_wallet_info, &QToolButton::clicked, this, [this]{
        showInfoPopup(this, tr("Encrypt Wallet"),
            tr("<p><b>What it does:</b> Locks your wallet file with a passphrase you choose. "
               "If someone steals a copy of the file, they can't spend from it without the passphrase.</p>"
               "<p><b>How it feels:</b> Opening the wallet and receiving coins still works normally. "
               "Only <b>sending</b> coins asks for your passphrase.</p>"
               "<p><b>Real-world example:</b> You back up <code>wallet.dat</code> to OneDrive for safekeeping. "
               "Later, that account is compromised. With encryption ON, the attacker has an unreadable blob. "
               "With encryption OFF, they have your money.</p>"
               "<p><b>Trade-off:</b> If you forget the passphrase, your coins are gone forever. "
               "There is no reset, no support line. Write the passphrase on paper, store it somewhere safe.</p>"
               "<p><b>Recommended for any wallet that will hold real value.</b></p>"));
    });

    connect(ui->disable_privkeys_info, &QToolButton::clicked, this, [this]{
        showInfoPopup(this, tr("Disable Private Keys"),
            tr("<p><b>What it does:</b> Creates a wallet that can only <i>watch</i> addresses; "
               "it cannot sign transactions to spend from them.</p>"
               "<p><b>How it feels:</b> The wallet shows balances and incoming transactions for any "
               "addresses you import, but the Send tab is disabled.</p>"
               "<p><b>Real-world example:</b> Your real coins live on a hardware wallet kept in a safe. "
               "You import the hardware wallet's <i>public</i> addresses here so you can track the balance "
               "on your desktop, without exposing the private keys.</p>"
               "<p><b>Leave this OFF</b> unless you specifically want a watch-only wallet.</p>"));
    });

    connect(ui->blank_wallet_info, &QToolButton::clicked, this, [this]{
        showInfoPopup(this, tr("Make Blank Wallet"),
            tr("<p><b>What it does:</b> Creates an empty wallet with no addresses and no keys. "
               "You add your own keys later (by importing a private key, a WIF, or an HD seed).</p>"
               "<p><b>How it feels:</b> Right after creation there's nothing in it. You then use "
               "<b>importprivkey</b> or restore an HD seed to populate it.</p>"
               "<p><b>Real-world example:</b> You're recovering an old wallet from a 24-word seed phrase. "
               "You create a blank wallet, then restore the seed into it so all your historical addresses re-derive.</p>"
               "<p><b>Leave this OFF</b> if you just want a fresh wallet that generates its own keys.</p>"));
    });

    connect(ui->descriptor_info, &QToolButton::clicked, this, [this]{
        showInfoPopup(this, tr("Descriptor Wallet"),
            tr("<p><b>What it does:</b> Uses the newer <i>output descriptor</i> format to track which "
               "addresses belong to the wallet, instead of the older keypool approach.</p>"
               "<p><b>How it feels:</b> Identical to a regular wallet day-to-day — same Send/Receive — "
               "but backups can describe ranges of addresses more flexibly.</p>"
               "<p><b>Real-world example:</b> Power users sharing an xpub with an accountant so they can "
               "watch balances without seeing private keys.</p>"
               "<p><b>Currently disabled</b> in BurritoCoin Core. Leave this OFF.</p>"));
    });
}

CreateWalletDialog::~CreateWalletDialog()
{
    delete ui;
}

QString CreateWalletDialog::walletName() const
{
    return ui->wallet_name_line_edit->text();
}

bool CreateWalletDialog::isEncryptWalletChecked() const
{
    return ui->encrypt_wallet_checkbox->isChecked();
}

bool CreateWalletDialog::isDisablePrivateKeysChecked() const
{
    return ui->disable_privkeys_checkbox->isChecked();
}

bool CreateWalletDialog::isMakeBlankWalletChecked() const
{
    return ui->blank_wallet_checkbox->isChecked();
}

bool CreateWalletDialog::isDescriptorWalletChecked() const
{
    return ui->descriptor_checkbox->isChecked();
}
