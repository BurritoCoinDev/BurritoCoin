#include <mweb/mweb_wallet.h>
#include <wallet/wallet.h>
#include <wallet/coincontrol.h>
#include <util/bip32.h>

using namespace MWEB;

bool Wallet::UpgradeCoins()
{
    mw::Keychain::Ptr keychain = GetKeychain();
    if (!keychain || !keychain->HasSpendSecret()) {
        return false;
    }

    // Loop through transactions and try upgrading output coins
    for (auto& entry : m_pWallet->mapWallet) {
        CWalletTx* wtx = &entry.second;
        RewindOutputs(*wtx->tx);

        if (wtx->mweb_wtx_info && wtx->mweb_wtx_info->received_coin) {
            mw::Coin& coin = *wtx->mweb_wtx_info->received_coin;
            if (!coin.HasSpendKey()) {
                coin.spend_key = keychain->CalculateOutputKey(coin);

                // If spend key was populated, update the database and m_coins map.
                if (coin.HasSpendKey()) {
                    m_coins[coin.output_id] = coin;

                    WalletBatch batch(m_pWallet->GetDatabase());
                    batch.WriteMWEBCoin(coin);
                    batch.WriteTx(*wtx);
                }
            }
        }
    }

    return true;
}

std::vector<mw::Coin> Wallet::RewindOutputs(const CTransaction& tx)
{
    std::vector<mw::Coin> coins;

    if (tx.HasMWEBTx()) {
        for (const Output& output : tx.mweb_tx.m_transaction->GetOutputs()) {
            mw::Coin mweb_coin;
            if (RewindOutput(output, mweb_coin)) {
                coins.push_back(mweb_coin);
            }
        }
    }

    return coins;
}

bool Wallet::RewindOutput(const Output& output, mw::Coin& coin)
{
    mw::Keychain::Ptr keychain = GetKeychain();

    if (GetCoin(output.GetOutputID(), coin) && coin.IsMine()) {
        // If the coin has the spend key, it's fully rewound.
        // If not, try rewinding further if we have the master spend key (i.e. wallet is unlocked).
        if (coin.HasSpendKey() || !keychain || !keychain->HasSpendSecret()) {
            return true;
        }
    }

    if (!keychain || !keychain->RewindOutput(output, coin)) {
        return false;
    }

    m_coins[coin.output_id] = coin;
    WalletBatch(m_pWallet->GetDatabase()).WriteMWEBCoin(coin);
    return true;
}

bool Wallet::IsChange(const StealthAddress& address) const
{
    StealthAddress change_addr;
    return GetStealthAddress(mw::CHANGE_INDEX, change_addr) && change_addr == address;
}

bool Wallet::GetStealthAddress(const mw::Coin& coin, StealthAddress& address) const
{
    if (coin.HasAddress()) {
        address = *coin.address;
        return true;
    }

    return GetStealthAddress(coin.address_index, address);
}

bool Wallet::GetStealthAddress(const uint32_t index, StealthAddress& address) const
{
    mw::Keychain::Ptr keychain = GetKeychain();
    if (!keychain || index == mw::UNKNOWN_INDEX || index == mw::CUSTOM_KEY) {
        return false;
    }

    if (keychain->GetSpendSecret().IsNull()) {
        return false;
    }

    address = keychain->GetStealthAddress(index);
    return true;
}

void Wallet::LoadToWallet(const mw::Coin& coin)
{
    m_coins[coin.output_id] = coin;
}

void Wallet::SaveToWallet(const std::vector<mw::Coin>& coins)
{
    WalletBatch batch(m_pWallet->GetDatabase());
    for (const mw::Coin& coin : coins) {
        m_coins[coin.output_id] = coin;
        batch.WriteMWEBCoin(coin);
    }
}

bool Wallet::GetCoin(const mw::Hash& output_id, mw::Coin& coin) const
{
    auto iter = m_coins.find(output_id);
    if (iter != m_coins.end()) {
        coin = iter->second;
        return true;
    }

    coin.Reset();
    return false;
}

CAmount Wallet::GetBalance() const
{
    // Collect all MWEB output IDs that have been spent by this wallet.
    std::set<mw::Hash> spent_outputs;
    for (const auto& entry : m_pWallet->mapWallet) {
        const CWalletTx& wtx = entry.second;
        if (wtx.mweb_wtx_info && wtx.mweb_wtx_info->spent_input) {
            spent_outputs.insert(*wtx.mweb_wtx_info->spent_input);
        }
    }

    // Sum amounts of all unspent coins that belong to this wallet.
    CAmount balance = 0;
    for (const auto& entry : m_coins) {
        const mw::Coin& coin = entry.second;
        if (coin.IsMine() && spent_outputs.find(coin.output_id) == spent_outputs.end()) {
            balance += coin.amount;
        }
    }
    return balance;
}

mw::Keychain::Ptr Wallet::GetKeychain() const
{
    auto spk_man = m_pWallet->GetScriptPubKeyMan(OutputType::MWEB, false);
    return spk_man ? spk_man->GetMWEBKeychain() : nullptr;
}