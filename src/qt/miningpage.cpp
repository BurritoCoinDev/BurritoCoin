// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningpage.h>

#include <qt/brand.h>
#include <qt/clientmodel.h>
#include <qt/externalminer.h>
#include <qt/miningmodel.h>
#include <qt/miningutil.h>
#include <qt/stratumbridge.h>
#include <qt/walletmodel.h>

#include <chainparams.h>
#include <script/script.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
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
    const double days = hours / 24.0;
    // Guard the int conversion: a near-zero local hashrate can make the estimate
    // astronomically large, and qRound(double)->int / tr's %n are int-domain.
    if (days > 365000.0) return QObject::tr("more than a thousand years");
    return QObject::tr("about %n day(s)", "", qRound(days));
}

//! Rounded card frame shared by the hero / stats / settings sections.
QFrame* MakeCard(QWidget* parent)
{
    QFrame* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("miningCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#miningCard { background-color:%1; border:1px solid %2; border-radius:12px; }")
        .arg(QLatin1String(brand::CardBg), QLatin1String(brand::Border)));
    return card;
}

//! Small uppercase caption used above the big stat values.
QLabel* MakeCaption(const QString& text, QWidget* parent)
{
    QLabel* caption = new QLabel(text, parent);
    caption->setStyleSheet(QStringLiteral("color:%1; font-size:11px; font-weight:bold;")
        .arg(QLatin1String(brand::Muted)));
    return caption;
}

//! Status pill styles (Idle / Mining / Paused).
QString PillStyle(const char* bg, const char* fg)
{
    return QStringLiteral(
        "background-color:%1; color:%2; border-radius:11px; padding:4px 14px; font-weight:bold;")
        .arg(QLatin1String(bg), QLatin1String(fg));
}
QString PillIdle()   { return PillStyle(brand::Surface, brand::Muted); }
QString PillMining() { return PillStyle("#123f1e", brand::SuccessLight); }
QString PillPaused() { return PillStyle("#4a2c08", brand::Gold); }

//! The primary CTA: gold while idle ("Start Mining"), danger outline while
//! mining ("Stop Mining").
QString StartButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background-color:%1; color:%2; border:none; border-radius:8px;"
        "  padding:10px 26px; font-weight:bold; font-size:14px; }"
        "QPushButton:hover { background-color:%3; }"
        "QPushButton:pressed { background-color:%4; }"
        "QPushButton:disabled { background-color:%5; color:%6; }")
        .arg(QLatin1String(brand::Gold), QLatin1String(brand::Brown),
             QLatin1String(brand::Hover), QLatin1String(brand::GoldDark),
             QLatin1String(brand::Surface), QLatin1String(brand::Disabled));
}
QString StopButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background-color:transparent; color:%1; border:1px solid %2;"
        "  border-radius:8px; padding:10px 26px; font-weight:bold; font-size:14px; }"
        "QPushButton:hover { background-color:#3a120d; }"
        "QPushButton:pressed { background-color:#2a0d09; }")
        .arg(QLatin1String(brand::DangerLight), QLatin1String(brand::Danger));
}
} // namespace

MiningPage::MiningPage(const PlatformStyle* platform_style, QWidget* parent)
    : QWidget(parent), m_platform_style(platform_style)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(14);

    QLabel* title = new QLabel(tr("\xe2\x9b\x8f\xef\xb8\x8f  Mine BurritoCoin"), this);
    title->setStyleSheet(QStringLiteral("font-size:20px; font-weight:bold; color:%1;")
        .arg(QLatin1String(brand::Gold)));
    layout->addWidget(title);

    QLabel* intro = new QLabel(tr(
        "Mining uses your computer's processor to help secure the BurritoCoin network. "
        "When your computer finds a block, the reward is paid <b>directly into this wallet</b>. "
        "Start with one core \xe2\x80\x94 you can add more below. Leaving a core or two free keeps "
        "the rest of your computer responsive."), this);
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    intro->setStyleSheet(QStringLiteral("color:%1;").arg(QLatin1String(brand::Muted)));
    layout->addWidget(intro);

    // --- Hero card: status pill, live speed, start/stop CTA ---------------
    QFrame* hero = MakeCard(this);
    QVBoxLayout* hero_layout = new QVBoxLayout(hero);
    hero_layout->setContentsMargins(20, 16, 20, 16);
    hero_layout->setSpacing(6);

    QHBoxLayout* hero_top = new QHBoxLayout;
    m_status_value = new QLabel(tr("Idle"), hero);
    m_status_value->setStyleSheet(PillIdle());
    hero_top->addWidget(m_status_value, 0, Qt::AlignVCenter);
    hero_top->addStretch(1);
    m_start_stop = new QPushButton(tr("Start Mining"), hero);
    m_start_stop->setMinimumHeight(44);
    m_start_stop->setMinimumWidth(190);
    m_start_stop->setCursor(Qt::PointingHandCursor);
    m_start_stop->setStyleSheet(StartButtonStyle());
    hero_top->addWidget(m_start_stop, 0, Qt::AlignVCenter);
    hero_layout->addLayout(hero_top);

    // Qt::AutoText (the default) renders plain hashrate strings verbatim and
    // still detects the tag-prefixed error span from onMiningError().
    m_hashrate_value = new QLabel(QString::fromUtf8("\xe2\x80\x94"), hero);
    m_hashrate_value->setStyleSheet(QStringLiteral("font-size:34px; font-weight:bold; color:%1;")
        .arg(QLatin1String(brand::GoldLight)));
    hero_layout->addWidget(m_hashrate_value);

    hero_layout->addWidget(MakeCaption(tr("CURRENT SPEED"), hero));
    layout->addWidget(hero);

    // --- Stat cards: blocks found + expected time to a block --------------
    QHBoxLayout* stats_row = new QHBoxLayout;
    stats_row->setSpacing(14);

    QFrame* found_card = MakeCard(this);
    QVBoxLayout* found_layout = new QVBoxLayout(found_card);
    found_layout->setContentsMargins(20, 14, 20, 14);
    found_layout->setSpacing(4);
    found_layout->addWidget(MakeCaption(tr("BLOCKS FOUND THIS SESSION"), found_card));
    m_blocks_value = new QLabel(QStringLiteral("0"), found_card);
    m_blocks_value->setStyleSheet(QStringLiteral("font-size:18px; font-weight:bold; color:%1;")
        .arg(QLatin1String(brand::Text)));
    found_layout->addWidget(m_blocks_value);
    stats_row->addWidget(found_card, 1);

    QFrame* eta_card = MakeCard(this);
    QVBoxLayout* eta_layout = new QVBoxLayout(eta_card);
    eta_layout->setContentsMargins(20, 14, 20, 14);
    eta_layout->setSpacing(4);
    eta_layout->addWidget(MakeCaption(tr("ESTIMATED TIME TO A BLOCK"), eta_card));
    m_eta_value = new QLabel(QString::fromUtf8("\xe2\x80\x94"), eta_card);
    m_eta_value->setStyleSheet(QStringLiteral("font-size:18px; font-weight:bold; color:%1;")
        .arg(QLatin1String(brand::Text)));
    eta_layout->addWidget(m_eta_value);
    stats_row->addWidget(eta_card, 1);

    layout->addLayout(stats_row);

    // --- Settings card: core selector + auto-pause options ----------------
    const int max_threads = MiningModel::maxThreads();

    QFrame* settings_card = MakeCard(this);
    QVBoxLayout* settings_layout = new QVBoxLayout(settings_card);
    settings_layout->setContentsMargins(20, 14, 20, 16);
    settings_layout->setSpacing(10);

    QLabel* settings_header = new QLabel(tr("Mining settings"), settings_card);
    settings_header->setStyleSheet(QStringLiteral("font-size:13px; font-weight:bold; color:%1;")
        .arg(QLatin1String(brand::Gold)));
    settings_layout->addWidget(settings_header);

    QHBoxLayout* cores_row = new QHBoxLayout;
    QLabel* cores_caption = new QLabel(tr("Processor cores:"), settings_card);
    m_thread_slider = new QSlider(Qt::Horizontal, settings_card);
    m_thread_slider->setMinimum(1);
    m_thread_slider->setMaximum(max_threads);
    m_thread_slider->setValue(1); // conservative default: a single core
    m_thread_slider->setPageStep(1);
    m_thread_slider->setTickPosition(QSlider::TicksBelow);
    m_thread_label = new QLabel(settings_card);
    m_thread_label->setMinimumWidth(110);
    cores_row->addWidget(cores_caption);
    cores_row->addWidget(m_thread_slider, 1);
    cores_row->addWidget(m_thread_label);
    settings_layout->addLayout(cores_row);

    m_all_cores = new QCheckBox(tr("Use all cores"), settings_card);
    settings_layout->addWidget(m_all_cores);

    QSettings pause_settings;
    m_pause_battery = new QCheckBox(tr("Pause while on battery power"), settings_card);
    m_pause_battery->setChecked(pause_settings.value(QStringLiteral("mining/pause_on_battery"), true).toBool());
    connect(m_pause_battery, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(QStringLiteral("mining/pause_on_battery"), on);
    });
    settings_layout->addWidget(m_pause_battery);

    m_pause_busy = new QCheckBox(tr("Pause while I'm using the computer"), settings_card);
    m_pause_busy->setChecked(pause_settings.value(QStringLiteral("mining/pause_when_busy"), false).toBool());
    connect(m_pause_busy, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(QStringLiteral("mining/pause_when_busy"), on);
    });
    settings_layout->addWidget(m_pause_busy);

    // --- Advanced: mine with an external program (tuned CPU / GPU) ----------
    // The wallet runs a private localhost Stratum bridge and feeds it with the
    // chosen miner (cpuminer-opt for tuned CPU, ccminer/sgminer for GPU), which
    // are far faster than the in-process engine. (Phase 3 makes the GPU case
    // auto-detect + auto-download; for now the path is chosen here.)
    m_use_external = new QCheckBox(
        tr("Use an external miner program (advanced \xe2\x80\x94 much faster / GPU)"), settings_card);
    settings_layout->addWidget(m_use_external);

    QHBoxLayout* miner_row = new QHBoxLayout;
    QLabel* miner_caption = new QLabel(tr("Miner program:"), settings_card);
    m_miner_path = new QLineEdit(settings_card);
    m_miner_path->setPlaceholderText(tr("path to cpuminer / ccminer / sgminer"));
    m_miner_path->setText(QSettings().value(QStringLiteral("mining/external_path")).toString());
    m_browse_button = new QPushButton(tr("Browse\xe2\x80\xa6"), settings_card);
    miner_row->addWidget(miner_caption);
    miner_row->addWidget(m_miner_path, 1);
    miner_row->addWidget(m_browse_button);
    settings_layout->addLayout(miner_row);

    layout->addWidget(settings_card);

    if (max_threads <= 1) {
        // Single-core machine: nothing to choose.
        m_thread_slider->setEnabled(false);
        m_all_cores->setEnabled(false);
    }

    // External-miner output, shown only while the external engine is selected so
    // a failed handshake is visible instead of a black box.
    m_miner_log = new QPlainTextEdit(this);
    m_miner_log->setReadOnly(true);
    m_miner_log->setMaximumBlockCount(500);
    m_miner_log->setFixedHeight(140);
    m_miner_log->setPlaceholderText(tr("Miner output will appear here."));
    m_miner_log->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:8px;"
        " font-family:Consolas,'Courier New',monospace; font-size:11px; padding:6px; }")
        .arg(QLatin1String(brand::CodeBg), QLatin1String(brand::Muted), QLatin1String(brand::Border)));
    m_miner_log->setVisible(false);
    layout->addWidget(m_miner_log);

    QLabel* maturity_note = new QLabel(tr(
        "Note: newly mined coins need 100 confirmations (about 4 hours) before they "
        "can be spent. They will appear as \xe2\x80\x9cimmature\xe2\x80\x9d until then."), this);
    maturity_note->setWordWrap(true);
    maturity_note->setStyleSheet(QStringLiteral("color:%1; font-size:12px;")
        .arg(QLatin1String(brand::MutedGrey)));
    layout->addWidget(maturity_note);

    layout->addStretch(1);

    connect(m_start_stop, &QPushButton::clicked, this, &MiningPage::onStartStopClicked);
    connect(m_all_cores, &QCheckBox::toggled, this, &MiningPage::onUseAllCoresToggled);
    connect(m_thread_slider, &QSlider::valueChanged, this, &MiningPage::onThreadSliderChanged);
    connect(m_use_external, &QCheckBox::toggled, this, &MiningPage::onUseExternalToggled);
    connect(m_browse_button, &QPushButton::clicked, this, &MiningPage::onBrowseMiner);

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
    if (!client_model) {
        if (m_miner) m_miner->stop();
        // Kill the external miner and stop the bridge before the node (and the
        // chainstate/mempool the bridge holds) is torn down.
        if (m_external) m_external->stop();
        if (m_bridge) m_bridge->stop();
        m_external_active = false;
    }
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
    connect(m_miner, &MiningModel::pauseStateChanged, this, &MiningPage::onPauseStateChanged);
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
    const bool busy = isBusy();
    const bool ext = m_use_external && m_use_external->isChecked();
    const bool can_mine = m_wallet_model != nullptr && (ext || m_miner != nullptr);
    const int max_threads = MiningModel::maxThreads();

    m_start_stop->setEnabled(can_mine || busy);

    // Built-in core controls apply to the in-process engine only, and lock while
    // busy; the external miner manages its own threads.
    const bool core_ok = !busy && !ext && max_threads > 1;
    m_thread_slider->setEnabled(core_ok);
    m_all_cores->setEnabled(core_ok);
    if (m_pause_battery) m_pause_battery->setEnabled(!busy && !ext);
    if (m_pause_busy) m_pause_busy->setEnabled(!busy && !ext);

    // Engine choice + miner path can only change while stopped.
    if (m_use_external) m_use_external->setEnabled(!busy);
    if (m_miner_path) m_miner_path->setEnabled(ext && !busy);
    if (m_browse_button) m_browse_button->setEnabled(ext && !busy);
}

void MiningPage::onStartStopClicked()
{
    const bool ext = m_use_external && m_use_external->isChecked();

    // Stop path.
    if (m_external_active) { stopExternal(); return; }
    if (!ext && m_miner && m_miner->isMining()) { m_miner->stop(); return; }

    // Start path — the one-time disclosure applies to any engine.
    QSettings settings;
    if (!settings.value(QStringLiteral("mining/disclosure_shown"), false).toBool()) {
        QMessageBox::information(this, tr("About mining"), tr(
            "Mining runs your processor hard \xe2\x80\x94 it uses electricity and produces heat. "
            "A few things to know:\n\n"
            "\xe2\x80\xa2 Your antivirus may flag the wallet or the miner as a \xe2\x80\x9ccoin "
            "miner\xe2\x80\x9d. That is expected for mining software; if it quarantines it, restore "
            "it from your antivirus's quarantine list.\n\n"
            "\xe2\x80\xa2 Newly mined coins are \xe2\x80\x9cimmature\xe2\x80\x9d for 100 confirmations "
            "(about 4 hours) before you can spend them.\n\n"
            "\xe2\x80\xa2 You can have mining pause automatically on battery or while you're using "
            "the computer, in Settings \xe2\x80\xba Mining.\n\n"
            "This won't be shown again."));
        settings.setValue(QStringLiteral("mining/disclosure_shown"), true);
    }

    if (ext) startExternal();
    else if (m_miner) m_miner->start(selectedThreadCount());
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
    applyMiningUi(mining);
}

void MiningPage::onHashrate(double hashes_per_sec)
{
    m_hashrate_value->setText(FormatHashrate(hashes_per_sec));

    // Expected solo time to a block = (expected hashes per block) / your hashrate,
    // where expected hashes per block = difficulty * 2^32 (difficulty is reported
    // relative to the 0x1d00ffff diff-1 target). This is the correct estimate even
    // when the chain runs far below its target block spacing — do NOT derive it
    // from networkHashPS * nPowTargetSpacing, which assumes blocks arrive every
    // 150 s and so badly underestimates the ETA on an under-powered chain.
    // Difficulty only changes at retargets, so refresh it on a slow cadence.
    if (m_client_model && (m_eta_refresh_tick++ % 10 == 0)) {
        m_cached_difficulty = m_client_model->getDifficulty();
    }
    if (hashes_per_sec <= 0.0 || m_cached_difficulty <= 0.0) {
        m_eta_value->setText(QString::fromUtf8("\xe2\x80\x94"));
        return;
    }
    const double hashes_per_block = m_cached_difficulty * 4294967296.0; // difficulty * 2^32
    m_eta_value->setText(FormatDuration(hashes_per_block / hashes_per_sec));
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
    // If the external engine errored (launch failure / bridge issue / crash),
    // tear it down before surfacing the message.
    if (m_external_active) {
        if (m_external) m_external->stop();
        if (m_bridge) m_bridge->stop();
        m_external_active = false;
        applyMiningUi(false);
    }
    m_status_value->setText(tr("Idle"));
    m_status_value->setStyleSheet(PillIdle());
    // A modal would be heavy for transient cases; surface inline in the speed
    // slot (small, so a long message doesn't render at hero size).
    m_hashrate_value->setText(QStringLiteral("<span style='font-size:13px; color:%1;'>%2</span>")
                                  .arg(QLatin1String(brand::DangerLight), message.toHtmlEscaped()));
}

void MiningPage::onPauseStateChanged(bool paused, const QString& reason)
{
    if (!m_miner || !m_miner->isMining()) return;
    if (paused) {
        m_status_value->setText(reason);
        m_status_value->setStyleSheet(PillPaused());
        m_hashrate_value->setText(QString::fromUtf8("\xe2\x80\x94"));
        m_eta_value->setText(QString::fromUtf8("\xe2\x80\x94"));
    } else {
        m_status_value->setText(tr("Mining"));
        m_status_value->setStyleSheet(PillMining());
    }
}

// --- External-miner (Stratum-bridge) engine ------------------------------

bool MiningPage::isBusy() const
{
    return (m_miner && m_miner->isMining()) || m_external_active;
}

void MiningPage::applyMiningUi(bool mining)
{
    m_start_stop->setText(mining ? tr("Stop Mining") : tr("Start Mining"));
    m_start_stop->setStyleSheet(mining ? StopButtonStyle() : StartButtonStyle());
    m_status_value->setText(mining ? tr("Mining") : tr("Idle"));
    m_status_value->setStyleSheet(mining ? PillMining() : PillIdle());
    if (!mining) m_hashrate_value->setText(QString::fromUtf8("\xe2\x80\x94"));
    updateControlsEnabled();
}

void MiningPage::onUseExternalToggled(bool checked)
{
    if (m_miner_log) m_miner_log->setVisible(checked);
    updateControlsEnabled();
}

void MiningPage::onMinerLog(const QString& line)
{
    if (m_miner_log) m_miner_log->appendPlainText(line);
}

void MiningPage::onBrowseMiner()
{
    const QString current = m_miner_path ? m_miner_path->text() : QString();
    const QString start_dir = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Choose miner program"), start_dir,
#ifdef Q_OS_WIN
        tr("Programs (*.exe);;All files (*)")
#else
        tr("All files (*)")
#endif
    );
    if (!file.isEmpty() && m_miner_path) m_miner_path->setText(file);
}

void MiningPage::startExternal()
{
    if (!m_client_model) { onMiningError(tr("The node isn't ready yet.")); return; }
    if (!m_wallet_model) { onMiningError(tr("Open a wallet before mining.")); return; }

    const QString program = m_miner_path ? m_miner_path->text().trimmed() : QString();
    if (program.isEmpty()) {
        onMiningError(tr("Choose a miner program first (Browse\xe2\x80\xa6)."));
        return;
    }

    // Resolve the wallet payout (same cache/behaviour as the in-process miner).
    CScript payout;
    QString err;
    if (!MiningUtil::ResolveCoinbaseScript(m_wallet_model, payout, err)) {
        onMiningError(err);
        return;
    }

    if (!m_bridge) {
        m_bridge = new StratumBridge(m_client_model->node(), this);
        connect(m_bridge, &StratumBridge::hashrateChanged, this, &MiningPage::onHashrate);
        connect(m_bridge, &StratumBridge::blockFound, this, &MiningPage::onBlockFound);
        connect(m_bridge, &StratumBridge::error, this, &MiningPage::onMiningError);
    }
    const quint16 port = m_bridge->start(payout, 0);
    if (port == 0) {
        // start() already emitted error() synchronously with the specific reason
        // (e.g. the OS listen error), which onMiningError surfaced. Don't clobber
        // that with a generic message — just abort; the UI is still idle here.
        return;
    }

    if (!m_external) {
        m_external = new ExternalMiner(this);
        connect(m_external, &ExternalMiner::failed, this, &MiningPage::onMiningError);
        connect(m_external, &ExternalMiner::stopped, this, &MiningPage::onExternalStopped);
        connect(m_external, &ExternalMiner::logLine, this, &MiningPage::onMinerLog);
    }
    if (m_miner_log) m_miner_log->clear();
    // scrypt over the localhost bridge; the miner's worker/password are ignored
    // (the bridge pays to the wallet-resolved script, not the stratum user).
    const QStringList args = {
        QStringLiteral("-a"), QStringLiteral("scrypt"),
        QStringLiteral("-o"), QStringLiteral("stratum+tcp://127.0.0.1:%1").arg(port),
        QStringLiteral("-u"), QStringLiteral("brto"),
        QStringLiteral("-p"), QStringLiteral("x"),
    };
    m_external->start(program, args);
    QSettings().setValue(QStringLiteral("mining/external_path"), program);

    m_external_active = true;
    applyMiningUi(true);
}

void MiningPage::stopExternal()
{
    if (m_external) m_external->stop();
    if (m_bridge) m_bridge->stop();
    m_external_active = false;
    applyMiningUi(false);
}

void MiningPage::onExternalStopped(int exit_code)
{
    Q_UNUSED(exit_code);
    if (!m_external_active) return; // already torn down by stopExternal()/onMiningError()
    if (m_bridge) m_bridge->stop();
    m_external_active = false;
    applyMiningUi(false);
}
