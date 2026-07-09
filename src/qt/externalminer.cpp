// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/externalminer.h>

#include <QProcess>
#include <QTimer>

namespace {
constexpr int LOG_RING = 200;      //!< recent output lines retained for the UI
constexpr int STOP_GRACE_MS = 3000; //!< terminate() -> kill() fallback delay
} // namespace

ExternalMiner::ExternalMiner(QObject* parent) : QObject(parent) {}

ExternalMiner::~ExternalMiner()
{
    if (m_proc) {
        // Don't re-enter our slots (and emit) while the object is going away.
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
            m_proc->waitForFinished(1000);
        }
    }
}

bool ExternalMiner::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void ExternalMiner::start(const QString& program, const QStringList& args)
{
    if (isRunning()) return;
    m_stopping = false;
    m_buf.clear();
    m_log.clear();
    ++m_generation; // invalidate any pending hard-kill from a previous stop()

    if (!m_proc) {
        m_proc = new QProcess(this);
        // Miners log to stderr as well as stdout; fold both into one stream.
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_proc, &QProcess::readyReadStandardOutput, this, &ExternalMiner::onReadyRead);
        connect(m_proc, &QProcess::started, this, &ExternalMiner::started);
        connect(m_proc, &QProcess::errorOccurred, this,
                [this](QProcess::ProcessError e) { onErrorOccurred(static_cast<int>(e)); });
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus st) { onFinished(code, static_cast<int>(st)); });
    }

    m_proc->start(program, args);
}

void ExternalMiner::stop()
{
    if (!isRunning()) return;
    m_stopping = true;
    m_proc->terminate();
    // Non-blocking hard-kill fallback. `this` as context cancels the callback if
    // we're destroyed first. m_proc is a single reused QProcess, so a bare
    // capture could kill a miner that stop()+start() restarted within the grace
    // window; gate on the generation so this only kills the SAME run we asked to
    // stop.
    const unsigned gen = m_generation;
    QTimer::singleShot(STOP_GRACE_MS, this, [this, gen]() {
        if (gen == m_generation && m_proc && m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
        }
    });
}

void ExternalMiner::onReadyRead()
{
    if (!m_proc) return;
    m_buf += m_proc->readAllStandardOutput();
    int nl;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        const QString line = QString::fromLocal8Bit(m_buf.left(nl)).trimmed();
        m_buf.remove(0, nl + 1);
        if (!line.isEmpty()) {
            pushLog(line);
            Q_EMIT logLine(line);
        }
    }
}

void ExternalMiner::onErrorOccurred(int process_error)
{
    if (process_error == QProcess::FailedToStart) {
        Q_EMIT failed(tr("Could not launch the miner \xe2\x80\x94 check that the program path is correct."));
    }
    // Other errors (Crashed, etc.) surface via onFinished with a CrashExit status.
}

void ExternalMiner::onFinished(int exit_code, int exit_status)
{
    if (!m_stopping && exit_status == QProcess::CrashExit) {
        Q_EMIT failed(tr("The miner stopped unexpectedly (exit %1).").arg(exit_code));
    }
    m_stopping = false;
    Q_EMIT stopped(exit_code);
}

void ExternalMiner::pushLog(const QString& line)
{
    m_log.append(line);
    while (m_log.size() > LOG_RING) m_log.removeFirst();
}
