// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "chain.h"
#include "primitives/zerocoin.h"
#include "main.h"
#include "stakeinput.h"
#include "wallet.h"

// The zPIV block index is the first appearance of the accumulator checksum that was used in the spend
// note that this also means when staking that this checksum should be from a block that is beyond 60 minutes old and
// 100 blocks deep.
CBlockIndex* CZPivStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;


    if (fMint) {
        int nHeightChecksum = chainActive.Height() - 120;
        nHeightChecksum -= (nHeightChecksum % 10);
        pindexFrom = chainActive[nHeightChecksum];
        return pindexFrom;
    }

    uint32_t nChecksum = spend->getAccumulatorChecksum();
    int nHeightChecksum = GetChecksumHeight(nChecksum, spend->getDenomination());
    if (nHeightChecksum < Params().Zerocoin_StartHeight()) {
        pindexFrom = nullptr;
    } else {
        //note that this will be a nullptr if the height DNE
        pindexFrom = chainActive[nHeightChecksum];
    }

    return pindexFrom;
}

CAmount CZPivStake::GetValue()
{
    return denom * COIN;
}

//Use the first accumulator checkpoint that occurs 60 minutes after the block being staked from
bool CZPivStake::GetModifier(uint64_t& nStakeModifier)
{
    CBlockIndex* pindex = GetIndexFrom();
    if (!pindex)
        return false;

    int64_t nTimeBlockFrom = pindex->GetBlockTime();
    while (true) {
        if (pindex->GetBlockTime() - nTimeBlockFrom > 60*60) {
            nStakeModifier = pindex->nAccumulatorCheckpoint.Get64();
            return true;
        }

        if (pindex->nHeight + 1 <= chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            return false;
    }
}

CDataStream CZPivStake::GetUniqueness()
{
    //LogPrintf("%s serial=%s\n", __func__, bnSerial.GetHex());
    //The unique identifier for a ZPIV is the serial
    CDataStream ss(SER_GETHASH, 0);
    ss << bnSerial;
    return ss;
}

bool CZPivStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    LogPrintf("%s\n", __func__);
    CZerocoinSpendReceipt receipt;
    int nSecurityLevel = 5;
    if (!pwallet->MintToTxIn(mint, nSecurityLevel, hashTxOut, txIn, receipt))
        return error("%s\n", receipt.GetStatusMessage());

    return true;
}

bool CZPivStake::CreateTxOut(CWallet* pwallet, CTxOut& out)
{
    LogPrintf("%s\n", __func__);
    //todo: add mints here
    CPubKey pubkey = pwallet->GenerateNewKey();
    CScript scriptPubKey = GetScriptForDestination(pubkey.GetID());
    out = CTxOut(0, scriptPubKey);

    return true;
}

bool CZPivStake::GetTxFrom(CTransaction& tx)
{
    LogPrintf("%s\n", __func__);
    return false;
}


//!PIV Stake
bool CPivStake::SetInput(CTransaction txPrev, unsigned int n)
{
    this->txFrom = txPrev;
    this->nPosition = n;
    return true;
}

bool CPivStake::GetTxFrom(CTransaction& tx)
{
    tx = txFrom;
    return true;
}

bool CPivStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    txIn = CTxIn(txFrom.GetHash(), nPosition);
    return true;
}

CAmount CPivStake::GetValue()
{
    return txFrom.vout[nPosition].nValue;
}

bool CPivStake::CreateTxOut(CWallet* pwallet, CTxOut& out)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = txFrom.vout[nPosition].scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions)) {
        LogPrintf("CreateCoinStake : failed to parse kernel\n");
        return false;
    }

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
        return false; // only support pay to public key and pay to address

    CScript scriptPubKey;
    if (whichType == TX_PUBKEYHASH) // pay to address type
    {
        //convert to pay to public key type
        CKey key;
        if (!pwallet->GetKey(uint160(vSolutions[0]), key))
            return false;

        scriptPubKey << key.GetPubKey() << OP_CHECKSIG;
    } else
        scriptPubKey = scriptPubKeyKernel;

    out = CTxOut(0, scriptPubKey);
    return true;
}

bool CPivStake::GetModifier(uint64_t& nStakeModifier)
{
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;
    GetIndexFrom();
    if (!pindexFrom)
        return error("%s: failed to get index from", __func__);

    if (!GetKernelStakeModifier(pindexFrom->GetBlockHash(), nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false))
        return error("CheckStakeKernelHash(): failed to get kernel stake modifier \n");

    return true;
}

CDataStream CPivStake::GetUniqueness()
{
    //The unique identifier for a PIV stake is the outpoint
    CDataStream ss(SER_NETWORK, 0);
    ss << nPosition << txFrom.GetHash();
    return ss;
}

//The block that the UTXO was added to the chain
CBlockIndex* CPivStake::GetIndexFrom()
{
    uint256 hashBlock = 0;
    CTransaction tx;
    if (GetTransaction(txFrom.GetHash(), tx, hashBlock, true)) {
        // If the index is in the chain, then set it as the "index from"
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            if (chainActive.Contains(pindex))
                pindexFrom = pindex;
        }
    } else {
        LogPrintf("%s : failed to find tx %s\n", __func__, txFrom.GetHash().GetHex());
    }

    return pindexFrom;
}