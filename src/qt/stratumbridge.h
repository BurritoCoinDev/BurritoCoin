// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_STRATUMBRIDGE_H
#define BURRITOCOIN_QT_STRATUMBRIDGE_H

#include <script/script.h>
#include <stratumjob.h>

#include <QHash>
#include <QObject>
#include <QString>

#include <cstdint>
#include <deque>
#include <memory>

class CTxMemPool;
class ChainstateManager;
namespace interfaces { class Node; }

QT_BEGIN_NAMESPACE
class QTcpServer;
class QTcpSocket;
class QTimer;
QT_END_NAMESPACE

/**
 * A minimal, LOCALHOST-ONLY Stratum v1 server that lets an external miner
 * (cpuminer-opt for tuned CPU, or ccminer/sgminer for GPU) mine BurritoCoin
 * straight into this wallet.
 *
 * Design / trust model:
 *  - Binds 127.0.0.1 ONLY. The miner interface can submit blocks, so it must
 *    never be reachable off the machine; there is no auth beyond that.
 *  - All consensus-adjacent work lives in the already-unit-tested StratumJob:
 *    this class only speaks the wire protocol and manages the job lifecycle.
 *  - The miner is NEVER trusted. A submitted share is turned back into a full
 *    block via StratumJob::reconstructBlock and re-validated through the normal
 *    MiningUtil::SubmitBlock (ProcessNewBlock) path; only a share whose header
 *    actually meets the NETWORK target is submitted. Lower shares are counted
 *    for the hashrate readout and discarded.
 *
 * Wire specifics:
 *  - extranonce1 = 4 server bytes; extranonce2_size = 4 (the two fill the
 *    8-byte extranonce sentinel StratumJob splits coinb1/coinb2 at).
 *  - A fresh job is issued on every new tip (clean_jobs=true) and on a slow
 *    ntime-roll timer; jobs are kept in a small ring so a slightly-late share
 *    for the previous template still reconstructs.
 *
 * Threading: lives on the GUI thread. Template assembly and block submission
 * take cs_main internally (via MiningUtil); kept off the hot path by only
 * assembling on tip-change / timer, not per share.
 */
class StratumBridge : public QObject
{
    Q_OBJECT

public:
    explicit StratumBridge(interfaces::Node& node, QObject* parent = nullptr);
    ~StratumBridge();

    //! Bind 127.0.0.1 and begin serving work paying to payout_script. Returns
    //! the bound port, or 0 on failure (port in use / node not ready). If
    //! preferred_port is 0 an ephemeral port is chosen.
    quint16 start(const CScript& payout_script, quint16 preferred_port = 0);
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }

Q_SIGNALS:
    //! Aggregate hashrate estimated from accepted shares (H/s).
    void hashrateChanged(double hashes_per_sec);
    //! A share met the network target and ProcessNewBlock accepted the block.
    void blockFound(const QString& block_hash, int height);
    //! At least one miner is currently connected/subscribed.
    void minerConnected(bool connected);
    //! Non-fatal surface for the UI (bind failure, template error, etc.).
    void error(const QString& message);

private Q_SLOTS:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void rebuildJob();        //!< assemble a fresh template + notify subscribers
    void pollTip();           //!< detect a new tip and force a clean job
    void estimateHashrate();  //!< fold the share window into a H/s figure

private:
    struct Session; // per-connection: socket, extranonce1, subscribed, line buffer

    void handleLine(Session& s, const QString& line);
    void sendJson(Session& s, const QString& obj);
    void notifyJob(Session& s, bool clean_jobs);
    void notifyAll(bool clean_jobs);
    bool processSubmit(Session& s, const QString& job_id,
                       const std::vector<unsigned char>& extranonce2,
                       uint32_t ntime, uint32_t nonce, QString& err);

    interfaces::Node& m_node;
    ChainstateManager* m_chainman = nullptr;
    CTxMemPool* m_mempool = nullptr;
    CScript m_payout_script;

    std::unique_ptr<QTcpServer> m_server;
    QHash<QTcpSocket*, std::shared_ptr<Session>> m_sessions;
    quint16 m_port = 0;

    std::deque<StratumJob> m_jobs;   //!< recent jobs (job_id lookup on submit)
    uint32_t m_next_extranonce1 = 1; //!< handed out per session
    uint64_t m_job_seq = 0;          //!< monotonic job-id source
    int m_last_tip_height = -1;

    QTimer* m_job_timer = nullptr;   //!< ntime-roll / template refresh
    QTimer* m_tip_timer = nullptr;   //!< new-tip poll
    QTimer* m_rate_timer = nullptr;  //!< hashrate estimate tick

    // Share-rate accounting for the hashrate estimate.
    double m_share_difficulty = 0.0; //!< set_difficulty value sent to miners
    std::deque<qint64> m_share_times_ms; //!< timestamps of recent accepted shares
    uint64_t m_blocks_found = 0;
};

#endif // BURRITOCOIN_QT_STRATUMBRIDGE_H
