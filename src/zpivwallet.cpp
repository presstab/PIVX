#include "wallet.h"

using namespace libzerocoin;

uint256 masterSeed;
uint512 stateSeed;

void SeedToZPiv(uint512 seed, CBigNum& bnSerial, CBigNum& bnRandomness)
{
    uint512 seedState = seed;
    ZerocoinParams* params = Params().Zerocoin_Params();
    while (true)
    {
        //change the seed to the next 'state'
        seedState = Hash512(seedState.begin(), seedState.end());

        //convert state seed into a seed for the serial and one for randomness
        uint256 serialSeed = seedState.trim256();
        LogPrintf("%s : serialSeed=%s\n", serialSeed.GetHex());
        uint256 randomnessSeed = uint512(seedState >> 256).trim256(); //todo check
        LogPrintf("%s : randomnessSeed=%s\n", randomnessSeed.GetHex());

        //hash serial seed
        uint256 hashSerial = Hash(serialSeed.begin(), serialSeed.end());
        CBigNum bnSerialTemp(hashSerial);
        bool fValidSerial = bnSerialTemp < params->coinCommitmentGroup.groupOrder;

        //hash randomness seed
        uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());
        CBigNum bnRandomTemp(hashRandomness);
        bool fValidRandomness = bnRandomTemp < params->coinCommitmentGroup.groupOrder;

        // serial and randomness need to be under a certain value
        if (!fValidSerial || !fValidRandomness)
            continue;

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

PrivateCoin GenerateDeterministicZPiv(CoinDenomination denom)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << masterSeed << stateSeed;
    uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());

    CBigNum bnSerial;
    CBigNum bnRandomness;
    SeedToZPiv(zerocoinSeed, bnSerial, bnRandomness);
    PrivateCoin coin(Params().Zerocoin_Params(), denom, bnSerial, bnRandomness);

    //set to the next seedstate
    ss.clear();
    ss << masterSeed << zerocoinSeed;
    stateSeed = Hash512(ss.begin(), ss.end());

    return coin;
}