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
    uint512 seedState;

public:
    explicit CzPIVWallet(uint256 seedMaster);
    bool GenerateDeterministicZPiv(libzerocoin::CoinDenomination denom, libzerocoin::PrivateCoin& coin);

private:
    uint512 GetNextZerocoinSeed();
    void UpdateState(uint512 seedZerocoin);
    void SeedToZPiv(uint512 seed, CBigNum& bnSerial, CBigNum& bnRandomness);
};

#endif //PIVX_ZPIVWALLET_H
