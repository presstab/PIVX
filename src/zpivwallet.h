// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ZPIVWALLET_H
#define PIVX_ZPIVWALLET_H

#include "libzerocoin/Coin.h"
#include "uint256.h"

class CzPIVWallet
{
private:
    uint256 seedMaster;
    uint32_t nCount;
    std::string strWalletFile;
    bool fFirstRun;
    const uint8_t nVersion = 1;

public:
    CzPIVWallet(std::string strWalletFile, bool fFirstRun);
    bool SetMasterSeed(const uint256& seedMaster);
    void SyncWithChain();
    void GenerateDeterministicZPIV(libzerocoin::CoinDenomination denom, libzerocoin::PrivateCoin& coin, bool fGenerateOnly = false);

private:
    uint512 GetNextZerocoinSeed();
    void UpdateCount();
    void SeedToZPiv(uint512 seed, CBigNum& bnSerial, CBigNum& bnRandomness);
};

#endif //PIVX_ZPIVWALLET_H
