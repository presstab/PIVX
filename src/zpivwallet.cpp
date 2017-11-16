// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpivwallet.h"
#include "main.h"

using namespace libzerocoin;

CzPIVWallet::CzPIVWallet(uint256 seedMaster)
{
    this->seedMaster = seedMaster;

    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster;
    seedState = Hash512(ss.begin(), ss.end());
}

uint256 RightRotate(uint256 seedIn)
{
    uint256 mask = 15;
    uint256 seedOut = seedIn;
    seedOut >>= 4;
    seedOut |= ((seedIn & mask) << 252);
    return seedOut;
}

bool RotateUntilValid(CBigNum bnMax, uint256& hash)
{
    CBigNum bnSerialTemp = CBigNum(hash);
    bool fValidSerial = false;
    int nAttempts = 0;
    while (!fValidSerial) {
        hash = RightRotate(hash);
        bnSerialTemp = CBigNum(hash);
        fValidSerial = bnSerialTemp < bnMax && bnSerialTemp > 0;

        nAttempts++;
        if (nAttempts >= 64)
            break;
    }

    return fValidSerial;
}

void CzPIVWallet::SeedToZPiv(uint512 seedZerocoin, CBigNum& bnSerial, CBigNum& bnRandomness)
{
    uint512 seedStateZerocoin = seedZerocoin;
    ZerocoinParams* params = Params().Zerocoin_Params();
    while (true)
    {
        //change the seed to the next 'state'
        seedStateZerocoin = Hash512(seedStateZerocoin.begin(), seedStateZerocoin.end());

        //convert state seed into a seed for the serial and one for randomness
        uint256 serialSeed = seedStateZerocoin.trim256();

        //hash serial seed
        uint256 hashSerial = Hash(serialSeed.begin(), serialSeed.end());
        CBigNum bnSerialTemp = CBigNum(hashSerial);
        bool fValidSerial = bnSerialTemp < params->coinCommitmentGroup.groupOrder;
        if (!fValidSerial) {
            fValidSerial = RotateUntilValid(params->coinCommitmentGroup.groupOrder, hashSerial);
        }

        if (!fValidSerial)
            continue;

        //hash randomness seed
        uint256 randomnessSeed = uint512(seedStateZerocoin >> 256).trim256();
        uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());
        CBigNum bnRandomTemp(hashRandomness);
        bool fValidRandomness = bnRandomTemp < params->coinCommitmentGroup.groupOrder;
        if (!fValidRandomness) {
            fValidRandomness = RotateUntilValid(params->coinCommitmentGroup.groupOrder, hashRandomness);
        }

        // serial and randomness need to be under a certain value
        if (!fValidRandomness)
            continue;

        bnSerialTemp = CBigNum(hashSerial);
        bnRandomness = CBigNum(hashRandomness);
        // Generate a Pedersen commitment to the serial number
        Commitment commitment(&params->coinCommitmentGroup, bnSerialTemp, bnRandomTemp);

        // Now verify that the commitment is a prime number
        // in the appropriate range. If not, we'll throw this coin
        // away and generate a new one.
        if (commitment.getCommitmentValue().isPrime(ZEROCOIN_MINT_PRIME_PARAM) &&
                commitment.getCommitmentValue() >= params->accumulatorParams.minCoinValue &&
                commitment.getCommitmentValue() <= params->accumulatorParams.maxCoinValue) {
            bnSerial = bnSerialTemp;
            bnRandomness = bnRandomTemp;
            return;
        }
    }
}

uint512 CzPIVWallet::GetNextZerocoinSeed()
{
    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster << seedState;
    uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());
    return zerocoinSeed;
}

void CzPIVWallet::UpdateState(uint512 seedZerocoin)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster << seedZerocoin;
    seedState = Hash512(ss.begin(), ss.end());
}

bool CzPIVWallet::GenerateDeterministicZPiv(CoinDenomination denom, PrivateCoin& coin)
{
    uint512 seedZerocoin = GetNextZerocoinSeed();

    CBigNum bnSerial;
    CBigNum bnRandomness;
    SeedToZPiv(seedZerocoin, bnSerial, bnRandomness);
    coin = PrivateCoin(Params().Zerocoin_Params(), denom, bnSerial, bnRandomness);

    //set to the next seedstate
    UpdateState(seedZerocoin);

    return true;
}