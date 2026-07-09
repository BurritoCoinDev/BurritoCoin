// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/stratumbridge.h>

#include <qt/miningutil.h>

#include <chainparams.h>
#include <consensus/params.h>
#include <interfaces/node.h>
#include <miner.h>
#include <pow.h>
#include <primitives/block.h>
#include <sync.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>

#include <univalue.h>

#include <QAbstractSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <exception>
#include <memory>
#include <string>

namespace {
//! A deliberately low fixed share difficulty so even a CPU miner submits shares
//! often enough for a live hashrate readout; a GPU just submits more. Block
//! submission is gated on the real NETWORK target regardless of this value, so
//! it only affects the readout, never correctness. (Per-miner vardiff is a
//! future refinement.)
constexpr double SHARE_DIFFICULTY = 1.0 / 16384.0;
constexpr double HASHES_PER_DIFF1 = 4294967296.0; // 2^32

constexpr int JOB_REFRESH_MS = 30000; // ntime-roll / template refresh
constexpr int TIP_POLL_MS = 1500;     // new-tip detection
constexpr int RATE_MS = 2000;         // hashrate estimate tick
constexpr qint64 HASHRATE_WINDOW_MS = 15000;
constexpr size_t JOB_RING = 8;        // recent jobs kept for stale-share tolerance
} // namespace

//! Per-connection state. Defined here (the header only forward-declares it).
struct StratumBridge::Session {
    QTcpSocket* socket = nullptr;
    QByteArray buffer;        //!< partial line accumulator
    uint32_t extranonce1 = 0; //!< this session's 4-byte server extranonce
    bool subscribed = false;
};

StratumBridge::StratumBridge(interfaces::Node& node, QObject* parent)
    : QObject(parent), m_node(node)
{
}

StratumBridge::~StratumBridge()
{
    stop();
}

bool StratumBridge::isRunning() const
{
    return m_server != nullptr;
}

quint16 StratumBridge::start(const CScript& payout_script, quint16 preferred_port)
{
    if (isRunning()) return m_port;

    QString err;
    if (!MiningUtil::CaptureNodeHandles(m_node, m_chainman, m_mempool, err)) {
        Q_EMIT error(err);
        return 0;
    }
    m_payout_script = payout_script;
    m_share_difficulty = SHARE_DIFFICULTY;

    m_server.reset(new QTcpServer(this));
    connect(m_server.get(), &QTcpServer::newConnection, this, &StratumBridge::onNewConnection);
    // LOCALHOST ONLY — the miner interface can submit blocks; never expose it.
    if (!m_server->listen(QHostAddress::LocalHost, preferred_port)) {
        Q_EMIT error(tr("Could not start the local mining bridge: %1").arg(m_server->errorString()));
        m_server.reset();
        m_chainman = nullptr;
        m_mempool = nullptr;
        return 0;
    }
    m_port = m_server->serverPort();
    m_last_tip_height = -1;

    m_job_timer = new QTimer(this);
    connect(m_job_timer, &QTimer::timeout, this, &StratumBridge::rebuildJob);
    m_job_timer->start(JOB_REFRESH_MS);

    m_tip_timer = new QTimer(this);
    connect(m_tip_timer, &QTimer::timeout, this, &StratumBridge::pollTip);
    m_tip_timer->start(TIP_POLL_MS);

    m_rate_timer = new QTimer(this);
    connect(m_rate_timer, &QTimer::timeout, this, &StratumBridge::estimateHashrate);
    m_rate_timer->start(RATE_MS);

    rebuildJob(); // seed the first job
    return m_port;
}

void StratumBridge::stop()
{
    for (QTimer** t : {&m_job_timer, &m_tip_timer, &m_rate_timer}) {
        if (*t) { (*t)->stop(); (*t)->deleteLater(); *t = nullptr; }
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.key()) it.key()->disconnectFromHost();
    }
    m_sessions.clear();
    if (m_server) { m_server->close(); m_server.reset(); }
    m_jobs.clear();
    m_share_times_ms.clear();
    m_port = 0;
    m_chainman = nullptr;
    m_mempool = nullptr;
}

void StratumBridge::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        auto s = std::make_shared<Session>();
        s->socket = sock;
        m_sessions.insert(sock, s);
        connect(sock, &QTcpSocket::readyRead, this, &StratumBridge::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &StratumBridge::onClientDisconnected);
    }
}

void StratumBridge::onClientReadyRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    auto it = m_sessions.find(sock);
    if (it == m_sessions.end()) return;
    Session& s = *it.value();

    s.buffer += sock->readAll();
    int nl;
    while ((nl = s.buffer.indexOf('\n')) >= 0) {
        const QByteArray raw = s.buffer.left(nl);
        s.buffer.remove(0, nl + 1);
        const QString line = QString::fromUtf8(raw).trimmed();
        if (!line.isEmpty()) handleLine(s, line);
    }
}

void StratumBridge::onClientDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_sessions.remove(sock);
    sock->deleteLater();
    Q_EMIT minerConnected(!m_sessions.isEmpty());
}

void StratumBridge::handleLine(Session& s, const QString& line)
{
    UniValue msg;
    if (!msg.read(line.toStdString()) || !msg.isObject()) return;
    const UniValue& idv = msg["id"];
    const std::string method =
        (msg.exists("method") && msg["method"].isStr()) ? msg["method"].get_str() : "";
    const UniValue& params = msg["params"];

    if (method == "mining.subscribe") {
        s.extranonce1 = m_next_extranonce1++;
        s.subscribed = true;

        // result: [ [["mining.set_difficulty",id],["mining.notify",id]], en1_hex, en2_size ]
        UniValue subs(UniValue::VARR);
        UniValue sd(UniValue::VARR); sd.push_back("mining.set_difficulty"); sd.push_back("1");
        UniValue nt(UniValue::VARR); nt.push_back("mining.notify"); nt.push_back("1");
        subs.push_back(sd);
        subs.push_back(nt);
        UniValue result(UniValue::VARR);
        result.push_back(subs);
        result.push_back(stratum::EncodeBE32(s.extranonce1)); // 4-byte extranonce1
        result.push_back(4);                                  // extranonce2_size
        reply(s, idv, result, NullUniValue);

        sendSetDifficulty(s);
        if (!m_jobs.empty()) notifyJob(s, true);
        Q_EMIT minerConnected(true);

    } else if (method == "mining.authorize") {
        reply(s, idv, UniValue(true), NullUniValue); // solo: accept any worker

    } else if (method == "mining.extranonce.subscribe") {
        reply(s, idv, UniValue(true), NullUniValue);

    } else if (method == "mining.submit") {
        // params: [worker, job_id, extranonce2_hex, ntime_hex, nonce_hex]
        std::vector<unsigned char> en2;
        uint32_t ntime = 0, nonce = 0;
        const bool parsed =
            params.isArray() && params.size() >= 5 &&
            params[1].isStr() &&
            params[2].isStr() && stratum::DecodeHexBytes(params[2].get_str(), en2) &&
            params[3].isStr() && stratum::DecodeBE32(params[3].get_str(), ntime) &&
            params[4].isStr() && stratum::DecodeBE32(params[4].get_str(), nonce);

        QString err;
        const bool ok = parsed &&
            processSubmit(s, QString::fromStdString(params[1].get_str()), en2, ntime, nonce, err);
        if (ok) {
            reply(s, idv, UniValue(true), NullUniValue);
        } else {
            UniValue e(UniValue::VARR);
            e.push_back(20);
            e.push_back(err.isEmpty() ? std::string("rejected") : err.toStdString());
            e.push_back(NullUniValue);
            reply(s, idv, UniValue(false), e);
        }

    } else if (!idv.isNull()) {
        UniValue e(UniValue::VARR);
        e.push_back(20);
        e.push_back(std::string("unknown method"));
        e.push_back(NullUniValue);
        reply(s, idv, NullUniValue, e);
    }
}

void StratumBridge::sendJson(Session& s, const QString& obj)
{
    if (s.socket && s.socket->state() == QAbstractSocket::ConnectedState) {
        s.socket->write(obj.toUtf8());
        s.socket->write("\n", 1);
    }
}

void StratumBridge::reply(Session& s, const UniValue& id, const UniValue& result, const UniValue& error)
{
    UniValue o(UniValue::VOBJ);
    o.pushKV("id", id.isNull() ? NullUniValue : id);
    o.pushKV("result", result);
    o.pushKV("error", error);
    sendJson(s, QString::fromStdString(o.write()));
}

void StratumBridge::sendNotification(Session& s, const char* method, const UniValue& params)
{
    UniValue o(UniValue::VOBJ);
    o.pushKV("id", NullUniValue);
    o.pushKV("method", std::string(method));
    o.pushKV("params", params);
    sendJson(s, QString::fromStdString(o.write()));
}

void StratumBridge::sendSetDifficulty(Session& s)
{
    UniValue params(UniValue::VARR);
    params.push_back(m_share_difficulty);
    sendNotification(s, "mining.set_difficulty", params);
}

void StratumBridge::notifyJob(Session& s, bool clean_jobs)
{
    if (m_jobs.empty()) return;
    const StratumJob& job = m_jobs.back();

    UniValue p(UniValue::VARR);
    p.push_back(job.job_id);
    p.push_back(stratum::EncodeStratumPrevhash(job.block.hashPrevBlock));
    p.push_back(HexStr(job.coinb1));
    p.push_back(HexStr(job.coinb2));
    UniValue branch(UniValue::VARR);
    for (const uint256& h : job.merkle_branch) branch.push_back(stratum::EncodeHashLE(h));
    p.push_back(branch);
    p.push_back(stratum::EncodeBE32(static_cast<uint32_t>(job.version)));
    p.push_back(stratum::EncodeBE32(job.nbits));
    p.push_back(stratum::EncodeBE32(job.ntime));
    p.push_back(clean_jobs);
    sendNotification(s, "mining.notify", p);
}

void StratumBridge::notifyAll(bool clean_jobs)
{
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()->subscribed) notifyJob(*it.value(), clean_jobs);
    }
}

void StratumBridge::rebuildJob()
{
    if (!m_chainman || !m_mempool) return;

    std::unique_ptr<CBlockTemplate> tmpl;
    try {
        tmpl = BlockAssembler(*m_mempool, Params()).CreateNewBlock(m_payout_script);
    } catch (const std::exception&) {
        tmpl.reset();
    }
    if (!tmpl) return; // transient (tip moving under us) — retried on the next tick

    const CBlockIndex* parent = nullptr;
    {
        LOCK(cs_main);
        parent = LookupBlockIndex(tmpl->block.hashPrevBlock);
    }
    if (!parent) return; // template's parent vanished (deep reorg)

    const uint256 prevhash = tmpl->block.hashPrevBlock;
    const bool clean = m_jobs.empty() || m_jobs.back().block.hashPrevBlock != prevhash;

    StratumJob job;
    // extranonce1 here is a placeholder: coinb1/coinb2 are sliced at a fixed
    // sentinel and are independent of its value, so each session reconstructs
    // with its OWN extranonce1 (set in processSubmit).
    job.buildFromTemplate(*tmpl, parent, /*extranonce1=*/0, std::to_string(++m_job_seq));

    m_jobs.push_back(std::move(job));
    while (m_jobs.size() > JOB_RING) m_jobs.pop_front();
    m_last_tip_height = parent->nHeight;

    notifyAll(clean);
}

void StratumBridge::pollTip()
{
    if (!m_chainman) return;
    const int h = m_node.getNumBlocks();
    if (h != m_last_tip_height) rebuildJob();
}

bool StratumBridge::processSubmit(Session& s, const QString& job_id_q,
                                  const std::vector<unsigned char>& en2,
                                  uint32_t ntime, uint32_t nonce, QString& err)
{
    if (en2.size() != 4) { err = QStringLiteral("bad extranonce2 size"); return false; }

    const std::string job_id = job_id_q.toStdString();
    const StratumJob* found = nullptr;
    for (auto it = m_jobs.rbegin(); it != m_jobs.rend(); ++it) {
        if (it->job_id == job_id) { found = &*it; break; }
    }
    if (!found) { err = QStringLiteral("stale/unknown job"); return false; }

    // Reconstruct with THIS session's extranonce1 (coinb1/coinb2 are en1-independent).
    StratumJob job = *found;
    job.extranonce1 = s.extranonce1;
    CBlock block;
    try {
        block = job.reconstructBlock(en2, ntime, nonce);
    } catch (const std::exception&) {
        err = QStringLiteral("reconstruct failed");
        return false;
    }

    // Valid share — count it for the hashrate readout (the miner submits at/above
    // our share difficulty).
    m_share_times_ms.push_back(QDateTime::currentMSecsSinceEpoch());

    // Only a share that actually meets the NETWORK target becomes a block, via
    // the same trusted ProcessNewBlock path the in-process miner uses.
    const Consensus::Params& consensus = Params().GetConsensus();
    if (CheckProofOfWork(block.GetPoWHash(), block.nBits, consensus)) {
        std::string hashhex;
        if (m_chainman && MiningUtil::SubmitBlock(*m_chainman, block, hashhex)) {
            ++m_blocks_found;
            Q_EMIT blockFound(QString::fromStdString(hashhex), job.height);
            rebuildJob(); // new tip -> fresh clean job for the miner
        }
    }
    return true;
}

void StratumBridge::estimateHashrate()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!m_share_times_ms.empty() && (now - m_share_times_ms.front()) > HASHRATE_WINDOW_MS) {
        m_share_times_ms.pop_front();
    }
    const double secs = HASHRATE_WINDOW_MS / 1000.0;
    const double shares_per_sec = secs > 0.0 ? m_share_times_ms.size() / secs : 0.0;
    // Each share is ~ (share_difficulty * 2^32) hashes, so H/s ≈ shares/s * that.
    Q_EMIT hashrateChanged(shares_per_sec * m_share_difficulty * HASHES_PER_DIFF1);
}
