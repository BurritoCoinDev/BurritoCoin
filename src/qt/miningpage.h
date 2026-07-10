// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_MININGPAGE_H
#define BURRITOCOIN_QT_MININGPAGE_H

#include <QWidget>

class ClientModel;
class ExternalMiner;
class MiningModel;
class PlatformStyle;
class StratumBridge;
class WalletModel;

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;

/** Top-level "Mine" tab: a friendly front end for the in-process CPU miner. */
class MiningPage : public QWidget
{
    Q_OBJECT

public:
    explicit MiningPage(const PlatformStyle* platform_style, QWidget* parent = nullptr);
    ~MiningPage();

    void setClientModel(ClientModel* client_model);
    void setWalletModel(WalletModel* wallet_model);

private Q_SLOTS:
    void onStartStopClicked();
    void onUseAllCoresToggled(bool checked);
    void onThreadSliderChanged(int value);
    void onMiningStateChanged(bool mining);
    void onHashrate(double hashes_per_sec);
    void onBlockFound(const QString& block_hash, int height);
    void onMiningError(const QString& message);
    void onPauseStateChanged(bool paused, const QString& reason);
    void onUseExternalToggled(bool checked);
    void onBrowseMiner();
    void onExternalStopped(int exit_code);
    void onMinerLog(const QString& line);

private:
    void ensureMiner();
    void updateThreadLabel();
    void updateControlsEnabled();
    int selectedThreadCount() const;

    //! External-miner (Stratum-bridge) engine.
    void startExternal();
    void stopExternal();
    void applyMiningUi(bool mining); //!< engine-agnostic hero pill/button update
    bool isBusy() const;             //!< built-in mining OR external running
    void openMinerSetupGuide();      //!< separate window: how to install/point a miner

    const PlatformStyle* m_platform_style;
    ClientModel* m_client_model = nullptr;
    WalletModel* m_wallet_model = nullptr;
    MiningModel* m_miner = nullptr;

    // External-miner engine: a localhost Stratum bridge fed by a child miner
    // process (cpuminer-opt for tuned CPU, ccminer/sgminer for GPU).
    StratumBridge* m_bridge = nullptr;
    ExternalMiner* m_external = nullptr;
    bool m_external_active = false;

    QSlider* m_thread_slider = nullptr;
    QLabel* m_thread_label = nullptr;
    QCheckBox* m_all_cores = nullptr;
    QCheckBox* m_pause_battery = nullptr;
    QCheckBox* m_pause_busy = nullptr;
    QCheckBox* m_use_external = nullptr;
    QLineEdit* m_miner_path = nullptr;
    QPushButton* m_browse_button = nullptr;
    QPlainTextEdit* m_miner_log = nullptr;
    QPushButton* m_start_stop = nullptr;
    QLabel* m_status_value = nullptr;
    QLabel* m_hashrate_value = nullptr;
    QLabel* m_blocks_value = nullptr;
    QLabel* m_eta_value = nullptr;

    int m_blocks_session = 0;
    double m_cached_difficulty = 0.0;
    int m_eta_refresh_tick = 0;
};

#endif // BURRITOCOIN_QT_MININGPAGE_H
