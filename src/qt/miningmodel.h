// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_MININGMODEL_H
#define BURRITOCOIN_QT_MININGMODEL_H

#include <script/script.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class CBlock;
class ChainstateManager;
class CTxMemPool;
class WalletModel;
class QTimer;

namespace interfaces {
class Node;
} // namespace interfaces

/**
 * In-process solo CPU miner for the wallet GUI.
 *
 * Owns a pool of std::thread workers, each of which assembles a block template
 * from the node's mempool (the same BlockAssembler path generatetoaddress uses),
 * stamps a per-worker extranonce into the coinbase, and walks the nonce space
 * running the scrypt PoW (CBlock::GetPoWHash) until it finds a share. Solved
 * blocks are submitted via ChainstateManager::ProcessNewBlock — exactly the
 * path src/rpc/mining.cpp uses.
 *
 * Workers are plain std::threads communicating only through atomics; a QTimer
 * on the GUI thread polls those atomics to compute hashrate, detect a new tip
 * (so workers reload onto it), and emit Qt signals for the UI. This keeps the
 * hot loop free of any QObject affinity concerns and lets Stop() interrupt
 * within a single hash instead of a long blocking RPC.
 */
class MiningModel : public QObject
{
    Q_OBJECT

public:
    explicit MiningModel(interfaces::Node& node, QObject* parent = nullptr);
    ~MiningModel();

    void setWalletModel(WalletModel* wallet_model);

    bool isMining() const { return m_running.load(); }
    int activeThreads() const { return m_active_threads; }

    //! Logical-core count; the natural upper bound for the thread selector.
    static int maxThreads();

public Q_SLOTS:
    //! Begin mining with the given thread count (clamped to [1, maxThreads()]).
    void start(int threads);
    //! Stop mining and join all workers. Returns once every worker has exited.
    void stop();

Q_SIGNALS:
    void stateChanged(bool mining);
    void hashrateChanged(double hashes_per_sec);
    void blockFound(const QString& block_hash, int height);
    void error(const QString& message);
    //! Auto-pause state changed; reason is human-readable when paused, empty when resumed.
    void pauseStateChanged(bool paused, const QString& reason);

private Q_SLOTS:
    void poll();

private:
    void workerLoop(int id);
    void submitSolved(const CBlock& block);
    bool resolveCoinbaseScript(QString& err);

    interfaces::Node& m_node;
    WalletModel* m_wallet_model = nullptr;

    // Node-side handles captured at start(); valid for the node's lifetime.
    ChainstateManager* m_chainman = nullptr;
    CTxMemPool* m_mempool = nullptr;
    CScript m_coinbase_script;

    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    std::atomic<uint32_t> m_epoch{0};   //!< bumped when the tip changes; workers reload
    std::atomic<bool> m_paused{false};  //!< auto-pause (battery/idle): workers idle while true
    std::atomic<uint64_t> m_hashes{0};  //!< cumulative hashes this session
    std::atomic<int> m_blocks_found{0};
    int m_active_threads = 0;

    std::mutex m_found_mutex;
    QString m_last_found_hash;
    int m_last_emitted_found = 0;

    QTimer* m_poll_timer = nullptr;
    QElapsedTimer m_rate_timer;
    uint64_t m_last_hashes = 0;
    int m_last_height = -1;
};

#endif // BURRITOCOIN_QT_MININGMODEL_H
