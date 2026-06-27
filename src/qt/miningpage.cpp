// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningpage.h>

#include <qt/clientmodel.h>
#include <qt/miningmodel.h>
#include <qt/walletmodel.h>

#include <chainparams.h>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

namespace {
//! Human-friendly hashrate, e.g. "0 H/s", "812 H/s", "3.4 kH/s", "1.2 MH/s".
QString FormatHashrate(double hps)
{
    if (hps < 1000.0) {
        return QObject::tr("%1 H/s").arg(QLocale().toString(qRound(hps)));
    }
    if (hps < 1000000.0) {
        return QObject::tr("%1 kH/s").arg(QLocale().toString(hps / 1000.0, 'f', 2));
    }
    return QObject::tr("%1 MH/s").arg(QLocale().toString(hps / 1000000.0, 'f', 2));
}

//! Human-friendly duration, e.g. "about 9 minutes".
QString FormatDuration(double seconds)
{
    if (seconds < 90.0) return QObject::tr("about %n second(s)", "", qRound(seconds));
    const double minutes = seconds / 60.0;
    if (minutes < 90.0) return QObject::tr("about %n minute(s)", "", qRound(minutes));
    const double hours = minutes / 60.0;
    if (hours < 48.0) return QObject::tr("about %n hour(s)", "", qRound(hours));
    return QObject::tr("about %n day(s)", "", qRound(hours / 24.0));
}
} // namespace

MiningPage::MiningPage(const PlatformStyle* platform_style, QWidget* parent)
    : QWidget(parent), m_platform_style(platform_style)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(12);

    QLabel* title = new QLabel(tr("\xe2\x9b\x8f\xef\xb8\x8f  Mine BurritoCoin"), this);
    title->setStyleSheet("font-size:18px; font-weight:bold; color:#f5a623;");
    layout->addWidget(title);

    QLabel* intro = new QLabel(tr(
        "Mining uses your computer's processor to help secure the BurritoCoin network. "
        "When your computer finds a block, the reward is paid <b>directly into this wallet</b>. "
        "Start with one core \xe2\x80\x94 you can add more below. Leaving a core or two free keeps "
        "the rest of your computer responsive."), this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    layout->addWidget(intro);

    // --- Core selector -------------------------------------------------
    const int max_threads = MiningModel::maxThreads();

    QHBoxLayout* cores_row = new QHBoxLayout;
    QLabel* cores_caption = new QLabel(tr("Processor cores:"), this);
    m_thread_slider = new QSlider(Qt::Horizontal, this);
    m_thread_slider->setMinimum(1);
    m_thread_slider->setMaximum(max_threads);
    m_thread_slider->setValue(1); // conservative default: a single core
    m_thread_slider->setPageStep(1);
    m_thread_slider->setTickPosition(QSlider::TicksBelow);
    m_thread_label = new QLabel(this);
    m_thread_label->setMinimumWidth(110);
    cores_row->addWidget(cores_caption);
    cores_row->addWidget(m_thread_slider, 1);
    cores_row->addWidget(m_thread_label);
    layout->addLayout(cores_row);

    m_all_cores = new QCheckBox(tr("Use all cores"), this);
    layout->addWidget(m_all_cores);

    if (max_threads <= 1) {
        // Single-core machine: nothing to choose.
        m_thread_slider->setEnabled(false);
        m_all_cores->setEnabled(false);
    }

    // --- Start / Stop --------------------------------------------------
    m_start_stop = new QPushButton(tr("Start Mining"), this);
    m_start_stop->setMinimumHeight(36);
    m_start_stop->setStyleSheet("font-weight:bold;");
    layout->addWidget(m_start_stop);

    // --- Status readout ------------------------------------------------
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    QHBoxLayout* status_row = new QHBoxLayout;
    QLabel* status_caption = new QLabel(tr("Status:"), this);
    m_status_value = new QLabel(tr("Idle"), this);
    m_status_value->setStyleSheet("font-weight:bold;");
    status_row->addWidget(status_caption);
    status_row->addWidget(m_status_value);
    status_row->addStretch(1);
    layout->addLayout(status_row);

    QHBoxLayout* speed_row = new QHBoxLayout;
    QLabel* speed_caption = new QLabel(tr("Speed:"), this);
    m_hashrate_value = new QLabel(QString::fromUtf8("\xe2\x80\x94"), this);
    speed_row->addWidget(speed_caption);
    speed_row->addWidget(m_hashrate_value);
    speed_row->addStretch(1);
    layout->addLayout(speed_row);

    QHBoxLayout* found_row = new QHBoxLayout;
    QLabel* found_caption = new QLabel(tr("Blocks found this session:"), this);
    m_blocks_value = new QLabel(QStringLiteral("0"), this);
    found_row->addWidget(found_caption);
    found_row->addWidget(m_blocks_value);
    found_row->addStretch(1);
    layout->addLayout(found_row);

    QHBoxLayout* eta_row = new QHBoxLayout;
    QLabel* eta_caption = new QLabel(tr("Estimated time to a block:"), this);
    m_eta_value = new QLabel(QString::fromUtf8("\xe2\x80\x94"), this);
    eta_row->addWidget(eta_caption);
    eta_row->addWidget(m_eta_value);
    eta_row->addStretch(1);
    layout->addLayout(eta_row);

    QLabel* maturity_note = new QLabel(tr(
        "Note: newly mined coins need 100 confirmations (about 4 hours) before they "
        "can be spent. They will appear as \xe2\x80\x9cimmature\xe2\x80\x9d until then."), this);
    maturity_note->setWordWrap(true);
    maturity_note->setStyleSheet("color:#888;");
    layout->addWidget(maturity_note);

    layout->addStretch(1);

    connect(m_start_stop, &QPushButton::clicked, this, &MiningPage::onStartStopClicked);
    connect(m_all_cores, &QCheckBox::toggled, this, &MiningPage::onUseAllCoresToggled);
    connect(m_thread_slider, &QSlider::valueChanged, this, &MiningPage::onThreadSliderChanged);

    updateThreadLabel();
    updateControlsEnabled();
}

MiningPage::~MiningPage()
{
    // m_miner is parented to this and will stop() in its destructor.
}

void MiningPage::setClientModel(ClientModel* client_model)
{
    // On app shutdown the client model is detached (set to nullptr) on the GUI
    // thread BEFORE the node — and thus the mempool/chainstate the workers
    // touch — is torn down. Stop and join the workers here so they can never run
    // against freed node objects. stop() is a no-op if not mining.
    if (!client_model && m_miner) m_miner->stop();
    m_client_model = client_model;
    ensureMiner();
    updateControlsEnabled();
}

void MiningPage::setWalletModel(WalletModel* wallet_model)
{
    m_wallet_model = wallet_model;
    if (m_miner) m_miner->setWalletModel(wallet_model);
    updateControlsEnabled();
}

void MiningPage::ensureMiner()
{
    if (m_miner || !m_client_model) return;
    m_miner = new MiningModel(m_client_model->node(), this);
    m_miner->setWalletModel(m_wallet_model);
    connect(m_miner, &MiningModel::stateChanged, this, &MiningPage::onMiningStateChanged);
    connect(m_miner, &MiningModel::hashrateChanged, this, &MiningPage::onHashrate);
    connect(m_miner, &MiningModel::blockFound, this, &MiningPage::onBlockFound);
    connect(m_miner, &MiningModel::error, this, &MiningPage::onMiningError);
}

int MiningPage::selectedThreadCount() const
{
    if (m_all_cores && m_all_cores->isChecked()) return MiningModel::maxThreads();
    return m_thread_slider ? m_thread_slider->value() : 1;
}

void MiningPage::updateThreadLabel()
{
    const int max_threads = MiningModel::maxThreads();
    const int sel = selectedThreadCount();
    m_thread_label->setText(tr("%1 of %2 cores").arg(sel).arg(max_threads));
}

void MiningPage::updateControlsEnabled()
{
    const bool mining = m_miner && m_miner->isMining();
    const bool can_mine = (m_miner != nullptr) && (m_wallet_model != nullptr);
    const int max_threads = MiningModel::maxThreads();

    m_start_stop->setEnabled(can_mine || mining);
    // Core selection is locked while mining; stop first to change it.
    m_thread_slider->setEnabled(!mining && max_threads > 1);
    m_all_cores->setEnabled(!mining && max_threads > 1);
}

void MiningPage::onStartStopClicked()
{
    if (!m_miner) return;
    if (m_miner->isMining()) {
        m_miner->stop();
    } else {
        // One-time disclosure before the very first mining start.
        QSettings settings;
        if (!settings.value(QStringLiteral("mining/disclosure_shown"), false).toBool()) {
            QMessageBox::information(this, tr("About mining"), tr(
                "Mining runs your processor hard \xe2\x80\x94 it uses electricity and produces heat. "
                "A few things to know:\n\n"
                "\xe2\x80\xa2 Your antivirus may flag the wallet as a \xe2\x80\x9ccoin miner\xe2\x80\x9d. "
                "That is expected for mining software; if it quarantines the wallet, restore it from "
                "your antivirus's quarantine list.\n\n"
                "\xe2\x80\xa2 Newly mined coins are \xe2\x80\x9cimmature\xe2\x80\x9d for 100 confirmations "
                "(about 4 hours) before you can spend them.\n\n"
                "\xe2\x80\xa2 You can have mining pause automatically on battery or while you're using "
                "the computer, in Settings \xe2\x80\xba Mining.\n\n"
                "This won't be shown again."));
            settings.setValue(QStringLiteral("mining/disclosure_shown"), true);
        }
        m_miner->start(selectedThreadCount());
    }
}

void MiningPage::onUseAllCoresToggled(bool checked)
{
    if (checked && m_thread_slider) {
        const QSignalBlocker blocker(m_thread_slider);
        m_thread_slider->setValue(MiningModel::maxThreads());
    }
    updateThreadLabel();
}

void MiningPage::onThreadSliderChanged(int value)
{
    if (m_all_cores && value < MiningModel::maxThreads() && m_all_cores->isChecked()) {
        const QSignalBlocker blocker(m_all_cores);
        m_all_cores->setChecked(false);
    }
    updateThreadLabel();
}

void MiningPage::onMiningStateChanged(bool mining)
{
    m_start_stop->setText(mining ? tr("Stop Mining") : tr("Start Mining"));
    m_status_value->setText(mining ? tr("Mining") : tr("Idle"));
    m_status_value->setStyleSheet(mining ? "font-weight:bold; color:#1e8e3e;"
                                         : "font-weight:bold;");
    if (!mining) m_hashrate_value->setText(QString::fromUtf8("\xe2\x80\x94"));
    updateControlsEnabled();
}

void MiningPage::onHashrate(double hashes_per_sec)
{
    m_hashrate_value->setText(FormatHashrate(hashes_per_sec));

    // Refresh the (cs_main-locking) network hashrate only every ~10 ticks, then
    // show the expected solo time to a block: netHashPS * blockSpacing / yourHashPS.
    if (m_client_model && (m_net_hps_tick++ % 10 == 0)) {
        m_cached_net_hps = m_client_model->getNetworkHashPS();
    }
    if (hashes_per_sec <= 0.0 || m_cached_net_hps <= 0.0) {
        m_eta_value->setText(QString::fromUtf8("\xe2\x80\x94"));
        return;
    }
    const double spacing = static_cast<double>(Params().GetConsensus().nPowTargetSpacing);
    m_eta_value->setText(FormatDuration(m_cached_net_hps * spacing / hashes_per_sec));
}

void MiningPage::onBlockFound(const QString& block_hash, int height)
{
    ++m_blocks_session;
    m_blocks_value->setText(tr("%1  (latest at height %2)")
                                .arg(m_blocks_session)
                                .arg(height));
    Q_UNUSED(block_hash);
}

void MiningPage::onMiningError(const QString& message)
{
    m_status_value->setText(tr("Idle"));
    m_status_value->setStyleSheet("font-weight:bold;");
    // A modal would be heavy for transient cases; surface inline in the speed slot.
    m_hashrate_value->setText(QStringLiteral("<span style='color:#c0392b;'>%1</span>")
                                  .arg(message.toHtmlEscaped()));
}
