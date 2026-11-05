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
 * Workers are plain std::threads communicating through atomics, plus one
 * mutex around the payout script (taken once per template, not per hash); a
 * QTimer on the GUI thread polls those atomics to compute hashrate, detect a
 * new tip (so workers reload onto it), and emit Qt signals for the UI. The
 * wallet is only touched on the GUI thread. This keeps the hot loop free of
 * any QObject affinity concerns and lets Stop() interrupt within a single hash
 * instead of a long blocking RPC.
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
    //! After a found block: switch to a fresh payout address, then let the
    //! workers build their next templates. Queued from the worker that found it.
    void rotatePayout();

private:
    void workerLoop(int id);
    //! payout_gen is the m_payout_gen the block's coinbase script came from.
    void submitSolved(const CBlock& block, uint64_t payout_gen);
    bool resolveCoinbaseScript(QString& err);

    interfaces::Node& m_node;
    WalletModel* m_wallet_model = nullptr;

    // Node-side handles captured at start(); valid for the node's lifetime.
    ChainstateManager* m_chainman = nullptr;
    CTxMemPool* m_mempool = nullptr;

    // The payout script, replaced after every block it pays so each block pays
    // a different address. All three are guarded by m_script_mutex, which is
    // taken before cs_main (submitSolved holds it across ProcessNewBlock),
    // never after.
    std::mutex m_script_mutex;
    CScript m_coinbase_script;
    uint64_t m_payout_gen = 0;   //!< bumped whenever m_coinbase_script changes
    bool m_payout_used = false;  //!< a block paid it; no new templates until rotatePayout()

    // rotatePayout()'s replacement while it waits for m_script_mutex (GUI thread only).
    bool m_next_payout_ready = false;
    bool m_next_payout_ok = false;
    CScript m_next_payout;

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
