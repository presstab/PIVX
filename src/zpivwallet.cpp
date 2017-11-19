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

// Check if the value of the commitment meets requirements
bool IsValidCoinValue(const CBigNum& bnValue)
{
    return bnValue >= Params().Zerocoin_Params()->accumulatorParams.minCoinValue &&
    bnValue <= Params().Zerocoin_Params()->accumulatorParams.maxCoinValue &&
    bnValue.isPrime(ZEROCOIN_MINT_PRIME_PARAM);
}

void CzPIVWallet::SeedToZPiv(uint512 seedZerocoin, CBigNum& bnSerial, CBigNum& bnRandomness, uint256& attempts256)
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
        bnSerial.setuint256(hashSerial);
        bnSerial = bnSerial % params->coinCommitmentGroup.groupOrder;

        //hash randomness seed
        uint256 randomnessSeed = uint512(seedStateZerocoin >> 256).trim256();

        CBigNum commitmentValue;

        uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                                      attempts256.begin(), attempts256.end());
        
        bnRandomness.setuint256(hashRandomness);

        // Iterate on Randomness until a valid commitmentValue is found
        while (true) {
            bnRandomness = bnRandomness % params->coinCommitmentGroup.groupOrder;

            //See if serial and randomness make a valid commitment
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

            //Did not create a valid commitment value.
            //Change randomness to something new and random and try again
            attempts256 = attempts256 + 1;
            hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                                  attempts256.begin(), attempts256.end());
            bnRandomness.setuint256(hashRandomness);
        }
    }
}

bool CzPIVWallet::GenerateDeterministicZPiv(int nNumberOfMints, CoinDenomination denom, std::vector<PrivateCoin>& MintedCoins)
{
    CBigNum bnSerial;
    CBigNum bnRandomness;
    uint256 attempts(0);

    for (int i=0;i<nNumberOfMints;i++) {
        SeedToZPiv(seedState, bnSerial, bnRandomness, attempts);
        PrivateCoin coin = PrivateCoin(Params().Zerocoin_Params(), denom, bnSerial, bnRandomness);
        MintedCoins.push_back(coin);
    }

    return true;
}
