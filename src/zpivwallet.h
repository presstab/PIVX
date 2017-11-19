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
    uint256 coinNumber;

public:
    explicit CzPIVWallet(uint256 seedMaster);
    bool GenerateDeterministicZPiv(int nNumberOfMintsNeeded, libzerocoin::CoinDenomination denom, std::vector<libzerocoin::PrivateCoin>& coins);

private:
    void SeedToZPiv(uint512 seed, CBigNum& bnSerial, CBigNum& bnRandomness, uint256& attempts256);
};

#endif //PIVX_ZPIVWALLET_H
