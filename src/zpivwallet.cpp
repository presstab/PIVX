// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpivwallet.h"
#include "main.h"

using namespace libzerocoin;

CzPIVWallet::CzPIVWallet(uint256 seedMaster)
{
    this->seedMaster = seedMaster;
    this->coinNumber = 0;
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
    ZerocoinParams* params = Params().Zerocoin_Params();

    //convert state seed into a seed for the serial and one for randomness
    uint256 serialSeed = seedZerocoin.trim256();
    bnSerial.setuint256(serialSeed);
    bnSerial = bnSerial % params->coinCommitmentGroup.groupOrder;
    
    //std::cout << "Serial # = " << bnSerial.ToString() << "\n";

    //hash randomness seed with Bottom 256 bits of seedZerocoin & attempts256 which is initially 0
    uint256 randomnessSeed = uint512(seedZerocoin >> 256).trim256();
    uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                                  attempts256.begin(), attempts256.end());
    bnRandomness.setuint256(hashRandomness);
    bnRandomness = bnRandomness % params->coinCommitmentGroup.groupOrder;

    //See if serial and randomness make a valid commitment
    // Generate a Pedersen commitment to the serial number
    CBigNum commitmentValue = params->coinCommitmentGroup.g.pow_mod(bnSerial, params->coinCommitmentGroup.modulus).mul_mod(
                        params->coinCommitmentGroup.h.pow_mod(bnRandomness, params->coinCommitmentGroup.modulus),
                        params->coinCommitmentGroup.modulus);

    CBigNum BigNumRandomness;
    // Iterate on Randomness until a valid commitmentValue is found
    while (true) {
        
        // Now verify that the commitment is a prime number
        // in the appropriate range. If not, we'll throw this coin
        // away and generate a new one.
        if (IsValidCoinValue(commitmentValue)) {
            //std::cout << "Attempt # " << attempts256.Get32() << "\n";/// << attempts256.ToString() << ") Done\n";
            return;
        }

        //Did not create a valid commitment value.
        //Change randomness to something new and random and try again
        attempts256++;
        hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                              attempts256.begin(), attempts256.end());
        BigNumRandomness.setuint256(hashRandomness);
        bnRandomness = (bnRandomness + BigNumRandomness) % params->coinCommitmentGroup.groupOrder;
        commitmentValue = commitmentValue.mul_mod(params->coinCommitmentGroup.h.pow_mod(BigNumRandomness, params->coinCommitmentGroup.modulus), params->coinCommitmentGroup.modulus);
    }
}

bool CzPIVWallet::GenerateDeterministicZPiv(int nNumberOfMints, CoinDenomination denom, std::vector<PrivateCoin>& MintedCoins)
{
    CBigNum bnSerial;
    CBigNum bnRandomness;
    uint256 attempts(0);

    // coinNumber is retained between calls be able to continue minting from last value
    for (int i=0;i<nNumberOfMints;i++) {
        uint512 coinSeedState = Hash512(seedState.begin(), seedState.end(), coinNumber.begin(), coinNumber.end());
        coinNumber++;
        attempts = 0;
        SeedToZPiv(coinSeedState, bnSerial, bnRandomness, attempts);
        PrivateCoin coin = PrivateCoin(Params().Zerocoin_Params(), denom, bnSerial, bnRandomness);
        MintedCoins.push_back(coin);
    }

    return true;
}
