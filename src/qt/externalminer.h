// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_EXTERNALMINER_H
#define BURRITOCOIN_QT_EXTERNALMINER_H

#include <QObject>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

/**
 * Supervises an external Stratum miner (cpuminer-opt for tuned CPU, or
 * ccminer/sgminer for GPU) as a child process pointed at the in-wallet
 * StratumBridge on localhost.
 *
 * Deliberately thin: the bridge already produces work, validates shares,
 * submits blocks, and estimates hashrate — so this class only launches the
 * miner, tails its output for a log/first-error surface, and reports when it
 * starts, dies, or fails to launch. It is miner-agnostic; the caller builds the
 * argument list for the specific miner (algo, -o stratum URL, worker, etc.),
 * which keeps cpuminer/ccminer/sgminer CLI differences out of here.
 *
 * The miner is an arms-length, separate process communicating only over the
 * localhost Stratum socket (never linked in), so a GPLv2 miner does not entangle
 * the MIT wallet.
 */
class ExternalMiner : public QObject
{
    Q_OBJECT

public:
    explicit ExternalMiner(QObject* parent = nullptr);
    ~ExternalMiner();

    //! Launch `program` with `args` (the caller has already composed the algo,
    //! the stratum+tcp://127.0.0.1:<port> URL, and worker/password). No-op if a
    //! miner is already running.
    void start(const QString& program, const QStringList& args);

    //! Ask the miner to terminate (SIGTERM/close), then kill after a grace period.
    void stop();

    bool isRunning() const;

    //! The most recent lines of miner output, oldest-first (for a small log view).
    QStringList recentLog() const { return m_log; }

Q_SIGNALS:
    void started();
    void stopped(int exit_code);        //!< clean or crash exit; UI reverts to idle
    void logLine(const QString& line);  //!< one parsed line of miner stdout/stderr
    void failed(const QString& message);//!< could not launch (missing exe, etc.)

private Q_SLOTS:
    void onReadyRead();
    void onErrorOccurred(int process_error); //!< QProcess::ProcessError as int
    void onFinished(int exit_code, int exit_status);

private:
    void pushLog(const QString& line);

    QProcess* m_proc = nullptr;
    QByteArray m_buf;      //!< partial-line accumulator for stdout
    QStringList m_log;     //!< bounded ring of recent lines
    bool m_stopping = false;
    //! Incremented on every start(); a stop()'s delayed hard-kill only fires if
    //! this hasn't advanced since, so it can't kill a miner that was restarted
    //! within the grace window (m_proc is a single reused QProcess).
    unsigned m_generation = 0;
};

#endif // BURRITOCOIN_QT_EXTERNALMINER_H
