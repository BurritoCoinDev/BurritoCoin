// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/miningtests.h>

#include <chainparams.h>
#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <qt/clientmodel.h>
#include <qt/miningmodel.h>
#include <qt/miningutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/stratumbridge.h>
#include <qt/walletmodel.h>
#include <script/standard.h>
#include <stratumjob.h>
#include <test/util/setup_common.h>
#include <support/allocators/secure.h>
#include <univalue.h>
#include <util/memory.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>
#include <wallet/db.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/wallet.h>

#include <memory>
#include <set>

#include <QElapsedTimer>
#include <QHostAddress>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpSocket>

namespace {

//! How many blocks each test mines. Regtest blocks take a hash or two, so
//! several blocks arrive back to back: the case where rotation could lag.
constexpr int BLOCKS = 8;
constexpr int TIMEOUT_MS = 60000;

//! A regtest chain plus a wallet and the models a Mine tab works with.
struct MiningFixture {
    TestChain100Setup chain;
    std::shared_ptr<CWallet> wallet;
    std::unique_ptr<const PlatformStyle> platform_style{PlatformStyle::instantiate("other")};
    OptionsModel options_model;
    std::unique_ptr<ClientModel> client_model;
    std::unique_ptr<WalletModel> wallet_model;

    //! on_disk: a real wallet file in the test data directory, for tests that
    //! encrypt the wallet (encrypting rewrites the file; the mock has none).
    explicit MiningFixture(interfaces::Node& node, bool on_disk = false)
    {
        node.setContext(&chain.m_node);
        std::unique_ptr<WalletDatabase> database;
        if (on_disk) {
            DatabaseOptions options;
            options.require_create = true;
            DatabaseStatus status;
            bilingual_str error;
            database = MakeDatabase(GetDataDir() / "mining_test_wallet", options, status, error);
        } else {
            database = CreateMockWalletDatabase();
        }
        wallet = std::make_shared<CWallet>(node.context()->chain.get(), "", std::move(database));
        bool first_run;
        wallet->LoadWallet(first_run);
        {
            LOCK(wallet->cs_wallet);
            wallet->SetupLegacyScriptPubKeyMan();
        }
        client_model = MakeUnique<ClientModel>(node, &options_model);
        AddWallet(wallet);
        wallet_model = MakeUnique<WalletModel>(interfaces::MakeWallet(wallet), *client_model, platform_style.get());
        RemoveWallet(wallet, nullopt);
        // No payout address cached from an earlier test.
        QSettings().remove(QStringLiteral("mining"));
    }
};

int ChainHeight()
{
    LOCK(cs_main);
    return ::ChainActive().Height();
}

CScript CoinbasePayout(int height)
{
    const CBlockIndex* index;
    {
        LOCK(cs_main);
        index = ::ChainActive()[height];
    }
    CBlock block;
    if (!index || !ReadBlockFromDisk(block, index, Params().GetConsensus())) return CScript();
    return block.vtx[0]->vout[0].scriptPubKey;
}

//! Every block in [first, last] pays an address this wallet owns, and no two
//! of them pay the same one.
void CheckEachBlockPaysANewWalletAddress(CWallet& wallet, int first, int last)
{
    QVERIFY(last - first + 1 >= BLOCKS);
    std::set<CScript> seen;
    for (int height = first; height <= last; ++height) {
        const CScript payout = CoinbasePayout(height);
        CTxDestination dest;
        QVERIFY(ExtractDestination(payout, dest));
        {
            LOCK(wallet.cs_wallet);
            QVERIFY(wallet.IsMine(dest) != ISMINE_NO);
        }
        QVERIFY2(seen.insert(payout).second, qPrintable(QStringLiteral("block %1 reuses an earlier payout address").arg(height)));
    }
}

void SendLine(QTcpSocket& socket, const std::string& json)
{
    socket.write(json.c_str());
    socket.write("\n");
    socket.flush();
}

} // namespace

void MiningTests::payoutAddressIsReusedOnlyUntilABlockPaysIt()
{
    MiningFixture f(m_node);
    WalletModel* wallet_model = f.wallet_model.get();
    CScript first, second, again;
    QString err;

    // Until a block pays it, every Start mines to the same address.
    QVERIFY(MiningUtil::ResolveCoinbaseScript(wallet_model, first, err));
    QVERIFY(MiningUtil::ResolveCoinbaseScript(wallet_model, again, err));
    QVERIFY(again == first);

    // After a block pays it, the next one is new, and a restart keeps that.
    QVERIFY(MiningUtil::RotateCoinbaseScript(wallet_model, second));
    QVERIFY(second != first);
    QVERIFY(MiningUtil::ResolveCoinbaseScript(wallet_model, again, err));
    QVERIFY(again == second);

    // A wallet that mined with a build before per-block addresses: its cached
    // address was paid by every block, and nothing marks it unused.
    QSettings().remove(QStringLiteral("mining/%1/payout_unused").arg(wallet_model->getWalletName()));
    QVERIFY(MiningUtil::ResolveCoinbaseScript(wallet_model, again, err));
    QVERIFY(again != first && again != second);
}

void MiningTests::lockedWalletOutOfKeysKeepsItsAddress()
{
    // An encrypted, locked wallet can't make keys, only hand out the ones it
    // made in advance. Keep that to one at a time, so it runs out quickly.
    struct RestoreKeypool {
        ~RestoreKeypool() { gArgs.ForceSetArg("-keypool", std::to_string(DEFAULT_KEYPOOL_SIZE)); }
    } restore_keypool;
    gArgs.ForceSetArg("-keypool", "1");
    MiningFixture f(m_node, /*on_disk=*/true);
    WalletModel* wallet_model = f.wallet_model.get();
    CScript current, next, again;
    QString err;

    // Mining started before the wallet was encrypted and locked.
    QVERIFY(MiningUtil::ResolveCoinbaseScript(wallet_model, current, err));
    SecureString passphrase;
    passphrase = "mining test";
    QVERIFY(f.wallet->EncryptWallet(passphrase));
    QVERIFY(f.wallet->IsLocked());

    // Blocks keep paying new addresses until the pre-made keys run out; then
    // rotation keeps the last one...
    int rotations = 0;
    while (MiningUtil::RotateCoinbaseScript(wallet_model, next)) {
        current = next;
        QVERIFY(++rotations < 10);
    }
    // ...and Start Mining still works, on that address, rather than demanding an unlock.
    QVERIFY(MiningUtil::ResolveCoinbaseScript(wallet_model, again, err));
    QVERIFY(again == current);
}

void MiningTests::builtInMinerPaysEachBlockToANewAddress()
{
    MiningFixture f(m_node);
    const int start_height = ChainHeight();

    MiningModel miner(m_node);
    miner.setWalletModel(f.wallet_model.get());
    miner.start(2); // two workers, so blocks can be found back to back by different threads
    QVERIFY(miner.isMining());
    QTRY_VERIFY_WITH_TIMEOUT(ChainHeight() >= start_height + BLOCKS, TIMEOUT_MS);
    miner.stop();

    CheckEachBlockPaysANewWalletAddress(*f.wallet, start_height + 1, ChainHeight());
}

void MiningTests::bridgePaysEachBlockToANewAddress()
{
    MiningFixture f(m_node);
    const int start_height = ChainHeight();

    CScript payout;
    QString err;
    QVERIFY(MiningUtil::ResolveCoinbaseScript(f.wallet_model.get(), payout, err));
    StratumBridge bridge(m_node);
    WalletModel* wallet_model = f.wallet_model.get();
    const quint16 port = bridge.start(payout, 0, [wallet_model](CScript& next) {
        return MiningUtil::RotateCoinbaseScript(wallet_model, next);
    });
    QVERIFY(port != 0);
    QSignalSpy found(&bridge, &StratumBridge::blockFound);

    // A stand-in external miner. It hashes nothing: on regtest about half of
    // all nonces meet the target, and the bridge checks every submitted share,
    // so blindly counting the nonce up finds blocks.
    QTcpSocket miner;
    miner.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(miner.waitForConnected(5000));
    SendLine(miner, R"({"id":1,"method":"mining.subscribe","params":[]})");
    SendLine(miner, R"({"id":2,"method":"mining.authorize","params":["test","x"]})");

    std::string job_id;
    std::string ntime;
    uint32_t nonce = 0;
    int request_id = 3;
    QElapsedTimer timer;
    timer.start();
    while (found.count() < BLOCKS && timer.elapsed() < TIMEOUT_MS) {
        QTest::qWait(5);
        while (miner.canReadLine()) {
            UniValue msg;
            if (!msg.read(miner.readLine().trimmed().toStdString()) || !msg.isObject()) continue;
            const UniValue& method = msg["method"];
            if (method.isStr() && method.get_str() == "mining.notify") {
                job_id = msg["params"][0].get_str();
                ntime = msg["params"][7].get_str();
            }
        }
        if (job_id.empty()) continue;
        UniValue params(UniValue::VARR);
        params.push_back("test");
        params.push_back(job_id);
        params.push_back("00000000");
        params.push_back(ntime);
        params.push_back(stratum::EncodeBE32(nonce++));
        UniValue submit(UniValue::VOBJ);
        submit.pushKV("id", request_id++);
        submit.pushKV("method", "mining.submit");
        submit.pushKV("params", params);
        SendLine(miner, submit.write());
    }
    QCOMPARE(found.count(), BLOCKS);
    miner.disconnectFromHost();
    bridge.stop();

    CheckEachBlockPaysANewWalletAddress(*f.wallet, start_height + 1, ChainHeight());
}
