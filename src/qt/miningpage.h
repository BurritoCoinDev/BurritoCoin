// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_MININGPAGE_H
#define BURRITOCOIN_QT_MININGPAGE_H

#include <QWidget>

class ClientModel;
class MiningModel;
class PlatformStyle;
class WalletModel;

class QCheckBox;
class QLabel;
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

private:
    void ensureMiner();
    void updateThreadLabel();
    void updateControlsEnabled();
    int selectedThreadCount() const;

    const PlatformStyle* m_platform_style;
    ClientModel* m_client_model = nullptr;
    WalletModel* m_wallet_model = nullptr;
    MiningModel* m_miner = nullptr;

    QSlider* m_thread_slider = nullptr;
    QLabel* m_thread_label = nullptr;
    QCheckBox* m_all_cores = nullptr;
    QPushButton* m_start_stop = nullptr;
    QLabel* m_status_value = nullptr;
    QLabel* m_hashrate_value = nullptr;
    QLabel* m_blocks_value = nullptr;
    QLabel* m_eta_value = nullptr;

    int m_blocks_session = 0;
    double m_cached_net_hps = 0.0;
    int m_net_hps_tick = 0;
};

#endif // BURRITOCOIN_QT_MININGPAGE_H
