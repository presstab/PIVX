// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ACCUMULATORS_H
#define PIVX_ACCUMULATORS_H

#include "libzerocoin/Accumulator.h"
#include "libzerocoin/Coin.h"
#include "libzerocoin/Denominations.h"
#include "primitives/zerocoin.h"
#include "accumulatormap.h"
#include "chain.h"
#include "uint256.h"

class CBlockIndex;

class CoinWitnessData
{
public:
    std::unique_ptr<libzerocoin::PublicCoin> coin;
    std::unique_ptr<libzerocoin::Accumulator> pAccumulator;
    std::unique_ptr<libzerocoin::AccumulatorWitness> pWitness;
    libzerocoin::CoinDenomination denom;
    int nHeightCheckpoint;
    int nHeightMintAdded;
    int nHeightAccStart;
    int nMintsAdded;
    uint256 txid;
    bool isV1;

    CoinWitnessData();
    void SetHeightMintAdded(int nHeight);
  //  CoinWitnessData(CoinWitnessData&);
};

std::map<libzerocoin::CoinDenomination, int> GetMintMaturityHeight();
bool GenerateAccumulatorWitness(std::list<std::unique_ptr<CoinWitnessData> >& listCoinWitness, AccumulatorMap& mapAccumulators, int nSecurityLevel, string& strError, CBlockIndex* pindexCheckpoint);
bool GetAccumulatorValueFromDB(uint256 nCheckpoint, libzerocoin::CoinDenomination denom, CBigNum& bnAccValue);
bool GetAccumulatorValueFromChecksum(uint32_t nChecksum, bool fMemoryOnly, CBigNum& bnAccValue);
void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly);
bool CalculateAccumulatorCheckpoint(int nHeight, uint256& nCheckpoint, AccumulatorMap& mapAccumulators);
void DatabaseChecksums(AccumulatorMap& mapAccumulators);
bool LoadAccumulatorValuesFromDB(const uint256 nCheckpoint);
bool EraseAccumulatorValues(const uint256& nCheckpointErase, const uint256& nCheckpointPrevious);
uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination);
uint32_t GetChecksum(const CBigNum &bnValue);
int GetChecksumHeight(uint32_t nChecksum, libzerocoin::CoinDenomination denomination);
bool InvalidCheckpointRange(int nHeight);
void RandomizeSecurityLevel(int& nSecurityLevel);
bool ValidateAccumulatorCheckpoint(const CBlock& block, CBlockIndex* pindex, AccumulatorMap& mapAccumulators);

#endif //PIVX_ACCUMULATORS_H
