// Copyright (c) 2026 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_TEST_MININGTESTS_H
#define BURRITOCOIN_QT_TEST_MININGTESTS_H

#include <QObject>
#include <QTest>

namespace interfaces {
class Node;
} // namespace interfaces

class MiningTests : public QObject
{
 public:
    explicit MiningTests(interfaces::Node& node) : m_node(node) {}
    interfaces::Node& m_node;

    Q_OBJECT

private Q_SLOTS:
    void payoutAddressIsReusedOnlyUntilABlockPaysIt();
    void lockedWalletOutOfKeysKeepsItsAddress();
    void noPassphrasePromptWhenUnlockingCannotHelp();
    void closingTheMinePageWhileMiningDoesNotCrash();
    void builtInMinerPaysEachBlockToANewAddress();
    void bridgePaysEachBlockToANewAddress();
};

#endif // BURRITOCOIN_QT_TEST_MININGTESTS_H
