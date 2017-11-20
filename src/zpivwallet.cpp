// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpivwallet.h"
#include "main.h"
#include "txdb.h"
#include "walletdb.h"

using namespace libzerocoin;


CzPIVWallet::CzPIVWallet(std::string strWalletFile, bool fFirstRun)
{
    this->strWalletFile = strWalletFile;
    CWalletDB walletdb(strWalletFile);

    uint256 seed;
    if (!walletdb.ReadZPIVSeed(seed))
        fFirstRun = true;

    //First time running, generate master seed
    if (fFirstRun) {
        seedMaster = CBigNum::randBignum(CBigNum(~uint256(0))).getuint256();
        walletdb.WriteZPIVSeed(seedMaster);
        nCount = 0;
        return;
    }

    SetMasterSeed(seed);
}

bool CzPIVWallet::SetMasterSeed(const uint256& seedMaster)
{
    this->seedMaster = seedMaster;

    if (!CWalletDB(strWalletFile).WriteZPIVSeed(seedMaster))
        return false;

    if (!CWalletDB(strWalletFile).ReadZPIVCount(nCount))
        nCount = 0;

    //TODO remove this leak of seed from logs before merge to master
    LogPrintf("%s : seed=%s count=%d\n", __func__, seedMaster.GetHex().substr(0, 4), nCount);

    return true;
}

//Catch the counter up with the chain
void CzPIVWallet::SyncWithChain()
{
    //Create the next mint and see if the commitment value is on the chain
    while (true) {
        PrivateCoin coin(Params().Zerocoin_Params(), CoinDenomination::ZQ_ONE, false);
        GenerateDeterministicZPIV(CoinDenomination::ZQ_ONE, coin, true);

        uint256 txHash;
        if (zerocoinDB->ReadCoinMint(coin.getPublicCoin().getValue(), txHash)) {
            //this mint has already occured on the chain, increment counter's state to reflect this
            LogPrintf("%s : Found used coin mint %s \n", __func__, coin.getPublicCoin().getValue().GetHex());

            uint256 hashBlock;
            CTransaction tx;
            if (!GetTransaction(txHash, tx, hashBlock)) {
                LogPrintf("%s : failed to get transaction for mint %s!\n", __func__, coin.getPublicCoin().getValue().GetHex());
                continue;
            }

            //Find the denomination
            CoinDenomination denomination = CoinDenomination::ZQ_ERROR;
            for (const CTxOut out : tx.vout) {
                if (!out.scriptPubKey.IsZerocoinMint())
                    continue;

                PublicCoin pubcoin(Params().Zerocoin_Params());
                CValidationState state;
                if (!TxOutToPublicCoin(out, pubcoin, state)) {
                    LogPrintf("%s : failed to get mint from txout for %s!\n", __func__, coin.getPublicCoin().getValue().GetHex());
                    continue;
                }
                denomination = pubcoin.getDenomination();
            }

            if (denomination == ZQ_ERROR) {
                LogPrintf("%s : failed to get mint %s from tx %s!\n", __func__, coin.getPublicCoin().getValue(), tx.GetHash().GetHex());
                continue;
            }

            CZerocoinMint mint(denomination, coin.getPublicCoin().getValue(), coin.getRandomness(), coin.getSerialNumber(), false);
            int nHeight = 0;
            if (mapBlockIndex.count(hashBlock))
                nHeight = mapBlockIndex.at(hashBlock)->nHeight;

            AddMint(mint, txHash, nHeight);
            UpdateCount();
        } else {
            bnNextMintValue = coin.getPublicCoin().getValue();
            break;
        }
    }
}

bool CzPIVWallet::AddMint(CZerocoinMint mint, uint256 txHash, int nHeight)
{
    mint.SetTxHash(txHash);
    mint.SetHeight(nHeight);

    if (mint.GetSerialNumber() == 0) {
        CBigNum bnSerial;
        CBigNum bnRandomness;
        SeedToZPIV(GetNextZerocoinSeed(), bnSerial, bnRandomness);
        mint.SetSerialNumber(bnSerial);
        mint.SetRandomness(bnRandomness);
        UpdateCount();
    }

    if (!CWalletDB(strWalletFile).WriteZerocoinMint(mint)) {
        LogPrintf("%s : failed to database mint %s!\n", __func__, mint.GetValue().GetHex());
        return false;
    }

    return true;
}

// Check if the value of the commitment meets requirements
bool IsValidCoinValue(const CBigNum& bnValue)
{
    return bnValue >= Params().Zerocoin_Params()->accumulatorParams.minCoinValue &&
    bnValue <= Params().Zerocoin_Params()->accumulatorParams.maxCoinValue &&
    bnValue.isPrime(ZEROCOIN_MINT_PRIME_PARAM);
}

void CzPIVWallet::SeedToZPIV(uint512 seedZerocoin, CBigNum& bnSerial, CBigNum& bnRandomness)
{
    ZerocoinParams* params = Params().Zerocoin_Params();

    //convert state seed into a seed for the serial and one for randomness
    uint256 serialSeed = seedZerocoin.trim256();
    bnSerial.setuint256(serialSeed);
    bnSerial = bnSerial % params->coinCommitmentGroup.groupOrder;

    //std::cout << "Serial # = " << bnSerial.ToString() << "\n";

    //hash randomness seed with Bottom 256 bits of seedZerocoin & attempts256 which is initially 0
    uint256 randomnessSeed = uint512(seedZerocoin >> 256).trim256();
    uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());
    bnRandomness.setuint256(hashRandomness);
    bnRandomness = bnRandomness % params->coinCommitmentGroup.groupOrder;

    //See if serial and randomness make a valid commitment
    // Generate a Pedersen commitment to the serial number
    CBigNum commitmentValue = params->coinCommitmentGroup.g.pow_mod(bnSerial, params->coinCommitmentGroup.modulus).mul_mod(
                        params->coinCommitmentGroup.h.pow_mod(bnRandomness, params->coinCommitmentGroup.modulus),
                        params->coinCommitmentGroup.modulus);

    CBigNum BigNumRandomness;
    uint256 attempts256 = 0;
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

uint512 CzPIVWallet::GetNextZerocoinSeed()
{
    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster << nCount;
    uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());
    return zerocoinSeed;
}

void CzPIVWallet::UpdateCount()
{
    nCount++;
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteZPIVCount(nCount);
}

void CzPIVWallet::GenerateDeterministicZPIV(CoinDenomination denom, PrivateCoin& coin, bool fGenerateOnly)
{
    uint512 seedZerocoin = GetNextZerocoinSeed();

    //TODO remove this leak of seed from logs before merge to master
    if (!fGenerateOnly)
        LogPrintf("%s : Generated new deterministic mint. Count=%d seed=%s\n", __func__, nCount, seedZerocoin.GetHex().substr(0, 4));

    CBigNum bnSerial;
    CBigNum bnRandomness;
    SeedToZPIV(seedZerocoin, bnSerial, bnRandomness);
    coin = PrivateCoin(Params().Zerocoin_Params(), denom, bnSerial, bnRandomness);

    if (fGenerateOnly)
        return;

    //set to the next count
    UpdateCount();
}

bool CzPIVWallet::IsNextMint(const CBigNum& bnValue)
{
    return bnValue == bnNextMintValue;
}