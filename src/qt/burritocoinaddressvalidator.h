// Copyright (c) 2011-2014 The BurritoCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BURRITOCOIN_QT_BURRITOCOINADDRESSVALIDATOR_H
#define BURRITOCOIN_QT_BURRITOCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class BurritoCoinAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BurritoCoinAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

/** BurritoCoin address widget validator, checks for a valid burritocoin address.
 */
class BurritoCoinAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BurritoCoinAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

#endif // BURRITOCOIN_QT_BURRITOCOINADDRESSVALIDATOR_H
