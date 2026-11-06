// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningmodel.h>

#include <qt/miningpower.h>
#include <qt/miningutil.h>
#include <qt/walletmodel.h>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <logging.h>
#include <miner.h>
#include <node/context.h>
#include <outputtype.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <validation.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>

#include <QSettings>
#include <QThread>
#include <QTimer>

MiningModel::MiningModel(interfaces::Node& node, QObject* parent)
    : QObject(parent), m_node(node)
{
    m_poll_timer = new QTimer(this);
    connect(m_poll_timer, &QTimer::timeout, this, &MiningModel::poll);
}

MiningModel::~MiningModel()
{
    stop();
}

void MiningModel::setWalletModel(WalletModel* wallet_model)
{
    m_wallet_model = wallet_model;
}

int MiningModel::maxThreads()
{
    const int n = QThread::idealThreadCount();
    return n > 0 ? n : 1;
}

bool MiningModel::resolveCoinbaseScript(QString& err)
{
    CScript script;
    if (!MiningUtil::ResolveCoinbaseScript(m_wallet_model, script, err)) return false;
    std::lock_guard<std::mutex> lock(m_script_mutex);
    m_coinbase_script = script;
    ++m_payout_gen;
    m_payout_used = false;
    return true;
}

void MiningModel::start(int threads)
{
    if (m_running.load()) return;

    QString err;
    if (!MiningUtil::CaptureNodeHandles(m_node, m_chainman, m_mempool, err)) {
        Q_EMIT error(err);
        return;
    }
    if (!resolveCoinbaseScript(err)) {
        Q_EMIT error(err);
        return;
    }

    threads = std::max(1, std::min(threads, maxThreads()));

    m_running.store(true);
    m_paused.store(false); // begin un-paused; the first poll() re-derives the real state
    m_hashes.store(0);
    m_last_hashes = 0;
    m_last_emitted_found = m_blocks_found.load();
    m_last_height = m_node.getNumBlocks();
    m_rate_timer.start();
    m_active_threads = threads;

    m_workers.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        m_workers.emplace_back([this, i] { workerLoop(i); });
    }

    m_poll_timer->start(1000);
    Q_EMIT stateChanged(true);
}

void MiningModel::stop()
{
    if (!m_running.load()) return;

    m_running.store(false);
    // Workers re-check m_running between hashes, so each exits within a single
    // scrypt evaluation (sub-millisecond to a few ms) — Stop feels instant.
    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
    m_workers.clear();

    if (m_poll_timer) m_poll_timer->stop();
    m_active_threads = 0;
    Q_EMIT hashrateChanged(0.0);
    Q_EMIT stateChanged(false);
}

void MiningModel::workerLoop(int id)
{
    // Give each worker a disjoint extranonce band so their coinbases — and thus
    // their block headers — differ and they never duplicate each other's work.
    unsigned int extra_nonce = static_cast<unsigned int>(id) << 20;
    const CChainParams& params = Params();
    const Consensus::Params& consensus = params.GetConsensus();

    while (m_running.load()) {
        if (m_paused.load()) {
            // Auto-paused (on battery / user busy): idle without burning CPU.
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        const uint32_t epoch = m_epoch.load();
        CScript coinbase_script;
        uint64_t payout_gen;
        bool payout_used;
        {
            std::lock_guard<std::mutex> lock(m_script_mutex);
            coinbase_script = m_coinbase_script;
            payout_gen = m_payout_gen;
            payout_used = m_payout_used;
        }
        if (payout_used) {
            // A block just paid this address; wait (milliseconds) for
            // rotatePayout() so the next template pays a new one.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        std::unique_ptr<CBlockTemplate> tmpl;
        try {
            tmpl = BlockAssembler(*m_mempool, params).CreateNewBlock(coinbase_script);
        } catch (const std::exception&) {
            tmpl.reset();
        }
        if (!tmpl) {
            // Transient (e.g. tip moving under us); back off briefly and retry.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        CBlock block = tmpl->block;
        {
            // Stamp THIS worker's own extranonce band straight into the coinbase
            // rather than calling IncrementExtraNonce(): that helper keeps one
            // process-global static and resets the caller's value to 0 on the
            // first call after a tip change, which would collapse our per-worker
            // bands and make two workers grind the identical header. We mirror
            // IncrementExtraNonce's coinbase layout (BIP34 height-first, scriptSig
            // <= 100 bytes) exactly, just without the shared static.
            LOCK(cs_main);
            // Derive the BIP34 height from the template's OWN parent, not the
            // live tip: if the tip advanced since CreateNewBlock fixed
            // hashPrevBlock (e.g. a sibling worker just connected a block), a
            // live-tip height would mismatch the parent and the solved block
            // would be rejected 'bad-cb-height'. This keeps the coinbase
            // self-consistent with the template under multi-worker tip races.
            const CBlockIndex* prev = LookupBlockIndex(block.hashPrevBlock);
            if (!prev) continue; // template's parent vanished (deep reorg) — reload
            const int height = prev->nHeight + 1;
            CMutableTransaction coinbase(*block.vtx[0]);
            coinbase.vin[0].scriptSig = (CScript() << height << CScriptNum(extra_nonce));
            assert(coinbase.vin[0].scriptSig.size() <= 100);
            block.vtx[0] = MakeTransactionRef(std::move(coinbase));
            block.hashMerkleRoot = BlockMerkleRoot(block);
        }
        ++extra_nonce; // advance this worker's band for the next template
        block.nNonce = 0;
        {
            // Another worker may have found a block paying this address while
            // the template was being built; it would pay the address twice.
            std::lock_guard<std::mutex> lock(m_script_mutex);
            if (m_payout_used || m_payout_gen != payout_gen) continue;
        }

        while (m_running.load() && !m_paused.load() && m_epoch.load() == epoch &&
               block.nNonce < std::numeric_limits<uint32_t>::max()) {
            if (CheckProofOfWork(block.GetPoWHash(), block.nBits, consensus)) {
                submitSolved(block, payout_gen);
                break; // reload a fresh template on the next outer iteration
            }
            ++block.nNonce;
            m_hashes.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void MiningModel::submitSolved(const CBlock& block, uint64_t payout_gen)
{
    // Held across the submit so that no worker can copy the payout script
    // between this block connecting and the script being marked used below.
    std::lock_guard<std::mutex> script_lock(m_script_mutex);
    std::string hash;
    if (MiningUtil::SubmitBlock(*m_chainman, block, hash)) {
        {
            std::lock_guard<std::mutex> lock(m_found_mutex);
            m_last_found_hash = QString::fromStdString(hash);
        }
        m_blocks_found.fetch_add(1);
        if (payout_gen == m_payout_gen && !m_payout_used) {
            // This block paid the current address. Workers wait until
            // rotatePayout() (on the GUI thread, which is the only one that
            // touches the wallet) has replaced it.
            m_payout_used = true;
            QMetaObject::invokeMethod(this, "rotatePayout", Qt::QueuedConnection);
        }
        // Switch every worker onto the new tip immediately rather than waiting
        // for the next poll() to notice the height change.
        m_epoch.fetch_add(1);
    }
}

void MiningModel::rotatePayout()
{
    if (!m_next_payout_ready) {
        m_next_payout_ok = MiningUtil::RotateCoinbaseScript(m_wallet_model, m_next_payout);
        if (!m_next_payout_ok) {
            // Keep mining to the current address rather than stopping; the
            // next Start asks the wallet for a fresh one again.
            LogPrintf("Mining: could not get a new payout address from the wallet; reusing the current one\n");
        }
        m_next_payout_ready = true;
    }
    // Never block this (the GUI) thread on m_script_mutex: a worker holds it
    // across ProcessNewBlock, which can itself wait on the GUI thread to show a
    // modal error (AbortNode). Try again shortly instead.
    std::unique_lock<std::mutex> lock(m_script_mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        QTimer::singleShot(10, this, &MiningModel::rotatePayout);
        return;
    }
    if (m_next_payout_ok) {
        m_coinbase_script = m_next_payout;
        ++m_payout_gen;
    }
    m_payout_used = false;
    m_next_payout_ready = false;
    lock.unlock();
    m_epoch.fetch_add(1);
}

void MiningModel::poll()
{
    // Hashrate over the interval since the last poll.
    const uint64_t total = m_hashes.load();
    const qint64 elapsed_ms = m_rate_timer.restart();
    if (elapsed_ms > 0) {
        const double hps = static_cast<double>(total - m_last_hashes) * 1000.0 /
                           static_cast<double>(elapsed_ms);
        Q_EMIT hashrateChanged(hps);
    }
    m_last_hashes = total;

    // A changed tip (our block or the network's) means workers should reload.
    const int height = m_node.getNumBlocks();
    if (height != m_last_height) {
        m_last_height = height;
        m_epoch.fetch_add(1);
    }

    // Surface any newly found blocks — one signal per block, so a session that
    // solves more than one within a single poll interval still counts them all.
    const int found = m_blocks_found.load();
    while (m_last_emitted_found < found) {
        ++m_last_emitted_found;
        QString hash;
        {
            std::lock_guard<std::mutex> lock(m_found_mutex);
            hash = m_last_found_hash;
        }
        Q_EMIT blockFound(hash, height);
    }

    // Auto-pause decision, re-read from settings each tick (cheap, cached).
    QSettings settings;
    QString reason;
    if (settings.value(QStringLiteral("mining/pause_on_battery"), true).toBool() &&
        MiningPower::OnBattery()) {
        reason = tr("Paused \xe2\x80\x94 on battery");
    } else if (settings.value(QStringLiteral("mining/pause_when_busy"), false).toBool() &&
               MiningPower::IdleSeconds() < 120) {
        reason = tr("Paused \xe2\x80\x94 you're using the computer");
    }
    const bool pause = !reason.isEmpty();
    if (m_paused.exchange(pause) != pause) {
        Q_EMIT pauseStateChanged(pause, reason);
    }
}
