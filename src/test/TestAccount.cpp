// Copyright 2016 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "TestAccount.h"

#include "ledger/DataFrame.h"
#include "lib/catch.hpp"
#include "main/Application.h"
#include "test/TestExceptions.h"
#include "test/TxTests.h"

namespace stellar
{

using namespace txtest;

SequenceNumber
TestAccount::loadSequenceNumber() const
{
    return loadAccount(getPublicKey(), mApp)->getSeqNum();
}

void
TestAccount::updateSequenceNumber()
{
    if (mSn == 0 && loadAccount(getPublicKey(), mApp, false))
    {
        mSn = loadSequenceNumber();
    }
}

int64_t
TestAccount::getBalance() const
{
    return loadAccount(getPublicKey(), mApp)->getBalance();
}

TransactionFramePtr
TestAccount::tx(std::vector<Operation> const& ops, SequenceNumber sn)
{
    if (sn == 0)
    {
        sn = nextSequenceNumber();
    }

    return transactionFromOperations(mApp.getNetworkID(), getSecretKey(),
                                     sn, ops);
}

Operation
TestAccount::op(Operation operation)
{
    operation.sourceAccount.activate() = getPublicKey();
    return operation;
}

TestAccount
TestAccount::createRoot(Application& app)
{
    auto secretKey = getRoot(app.getNetworkID());
    return TestAccount{app, secretKey};
}

TestAccount
TestAccount::create(SecretKey const& secretKey, uint64_t initialBalance)
{
    auto toCreate = loadAccount(secretKey.getPublicKey(), mApp, false);
    auto self = loadAccount(getSecretKey().getPublicKey(), mApp);

    try
    {
        applyTx(tx({createCreateAccountOp(secretKey.getPublicKey(), initialBalance)}), mApp);
    }
    catch (...)
    {
        auto toCreateAfter = loadAccount(secretKey.getPublicKey(), mApp, false);
        // check that the target account didn't change
        REQUIRE(!!toCreate == !!toCreateAfter);
        if (toCreate && toCreateAfter)
        {
            REQUIRE(toCreate->getAccount() == toCreateAfter->getAccount());
        }
        throw;
    }

    REQUIRE(loadAccount(secretKey.getPublicKey(), mApp));
    return TestAccount{mApp, secretKey};
}

TestAccount
TestAccount::create(std::string const& name, uint64_t initialBalance)
{
    return create(getAccount(name.c_str()), initialBalance);
}

void
TestAccount::merge(PublicKey const& into)
{
    applyTx(tx({createMergeOp(into)}), mApp);

    REQUIRE(loadAccount(into, mApp));
    REQUIRE(!loadAccount(getPublicKey(), mApp, false));
}

void
TestAccount::inflation()
{
    applyTx(tx({createInflationOp()}), mApp);
}

void
TestAccount::changeTrust(Asset const& asset, int64_t limit)
{
    applyTx(tx({createChangeTrustOp(asset, limit)}), mApp);
}

void
TestAccount::allowTrust(Asset const& asset, PublicKey const& trustor)
{
    applyTx(tx({createAllowTrustOp(trustor, asset, true)}), mApp);
}

void
TestAccount::denyTrust(Asset const& asset, PublicKey const& trustor)
{
    applyTx(tx({createAllowTrustOp(trustor, asset, false)}), mApp);
}

void
TestAccount::setOptions(AccountID* inflationDest, uint32_t* setFlags,
                        uint32_t* clearFlags, ThresholdSetter* thrs,
                        Signer* signer, std::string* homeDomain)
{
    applyTx(tx({createSetOptionsOp(inflationDest, setFlags, clearFlags, thrs,
                                   signer, homeDomain)}), mApp);
}

void
TestAccount::manageData(std::string const& name, DataValue* value)
{
    applyTx(tx({createManageDataOp(name, value)}), mApp);

    auto dataFrame =
        DataFrame::loadData(getPublicKey(), name, mApp.getDatabase());
    if (value)
    {
        REQUIRE(dataFrame != nullptr);
        REQUIRE(dataFrame->getData().dataValue == *value);
    }
    else
    {
        REQUIRE(dataFrame == nullptr);
    }
}

OfferFrame::pointer
TestAccount::loadOffer(uint64_t offerID) const
{
    return txtest::loadOffer(getPublicKey(), offerID, mApp, true);
}

bool
TestAccount::hasOffer(uint64_t offerID) const
{
    return !!txtest::loadOffer(getPublicKey(), offerID, mApp, false);
}

uint64_t
TestAccount::manageOffer(uint64_t offerID, Asset const& selling,
                         Asset const& buying, Price const& price,
                         int64_t amount, ManageOfferEffect expectedEffect)
{
    return applyManageOffer(mApp, offerID, getSecretKey(), selling, buying,
                            price, amount, nextSequenceNumber(),
                            expectedEffect);
}

uint64_t
TestAccount::createPassiveOffer(Asset const& selling, Asset const& buying,
                                Price const& price, int64_t amount,
                                ManageOfferEffect expectedEffect)
{
    return applyCreatePassiveOffer(mApp, getSecretKey(), selling, buying, price,
                                   amount, nextSequenceNumber(),
                                   expectedEffect);
}

void
TestAccount::pay(PublicKey const& destination, int64_t amount)
{
    applyPaymentTx(mApp, getSecretKey(), destination, nextSequenceNumber(),
                   amount);
}

void
TestAccount::pay(PublicKey const& destination, Asset const& selling,
                 int64_t amount)
{
    applyCreditPaymentTx(mApp, getSecretKey(), destination, selling,
                         nextSequenceNumber(), amount);
}

PathPaymentResult
TestAccount::pay(PublicKey const& destination, Asset const& sendCur,
                 int64_t sendMax, Asset const& destCur, int64_t destAmount,
                 std::vector<Asset> const& path, Asset* noIssuer)
{
    auto transaction = tx({createPathPaymentOp(destination, sendCur, sendMax,
                                               destCur, destAmount, path)});
    try
    {
        applyTx(transaction, mApp);
    }
    catch (ex_PATH_PAYMENT_NO_ISSUER &)
    {
        REQUIRE(noIssuer);
        REQUIRE(*noIssuer == transaction->getResult().result.results()[0].tr().pathPaymentResult().noIssuer());
        throw;
    }

    REQUIRE(!noIssuer);

    return getFirstResult(*transaction).tr().pathPaymentResult();
}
};
