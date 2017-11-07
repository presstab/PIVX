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
    uint256 seedOut = seedIn;
    seedOut >>= 1;
    seedIn &= 1;
    seedIn <<= 255;
    seedOut |= seedIn;
    return seedOut;
}

bool RotateUntilValid(const CBigNum& bnMax, uint256& hash, int& nRotations)
{
    CBigNum bnValue(hash);
    bool fValidSerial = false;
    nRotations = 0;
    while (!fValidSerial) {
        hash = RightRotate(hash);
        bnValue.setuint256(hash);
        fValidSerial = bnValue < bnMax && bnValue > 0;

        nRotations++;
        if (nRotations >= 255)
            break;
    }

    return fValidSerial;
}

// Check if the value of the commitment meets requirements
bool IsValidCoinValue(const CBigNum& bnValue)
{
    return bnValue >= Params().Zerocoin_Params()->accumulatorParams.minCoinValue &&
    bnValue <= Params().Zerocoin_Params()->accumulatorParams.maxCoinValue &&
    bnValue.isPrime(ZEROCOIN_MINT_PRIME_PARAM);
}

bool CreateValidCommitmentVariable(uint256& hash)
{
    CBigNum bnValue(hash);
    if (bnValue >= Params().Zerocoin_Params()->coinCommitmentGroup.groupOrder) {
        //See if this value can be inexpensively modified to meet the criteria
        int nRotations = 0;
        if (!RotateUntilValid(Params().Zerocoin_Params()->coinCommitmentGroup.groupOrder, hash, nRotations))
            return false;
    }

    return true;
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
        if (!CreateValidCommitmentVariable(hashSerial))
            continue;
        bnSerial.setuint256(hashSerial);

        //hash randomness seed
        uint256 randomnessSeed = uint512(seedStateZerocoin >> 256).trim256();
        uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());

        int nRotations = 0;
        CBigNum commitmentValue;
        while (true) {
            bnRandomness.setuint256(hashRandomness);

            if (bnRandomness >= params->coinCommitmentGroup.groupOrder) {
                int nRotationsInternal = 0;
                RotateUntilValid(params->coinCommitmentGroup.groupOrder, hashRandomness, nRotationsInternal);
                bnRandomness.setuint256(hashRandomness);
            }

            //See if serial and randomness make a valid commitment
            if (bnRandomness < params->coinCommitmentGroup.groupOrder) {
                // Generate a Pedersen commitment to the serial number
                commitmentValue = params->coinCommitmentGroup.g.pow_mod(bnSerial, params->coinCommitmentGroup.modulus).mul_mod(
                        params->coinCommitmentGroup.h.pow_mod(bnRandomness, params->coinCommitmentGroup.modulus),
                        params->coinCommitmentGroup.modulus);

                // Now verify that the commitment is a prime number
                // in the appropriate range. If not, we'll throw this coin
                // away and generate a new one.
                if (IsValidCoinValue(commitmentValue)) {
                    return;
                }
            }

            //Did not create a valid commitment value. Rotate the bits of hashRandomness. If a valid randomness is not created
            //then change the hashRandomness to a new random value.
            int nRotationsInternal = 0;
            bool fValidRotate = RotateUntilValid(params->coinCommitmentGroup.groupOrder, hashRandomness, nRotationsInternal);
            nRotations += nRotationsInternal;
            if (!fValidRotate || nRotations >= 255) {
                if (commitmentValue == 0)
                    break;

                //Change randomness to something new and random and try again
                hashRandomness = commitmentValue.getuint256();
                nRotations = 0;
            }
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