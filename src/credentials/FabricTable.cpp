/*
 *
 *    Copyright (c) 2021-2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 * @brief Defines a table of fabrics that have provisioned the device.
 */

#include "FabricTable.h"

#include <lib/core/CHIPEncoding.h>
#include <lib/support/BufferWriter.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/SafeInt.h>
#include <lib/support/ScopedBuffer.h>
#if CHIP_CRYPTO_HSM
#include <crypto/hsm/CHIPCryptoPALHsm.h>
#endif

namespace chip {
using namespace Credentials;
using namespace Crypto;

using CertChainElement = chip::Credentials::OperationalCertificateStore::CertChainElement;

namespace {

static_assert(kMinValidFabricIndex <= CHIP_CONFIG_MAX_FABRICS, "Must support some fabrics.");
static_assert(CHIP_CONFIG_MAX_FABRICS <= kMaxValidFabricIndex, "Max fabric count out of range.");

// Tags for our metadata storage.
constexpr TLV::Tag kVendorIdTag    = TLV::ContextTag(0);
constexpr TLV::Tag kFabricLabelTag = TLV::ContextTag(1);

// Tags for our index list storage.
constexpr TLV::Tag kNextAvailableFabricIndexTag = TLV::ContextTag(0);
constexpr TLV::Tag kFabricIndicesTag            = TLV::ContextTag(1);

} // anonymous namespace

CHIP_ERROR FabricInfo::Init(const FabricInfo::InitParams & initParams)
{
    ReturnErrorOnFailure(initParams.AreValid());

    Reset();

    mNodeId             = initParams.nodeId;
    mFabricId           = initParams.fabricId;
    mFabricIndex        = initParams.fabricIndex;
    mCompressedFabricId = initParams.compressedFabricId;
    mRootPublicKey      = initParams.rootPublicKey;
    mVendorId           = initParams.vendorId;

    // Deal with externally injected keys
    if (initParams.operationalKeypair != nullptr)
    {
        if (initParams.hasExternallyOwnedKeypair)
        {
            ReturnErrorOnFailure(SetExternallyOwnedOperationalKeypair(initParams.operationalKeypair));
        }
        else
        {
            ReturnErrorOnFailure(SetOperationalKeypair(initParams.operationalKeypair));
        }
    }

    return CHIP_NO_ERROR;
}

void FabricInfo::operator=(FabricInfo && other)
{
    Reset();

    mNodeId             = other.mNodeId;
    mFabricId           = other.mFabricId;
    mFabricIndex        = other.mFabricIndex;
    mCompressedFabricId = other.mCompressedFabricId;
    mRootPublicKey      = other.mRootPublicKey;
    mVendorId           = other.mVendorId;

    SetFabricLabel(other.GetFabricLabel());

    if (other.mOperationalKey != nullptr)
    {
        if (other.mHasExternallyOwnedOperationalKey)
        {
            VerifyOrDie(SetExternallyOwnedOperationalKeypair(other.mOperationalKey) == CHIP_NO_ERROR);
        }
        else
        {
            VerifyOrDie(SetOperationalKeypair(other.mOperationalKey) == CHIP_NO_ERROR);
        }
    }

    other.Reset();
}

CHIP_ERROR FabricInfo::CommitToStorage(PersistentStorageDelegate * storage) const
{
    DefaultStorageKeyAllocator keyAlloc;

    {
        uint8_t buf[MetadataTLVMaxSize()];
        TLV::TLVWriter writer;
        writer.Init(buf);

        TLV::TLVType outerType;
        ReturnErrorOnFailure(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outerType));

        ReturnErrorOnFailure(writer.Put(kVendorIdTag, mVendorId));

        ReturnErrorOnFailure(writer.PutString(kFabricLabelTag, CharSpan::fromCharString(mFabricLabel)));

        ReturnErrorOnFailure(writer.EndContainer(outerType));

        const auto metadataLength = writer.GetLengthWritten();
        VerifyOrReturnError(CanCastTo<uint16_t>(metadataLength), CHIP_ERROR_BUFFER_TOO_SMALL);
        ReturnErrorOnFailure(
            storage->SyncSetKeyValue(keyAlloc.FabricMetadata(mFabricIndex), buf, static_cast<uint16_t>(metadataLength)));
    }

    // NOTE: Operational Key is never saved to storage here. See OperationalKeystore interface for how it is accessed

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricInfo::LoadFromStorage(PersistentStorageDelegate * storage, FabricIndex newFabricIndex, const ByteSpan & rcac,
                                       const ByteSpan & noc)
{
    DefaultStorageKeyAllocator keyAlloc;

    mFabricIndex = newFabricIndex;

    // Regenerate operational metadata from NOC/RCAC
    {
        ReturnErrorOnFailure(ExtractNodeIdFabricIdFromOpCert(noc, &mNodeId, &mFabricId));

        P256PublicKeySpan rootPubKeySpan;
        ReturnErrorOnFailure(ExtractPublicKeyFromChipCert(rcac, rootPubKeySpan));
        mRootPublicKey = rootPubKeySpan;

        uint8_t compressedFabricIdBuf[sizeof(uint64_t)];
        MutableByteSpan compressedFabricIdSpan(compressedFabricIdBuf);
        ReturnErrorOnFailure(GenerateCompressedFabricId(mRootPublicKey, mFabricId, compressedFabricIdSpan));

        // Decode compressed fabric ID accounting for endianness, as GenerateCompressedFabricId()
        // returns a binary buffer and is agnostic of usage of the output as an integer type.
        mCompressedFabricId = Encoding::BigEndian::Get64(compressedFabricIdBuf);
    }

    // Load other storable metadata (label, vendorId, etc)
    {
        uint8_t buf[MetadataTLVMaxSize()];
        uint16_t size = sizeof(buf);
        ReturnErrorOnFailure(storage->SyncGetKeyValue(keyAlloc.FabricMetadata(mFabricIndex), buf, size));
        TLV::ContiguousBufferTLVReader reader;
        reader.Init(buf, size);

        ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Structure, TLV::AnonymousTag()));
        TLV::TLVType containerType;
        ReturnErrorOnFailure(reader.EnterContainer(containerType));

        ReturnErrorOnFailure(reader.Next(kVendorIdTag));
        ReturnErrorOnFailure(reader.Get(mVendorId));

        ReturnErrorOnFailure(reader.Next(kFabricLabelTag));
        CharSpan label;
        ReturnErrorOnFailure(reader.Get(label));

        VerifyOrReturnError(label.size() <= kFabricLabelMaxLengthInBytes, CHIP_ERROR_BUFFER_TOO_SMALL);
        Platform::CopyString(mFabricLabel, label);

        ReturnErrorOnFailure(reader.ExitContainer(containerType));
        ReturnErrorOnFailure(reader.VerifyEndOfContainer());
    }

    // NOTE: Operational Key is never loaded here. See OperationalKeystore interface for how it is accessed

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricInfo::SetFabricLabel(const CharSpan & fabricLabel)
{
    Platform::CopyString(mFabricLabel, fabricLabel);

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::DeleteMetadataFromStorage(FabricIndex fabricIndex)
{
    VerifyOrReturnError(IsValidFabricIndex(fabricIndex), CHIP_ERROR_INVALID_FABRIC_INDEX);
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INCORRECT_STATE);

    DefaultStorageKeyAllocator keyAlloc;
    CHIP_ERROR deleteErr = mStorage->SyncDeleteKeyValue(keyAlloc.FabricMetadata(fabricIndex));

    if (deleteErr != CHIP_NO_ERROR)
    {
        ChipLogError(FabricProvisioning, "Error deleting part of fabric %d: %" CHIP_ERROR_FORMAT, fabricIndex, deleteErr.Format());
    }
    return deleteErr;
}

CHIP_ERROR FabricInfo::SetOperationalKeypair(const P256Keypair * keyPair)
{
    VerifyOrReturnError(keyPair != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    P256SerializedKeypair serialized;
    ReturnErrorOnFailure(keyPair->Serialize(serialized));

    if (mHasExternallyOwnedOperationalKey)
    {
        // Drop it, so we will allocate an internally owned one.
        mOperationalKey                   = nullptr;
        mHasExternallyOwnedOperationalKey = false;
    }

    if (mOperationalKey == nullptr)
    {
#ifdef ENABLE_HSM_CASE_OPS_KEY
        mOperationalKey = chip::Platform::New<P256KeypairHSM>();
#else
        mOperationalKey = chip::Platform::New<P256Keypair>();
#endif
    }
    VerifyOrReturnError(mOperationalKey != nullptr, CHIP_ERROR_NO_MEMORY);
    return mOperationalKey->Deserialize(serialized);
}

CHIP_ERROR FabricInfo::SetExternallyOwnedOperationalKeypair(P256Keypair * keyPair)
{
    VerifyOrReturnError(keyPair != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    if (!mHasExternallyOwnedOperationalKey && mOperationalKey != nullptr)
    {
        chip::Platform::Delete(mOperationalKey);
        mOperationalKey = nullptr;
    }

    mHasExternallyOwnedOperationalKey = true;
    mOperationalKey                   = keyPair;
    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::ValidateIncomingNOCChain(const ByteSpan & noc, const ByteSpan & icac, const ByteSpan & rcac,
                                                 FabricId existingFabricId, Credentials::CertificateValidityPolicy * policy,
                                                 CompressedFabricId & outCompressedFabricId, FabricId & outFabricId,
                                                 NodeId & outNodeId, Crypto::P256PublicKey & outNocPubkey) const
{
    Credentials::ValidationContext validContext;

    // Note that we do NOT set a time in the validation context.  This will
    // cause the certificate chain NotBefore / NotAfter time validation logic
    // to report CertificateValidityResult::kTimeUnknown.
    //
    // The default CHIPCert policy passes NotBefore / NotAfter validation for
    // this case where time is unknown.  If an override policy is passed, it
    // will be up to the passed policy to decide how to handle this.
    //
    // In the FabricTable::AddNewFabric and FabricTable::UpdateFabric calls,
    // the passed policy always passes for all questions of time validity.  The
    // rationale is that installed certificates should be valid at the time of
    // installation by definition.  If they are not and the commissionee and
    // commissioner disagree enough on current time, CASE will fail and our
    // fail-safe timer will expire.
    //
    // This then is ultimately how we validate that NotBefore / NotAfter in
    // newly installed certificates is workable.
    validContext.Reset();
    validContext.mRequiredKeyUsages.Set(KeyUsageFlags::kDigitalSignature);
    validContext.mRequiredKeyPurposes.Set(KeyPurposeFlags::kServerAuth);
    validContext.mValidityPolicy = policy;

    ChipLogProgress(FabricProvisioning, "Validating NOC chain");
    CHIP_ERROR err = FabricTable::VerifyCredentials(noc, icac, rcac, validContext, outCompressedFabricId, outFabricId, outNodeId,
                                                    outNocPubkey, nullptr);
    if (err != CHIP_NO_ERROR && err != CHIP_ERROR_WRONG_NODE_ID)
    {
        err = CHIP_ERROR_UNSUPPORTED_CERT_FORMAT;
    }
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(FabricProvisioning, "Failed NOC chain validation: %" CHIP_ERROR_FORMAT, err.Format());
    }
    ReturnErrorOnFailure(err);

    // Validate fabric ID match for cases like UpdateNOC.
    if (existingFabricId != kUndefinedFabricId)
    {
        VerifyOrReturnError(existingFabricId == outFabricId, CHIP_ERROR_UNSUPPORTED_CERT_FORMAT);
    }

    ChipLogProgress(FabricProvisioning, "NOC chain validation successful");
    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricInfo::SignWithOpKeypair(ByteSpan message, P256ECDSASignature & outSignature) const
{
    VerifyOrReturnError(mOperationalKey != nullptr, CHIP_ERROR_KEY_NOT_FOUND);

    return mOperationalKey->ECDSA_sign_msg(message.data(), message.size(), outSignature);
}

CHIP_ERROR FabricInfo::FetchRootPubkey(Crypto::P256PublicKey & outPublicKey) const
{
    VerifyOrReturnError(IsInitialized(), CHIP_ERROR_KEY_NOT_FOUND);
    outPublicKey = mRootPublicKey;
    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::VerifyCredentials(FabricIndex fabricIndex, const ByteSpan & noc, const ByteSpan & icac,
                                          ValidationContext & context, CompressedFabricId & outCompressedFabricId,
                                          FabricId & outFabricId, NodeId & outNodeId, Crypto::P256PublicKey & outNocPubkey,
                                          Crypto::P256PublicKey * outRootPublicKey) const
{
    uint8_t rootCertBuf[kMaxCHIPCertLength];
    MutableByteSpan rootCertSpan{ rootCertBuf };
    ReturnErrorOnFailure(FetchRootCert(fabricIndex, rootCertSpan));
    return VerifyCredentials(noc, icac, rootCertSpan, context, outCompressedFabricId, outFabricId, outNodeId, outNocPubkey,
                             outRootPublicKey);
}

CHIP_ERROR FabricTable::VerifyCredentials(const ByteSpan & noc, const ByteSpan & icac, const ByteSpan & rcac,
                                          ValidationContext & context, CompressedFabricId & outCompressedFabricId,
                                          FabricId & outFabricId, NodeId & outNodeId, Crypto::P256PublicKey & outNocPubkey,
                                          Crypto::P256PublicKey * outRootPublicKey)
{
    // TODO - Optimize credentials verification logic
    //        The certificate chain construction and verification is a compute and memory intensive operation.
    //        It can be optimized by not loading certificate (i.e. rcac) that's local and implicitly trusted.
    //        The FindValidCert() algorithm will need updates to achieve this refactor.
    constexpr uint8_t kMaxNumCertsInOpCreds = 3;

    ChipCertificateSet certificates;
    ReturnErrorOnFailure(certificates.Init(kMaxNumCertsInOpCreds));

    ReturnErrorOnFailure(certificates.LoadCert(rcac, BitFlags<CertDecodeFlags>(CertDecodeFlags::kIsTrustAnchor)));

    if (!icac.empty())
    {
        ReturnErrorOnFailure(certificates.LoadCert(icac, BitFlags<CertDecodeFlags>(CertDecodeFlags::kGenerateTBSHash)));
    }

    ReturnErrorOnFailure(certificates.LoadCert(noc, BitFlags<CertDecodeFlags>(CertDecodeFlags::kGenerateTBSHash)));

    const ChipDN & nocSubjectDN              = certificates.GetLastCert()[0].mSubjectDN;
    const CertificateKeyId & nocSubjectKeyId = certificates.GetLastCert()[0].mSubjectKeyId;

    const ChipCertificateData * resultCert = nullptr;
    // FindValidCert() checks the certificate set constructed by loading noc, icac and rcac.
    // It confirms that the certs link correctly (noc -> icac -> rcac), and have been correctly signed.
    ReturnErrorOnFailure(certificates.FindValidCert(nocSubjectDN, nocSubjectKeyId, context, &resultCert));

    ReturnErrorOnFailure(ExtractNodeIdFabricIdFromOpCert(certificates.GetLastCert()[0], &outNodeId, &outFabricId));

    CHIP_ERROR err;
    FabricId icacFabricId = kUndefinedFabricId;
    if (!icac.empty())
    {
        err = ExtractFabricIdFromCert(certificates.GetCertSet()[1], &icacFabricId);
        if (err == CHIP_NO_ERROR)
        {
            ReturnErrorCodeIf(icacFabricId != outFabricId, CHIP_ERROR_FABRIC_MISMATCH_ON_ICA);
        }
        // FabricId is optional field in ICAC and "not found" code is not treated as error.
        else if (err != CHIP_ERROR_NOT_FOUND)
        {
            return err;
        }
    }

    FabricId rcacFabricId = kUndefinedFabricId;
    err                   = ExtractFabricIdFromCert(certificates.GetCertSet()[0], &rcacFabricId);
    if (err == CHIP_NO_ERROR)
    {
        ReturnErrorCodeIf(rcacFabricId != outFabricId, CHIP_ERROR_WRONG_CERT_DN);
    }
    // FabricId is optional field in RCAC and "not found" code is not treated as error.
    else if (err != CHIP_ERROR_NOT_FOUND)
    {
        return err;
    }

    // Extract compressed fabric ID and root public key
    {
        uint8_t compressedFabricIdBuf[sizeof(uint64_t)];
        MutableByteSpan compressedFabricIdSpan(compressedFabricIdBuf);
        P256PublicKey rootPubkey(certificates.GetCertSet()[0].mPublicKey);

        ReturnErrorOnFailure(GenerateCompressedFabricId(rootPubkey, outFabricId, compressedFabricIdSpan));

        // Decode compressed fabric ID accounting for endianness, as GenerateCompressedFabricId()
        // returns a binary buffer and is agnostic of usage of the output as an integer type.
        outCompressedFabricId = Encoding::BigEndian::Get64(compressedFabricIdBuf);

        if (outRootPublicKey != nullptr)
        {
            *outRootPublicKey = rootPubkey;
        }
    }

    outNocPubkey = certificates.GetLastCert()[0].mPublicKey;

    return CHIP_NO_ERROR;
}

FabricInfo * FabricTable::FindFabric(const Crypto::P256PublicKey & rootPubKey, FabricId fabricId)
{
    P256PublicKey candidatePubKey;

    // Try to match pending fabric first if available
    bool hasPendingFabric =
        mPendingFabric.IsInitialized() && mStateFlags.HasAll(StateFlags::kIsPendingFabricDataPresent, StateFlags::kIsUpdatePending);
    if (hasPendingFabric)
    {
        bool pubKeyAvailable = (mPendingFabric.FetchRootPubkey(candidatePubKey) == CHIP_NO_ERROR);
        if (pubKeyAvailable && rootPubKey.Matches(candidatePubKey) && fabricId == mPendingFabric.GetFabricId())
        {
            return &mPendingFabric;
        }
    }

    for (auto & fabric : mStates)
    {
        if (!fabric.IsInitialized())
        {
            continue;
        }
        if (fabric.FetchRootPubkey(candidatePubKey) != CHIP_NO_ERROR)
        {
            continue;
        }
        if (rootPubKey.Matches(candidatePubKey) && fabricId == fabric.GetFabricId())
        {
            return &fabric;
        }
    }

    return nullptr;
}

FabricInfo * FabricTable::FindFabricWithIndex(FabricIndex fabricIndex)
{
    // Try to match pending fabric first if available
    bool hasPendingFabric =
        mPendingFabric.IsInitialized() && mStateFlags.HasAll(StateFlags::kIsPendingFabricDataPresent, StateFlags::kIsUpdatePending);

    if (hasPendingFabric && (mPendingFabric.GetFabricIndex() == fabricIndex))
    {
        return &mPendingFabric;
    }

    for (auto & fabric : mStates)
    {
        if (!fabric.IsInitialized())
        {
            continue;
        }

        if (fabric.GetFabricIndex() == fabricIndex)
        {
            return &fabric;
        }
    }

    return nullptr;
}

const FabricInfo * FabricTable::FindFabricWithIndex(FabricIndex fabricIndex) const
{
    // Try to match pending fabric first if available
    bool hasPendingFabric =
        mPendingFabric.IsInitialized() && mStateFlags.HasAll(StateFlags::kIsPendingFabricDataPresent, StateFlags::kIsUpdatePending);

    if (hasPendingFabric && (mPendingFabric.GetFabricIndex() == fabricIndex))
    {
        return &mPendingFabric;
    }

    for (const auto & fabric : mStates)
    {
        if (!fabric.IsInitialized())
        {
            continue;
        }

        if (fabric.GetFabricIndex() == fabricIndex)
        {
            return &fabric;
        }
    }

    return nullptr;
}

FabricInfo * FabricTable::FindFabricWithCompressedId(CompressedFabricId compressedFabricId)
{
    // Try to match pending fabric first if available
    bool hasPendingFabric =
        mPendingFabric.IsInitialized() && mStateFlags.HasAll(StateFlags::kIsPendingFabricDataPresent, StateFlags::kIsUpdatePending);

    if (hasPendingFabric && (mPendingFabric.GetCompressedFabricId() == compressedFabricId))
    {
        return &mPendingFabric;
    }

    for (auto & fabric : mStates)
    {
        if (!fabric.IsInitialized())
        {
            continue;
        }

        if (compressedFabricId == fabric.GetPeerId().GetCompressedFabricId())
        {
            return &fabric;
        }
    }
    return nullptr;
}

CHIP_ERROR FabricTable::FetchRootCert(FabricIndex fabricIndex, MutableByteSpan & outCert) const
{
    VerifyOrReturnError(mOpCertStore != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mOpCertStore->GetCertificate(fabricIndex, CertChainElement::kRcac, outCert);
}

CHIP_ERROR FabricTable::FetchICACert(FabricIndex fabricIndex, MutableByteSpan & outCert) const
{
    VerifyOrReturnError(mOpCertStore != nullptr, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(!outCert.empty(), CHIP_ERROR_INVALID_ARGUMENT);

    CHIP_ERROR err = mOpCertStore->GetCertificate(fabricIndex, CertChainElement::kIcac, outCert);
    if (err == CHIP_ERROR_NOT_FOUND)
    {
        if (mOpCertStore->HasCertificateForFabric(fabricIndex, CertChainElement::kNoc))
        {
            // Didn't find ICAC, but have NOC: return empty for ICAC since not present in chain, but chain exists
            outCert.reduce_size(0);
            return CHIP_NO_ERROR;
        }
    }

    // For all other cases, delegate to operational cert store for results
    return err;
}

CHIP_ERROR FabricTable::FetchNOCCert(FabricIndex fabricIndex, MutableByteSpan & outCert) const
{
    VerifyOrReturnError(mOpCertStore != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mOpCertStore->GetCertificate(fabricIndex, CertChainElement::kNoc, outCert);
}

CHIP_ERROR FabricTable::FetchRootPubkey(FabricIndex fabricIndex, Crypto::P256PublicKey & outPublicKey) const
{
    const FabricInfo * fabricInfo = FindFabricWithIndex(fabricIndex);
    ReturnErrorCodeIf(fabricInfo == nullptr, CHIP_ERROR_INVALID_FABRIC_INDEX);
    return fabricInfo->FetchRootPubkey(outPublicKey);
}

CHIP_ERROR FabricTable::StoreFabricMetadata(const FabricInfo * fabricInfo) const
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrDie(fabricInfo != nullptr);

    FabricIndex fabricIndex = fabricInfo->GetFabricIndex();
    VerifyOrReturnError(IsValidFabricIndex(fabricIndex), CHIP_ERROR_INTERNAL);

    // TODO: Refactor not to internally rely directly on storage
    ReturnErrorOnFailure(fabricInfo->CommitToStorage(mStorage));

    ChipLogProgress(FabricProvisioning, "Metadata for Fabric 0x%x persisted to storage.", static_cast<unsigned>(fabricIndex));

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::LoadFromStorage(FabricInfo * fabric, FabricIndex newFabricIndex)
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    if (fabric->IsInitialized())
    {
        // TODO: When/how does this occur?
        return CHIP_NO_ERROR;
    }

    uint8_t nocBuf[kMaxCHIPCertLength];
    MutableByteSpan nocSpan{ nocBuf };
    uint8_t rcacBuf[kMaxCHIPCertLength];
    MutableByteSpan rcacSpan{ rcacBuf };

    CHIP_ERROR err = FetchNOCCert(newFabricIndex, nocSpan);
    if (err == CHIP_NO_ERROR)
    {
        err = FetchRootCert(newFabricIndex, rcacSpan);
    }

    // TODO: Sweep-away fabrics without RCAC/NOC by deleting everything and marking fabric gone.

    if (err == CHIP_NO_ERROR)
    {
        err = fabric->LoadFromStorage(mStorage, newFabricIndex, rcacSpan, nocSpan);
    }

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(FabricProvisioning, "Fabric failed to load Fabric (0x%x): %" CHIP_ERROR_FORMAT,
                     static_cast<unsigned>(newFabricIndex), err.Format());
        fabric->Reset();
        return err;
    }

    ChipLogProgress(FabricProvisioning,
                    "Fabric index 0x%x was retrieved from storage. Compressed FabricId 0x" ChipLogFormatX64
                    ", FabricId 0x" ChipLogFormatX64
                    ", NodeId 0x" ChipLogFormatX64 ", VendorId 0x%04X",
                    static_cast<unsigned>(fabric->GetFabricIndex()), ChipLogValueX64(fabric->GetCompressedFabricId()), ChipLogValueX64(fabric->GetFabricId()),
                    ChipLogValueX64(fabric->GetNodeId()), fabric->GetVendorId());

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::AddNewFabricForTest(const ByteSpan & rootCert, const ByteSpan & icacCert, const ByteSpan & nocCert,
                                            const ByteSpan & opKeySpan, FabricIndex * outFabricIndex)
{
    VerifyOrReturnError(outFabricIndex != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    CHIP_ERROR err = CHIP_ERROR_INTERNAL;

    Crypto::P256Keypair injectedOpKey;
    Crypto::P256SerializedKeypair injectedOpKeysSerialized;

    Crypto::P256Keypair * opKey = nullptr;
    if (!opKeySpan.empty())
    {
        memcpy(injectedOpKeysSerialized.Bytes(), opKeySpan.data(), opKeySpan.size());
        SuccessOrExit(err = injectedOpKeysSerialized.SetLength(opKeySpan.size()));
        SuccessOrExit(err = injectedOpKey.Deserialize(injectedOpKeysSerialized));
        opKey = &injectedOpKey;
    }

    SuccessOrExit(err = AddNewPendingTrustedRootCert(rootCert));
    SuccessOrExit(err = AddNewPendingFabricWithProvidedOpKey(nocCert, icacCert, VendorId::TestVendor1, opKey,
                                                             /*hasExternallyOwnedKeypair =*/false, outFabricIndex));
    SuccessOrExit(err = CommitPendingFabricData());
exit:
    if (err != CHIP_NO_ERROR)
    {
        RevertPendingFabricData();
    }
    return err;
}

/*
 * A validation policy we can pass into VerifyCredentials to extract the
 * latest NotBefore time in the certificate chain without having to load the
 * certificates into memory again, and one which will pass validation for all
 * questions of NotBefore / NotAfter validity.
 *
 * The rationale is that installed certificates should be valid at the time of
 * installation by definition.  If they are not and the commissionee and
 * commissioner disagree enough on current time, CASE will fail and our
 * fail-safe timer will expire.
 *
 * This then is ultimately how we validate that NotBefore / NotAfter in
 * newly installed certificates is workable.
 */
class NotBeforeCollector : public Credentials::CertificateValidityPolicy
{
public:
    NotBeforeCollector() : mLatestNotBefore(0) {}
    CHIP_ERROR ApplyCertificateValidityPolicy(const ChipCertificateData * cert, uint8_t depth,
                                              CertificateValidityResult result) override
    {
        if (cert->mNotBeforeTime > mLatestNotBefore.count())
        {
            mLatestNotBefore = System::Clock::Seconds32(cert->mNotBeforeTime);
        }
        return CHIP_NO_ERROR;
    }
    System::Clock::Seconds32 mLatestNotBefore;
};

CHIP_ERROR FabricTable::NotifyFabricUpdated(FabricIndex fabricIndex)
{
    FabricTable::Delegate * delegate = mDelegateListRoot;
    while (delegate)
    {
        // It is possible that delegate will remove itself from the list in the callback
        // so we grab the next delegate in the list now.
        FabricTable::Delegate * nextDelegate = delegate->next;
        delegate->OnFabricUpdated(*this, fabricIndex);
        delegate = nextDelegate;
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::NotifyFabricCommitted(FabricIndex fabricIndex)
{
    FabricTable::Delegate * delegate = mDelegateListRoot;
    while (delegate)
    {
        // It is possible that delegate will remove itself from the list in the callback
        // so we grab the next delegate in the list now.
        FabricTable::Delegate * nextDelegate = delegate->next;
        delegate->OnFabricCommitted(*this, fabricIndex);
        delegate = nextDelegate;
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR
FabricTable::AddOrUpdateInner(FabricIndex fabricIndex, Crypto::P256Keypair * existingOpKey, bool isExistingOpKeyExternallyOwned,
                              uint16_t vendorId, FabricIndex * outputIndex)
{
    // All parameters pre-validated before we get here

    bool isAddition = (fabricIndex == kUndefinedFabricIndex);

    FabricInfo::InitParams newFabricInfo;
    FabricInfo * fabricEntry    = nullptr;
    FabricId fabricIdToValidate = kUndefinedFabricId;
    CharSpan fabricLabel("");

    if (isAddition)
    {
        // Initialization for Adding a fabric

        // Make sure we have an available fabric index
        if (!mNextAvailableFabricIndex.HasValue())
        {
            // No more indices available.  Bail out.
            return CHIP_ERROR_NO_MEMORY;
        }

        fabricIndex = mNextAvailableFabricIndex.Value();

        // Find an available slot.
        for (auto & fabric : mStates)
        {
            if (fabric.IsInitialized())
            {
                continue;
            }
            fabricEntry = &fabric;
            break;
        }

        VerifyOrReturnError(fabricEntry != nullptr, CHIP_ERROR_NO_MEMORY);

        newFabricInfo.vendorId    = vendorId;
        newFabricInfo.fabricIndex = fabricIndex;
    }
    else
    {
        // Initialization for Upating fabric: setting up a shadow fabricInfo
        const FabricInfo * existingFabric = FindFabricWithIndex(fabricIndex);
        VerifyOrReturnError(existingFabric != nullptr, CHIP_ERROR_INTERNAL);

        mPendingFabric.Reset();
        fabricEntry = &mPendingFabric;

        newFabricInfo.vendorId    = existingFabric->GetVendorId();
        newFabricInfo.fabricIndex = fabricIndex;

        fabricIdToValidate = existingFabric->GetFabricId();
        fabricLabel        = existingFabric->GetFabricLabel();
    }

    // Make sure to not modify any of our state until ValidateIncomingNOCChain passes.
    NotBeforeCollector notBeforeCollector;
    P256PublicKey nocPubKey;

    // Validate the cert chain prior to adding
    {
        Platform::ScopedMemoryBuffer<uint8_t> nocBuf;
        Platform::ScopedMemoryBuffer<uint8_t> icacBuf;
        Platform::ScopedMemoryBuffer<uint8_t> rcacBuf;

        ReturnErrorCodeIf(!nocBuf.Alloc(kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);
        ReturnErrorCodeIf(!icacBuf.Alloc(kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);
        ReturnErrorCodeIf(!rcacBuf.Alloc(kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);

        MutableByteSpan nocSpan{ nocBuf.Get(), kMaxCHIPCertLength };
        MutableByteSpan icacSpan{ icacBuf.Get(), kMaxCHIPCertLength };
        MutableByteSpan rcacSpan{ rcacBuf.Get(), kMaxCHIPCertLength };

        ReturnErrorOnFailure(FetchNOCCert(fabricIndex, nocSpan));
        ReturnErrorOnFailure(FetchICACert(fabricIndex, icacSpan));
        ReturnErrorOnFailure(FetchRootCert(fabricIndex, rcacSpan));

        ReturnErrorOnFailure(ValidateIncomingNOCChain(nocSpan, icacSpan, rcacSpan, fabricIdToValidate, &notBeforeCollector,
                                                      newFabricInfo.compressedFabricId, newFabricInfo.fabricId,
                                                      newFabricInfo.nodeId, nocPubKey));

        P256PublicKeySpan rootPubKeySpan;
        ReturnErrorOnFailure(ExtractPublicKeyFromChipCert(rcacSpan, rootPubKeySpan));
        newFabricInfo.rootPublicKey = rootPubKeySpan;
    }

    if (existingOpKey != nullptr)
    {
        // Verify that public key in NOC matches public key of the provided keypair.
        // When operational key is not injected (e.g. when mOperationalKeystore != nullptr)
        // the check is done by the keystore in `ActivatePendingOperationalKey`.
        VerifyOrReturnError(existingOpKey->Pubkey().Length() == nocPubKey.Length(), CHIP_ERROR_INVALID_PUBLIC_KEY);
        VerifyOrReturnError(memcmp(existingOpKey->Pubkey().ConstBytes(), nocPubKey.ConstBytes(), nocPubKey.Length()) == 0,
                            CHIP_ERROR_INVALID_PUBLIC_KEY);

        newFabricInfo.operationalKeypair        = existingOpKey;
        newFabricInfo.hasExternallyOwnedKeypair = isExistingOpKeyExternallyOwned;
    }
    else if (mOperationalKeystore != nullptr)
    {
        // If a keystore exists, we activate the operational key now, which also validates if it was previously installed
        ReturnErrorOnFailure(mOperationalKeystore->ActivateOpKeypairForFabric(fabricIndex, nocPubKey));
    }
    else
    {
        return CHIP_ERROR_INCORRECT_STATE;
    }

    // Update local copy of fabric data. For add it's a new entry, for update, it's `mPendingFabric` shadow entry.
    ReturnErrorOnFailure(fabricEntry->Init(newFabricInfo));

    // Set the label, matching add/update semantics of empty/existing.
    fabricEntry->SetFabricLabel(fabricLabel);

    if (isAddition)
    {
        ChipLogProgress(FabricProvisioning, "Added new fabric at index: 0x%x",
                        static_cast<unsigned>(fabricEntry->GetFabricIndex()));
        ChipLogProgress(FabricProvisioning, "Assigned compressed fabric ID: 0x" ChipLogFormatX64 ", node ID: 0x" ChipLogFormatX64,
                        ChipLogValueX64(fabricEntry->GetCompressedFabricId()), ChipLogValueX64(fabricEntry->GetNodeId()));
    }
    else
    {
        ChipLogProgress(FabricProvisioning, "Updated fabric at index: 0x%x, Node ID: 0x" ChipLogFormatX64,
                        static_cast<unsigned>(fabricEntry->GetFabricIndex()), ChipLogValueX64(fabricEntry->GetNodeId()));
    }

    mPendingLastKnownGoodTime = notBeforeCollector.mLatestNotBefore;

    *outputIndex = fabricIndex;

    // Must be the last thing before we return, as this is undone later on error handling within Delete.
    if (isAddition)
    {
        mFabricCount++;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::Delete(FabricIndex fabricIndex)
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(IsValidFabricIndex(fabricIndex), CHIP_ERROR_INVALID_ARGUMENT);

    FabricInfo * fabricInfo = FindFabricWithIndex(fabricIndex);
    if (fabricInfo == &mPendingFabric)
    {
        // Asked to Delete while pending an update: reset the pending state and
        // get back to the underlying fabric data for existing fabric.
        RevertPendingFabricData();
        fabricInfo = FindFabricWithIndex(fabricIndex);
    }

    bool fabricIsInitialized = fabricInfo != nullptr && fabricInfo->IsInitialized();
    CHIP_ERROR metadataErr   = DeleteMetadataFromStorage(fabricIndex); // Delete from storage regardless

    CHIP_ERROR opKeyErr = CHIP_NO_ERROR;
    if (mOperationalKeystore != nullptr)
    {
        opKeyErr = mOperationalKeystore->RemoveOpKeypairForFabric(fabricIndex);
        // Not having found data is not an error, we may just have gotten here
        // on a fail-safe expiry after `RevertPendingFabricData`.
        if (opKeyErr == CHIP_ERROR_INVALID_FABRIC_INDEX)
        {
            opKeyErr = CHIP_NO_ERROR;
        }
    }

    CHIP_ERROR opCertsErr = CHIP_NO_ERROR;
    if (mOpCertStore != nullptr)
    {
        opCertsErr = mOpCertStore->RemoveOpCertsForFabric(fabricIndex);
        // Not having found data is not an error, we may just have gotten here
        // on a fail-safe expiry after `RevertPendingFabricData`.
        if (opCertsErr == CHIP_ERROR_INVALID_FABRIC_INDEX)
        {
            opCertsErr = CHIP_NO_ERROR;
        }
    }

    if (!fabricIsInitialized)
    {
        // Make sure to return the error our API promises, not whatever storage
        // chose to return.
        return CHIP_ERROR_NOT_FOUND;
    }

    // Since fabricIsInitialized was true, fabric is not null.
    fabricInfo->Reset();

    if (!mNextAvailableFabricIndex.HasValue())
    {
        // We must have been in a situation where CHIP_CONFIG_MAX_FABRICS is 254
        // and our fabric table was full, so there was no valid next index.  We
        // have a single available index now, though; use it as
        // mNextAvailableFabricIndex.
        mNextAvailableFabricIndex.SetValue(fabricIndex);
    }
    // If StoreFabricIndexInfo fails here, that's probably OK.  When we try to
    // read things from storage later we will realize there is nothing for this
    // index.
    StoreFabricIndexInfo();

    // If we ever start moving the FabricInfo entries around in the array on
    // delete, we should update DeleteAllFabrics to handle that.
    if (mFabricCount == 0)
    {
        ChipLogError(FabricProvisioning, "Trying to delete a fabric, but the current fabric count is already 0");
    }
    else
    {
        mFabricCount--;
        ChipLogProgress(FabricProvisioning, "Fabric (0x%x) deleted.", static_cast<unsigned>(fabricIndex));
    }

    if (mDelegateListRoot != nullptr)
    {
        FabricTable::Delegate * delegate = mDelegateListRoot;
        while (delegate)
        {
            // It is possible that delegate will remove itself from the list in OnFabricRemoved,
            // so we grab the next delegate in the list now.
            FabricTable::Delegate * nextDelegate = delegate->next;
            delegate->OnFabricRemoved(*this, fabricIndex);
            delegate = nextDelegate;
        }
    }

    // Only return error after trying really hard to remove everything we could
    ReturnErrorOnFailure(metadataErr);
    ReturnErrorOnFailure(opKeyErr);
    ReturnErrorOnFailure(opCertsErr);

    return CHIP_NO_ERROR;
}

void FabricTable::DeleteAllFabrics()
{
    static_assert(kMaxValidFabricIndex <= UINT8_MAX, "Cannot create more fabrics than UINT8_MAX");

    RevertPendingFabricData();

    for (auto & fabric : *this)
    {
        Delete(fabric.GetFabricIndex());
    }
}

CHIP_ERROR FabricTable::Init(const FabricTable::InitParams & initParams)
{
    VerifyOrReturnError(initParams.storage != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(initParams.opCertStore != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    mStorage             = initParams.storage;
    mOperationalKeystore = initParams.operationalKeystore;
    mOpCertStore         = initParams.opCertStore;

    ChipLogDetail(FabricProvisioning, "Initializing FabricTable from persistent storage");

    // Load the current fabrics from the storage.
    static_assert(kMaxValidFabricIndex <= UINT8_MAX, "Cannot create more fabrics than UINT8_MAX");

    mFabricCount = 0;
    for (auto & fabric : mStates)
    {
        fabric.Reset();
    }
    mNextAvailableFabricIndex.SetValue(kMinValidFabricIndex);

    // Init failure of Last Known Good Time is non-fatal.  If Last Known Good
    // Time is unknown during incoming certificate validation for CASE and
    // current time is also unknown, the certificate validity policy will see
    // this condition and can act appropriately.
    mLastKnownGoodTime.Init(mStorage);

    uint8_t buf[IndexInfoTLVMaxSize()];
    uint16_t size = sizeof(buf);
    DefaultStorageKeyAllocator keyAlloc;
    CHIP_ERROR err = mStorage->SyncGetKeyValue(keyAlloc.FabricIndexInfo(), buf, size);
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND)
    {
        // No fabrics yet.  Nothing to be done here.
    }
    else
    {
        ReturnErrorOnFailure(err);
        TLV::ContiguousBufferTLVReader reader;
        reader.Init(buf, size);

        ReturnErrorOnFailure(ReadFabricInfo(reader));
    }

    return CHIP_NO_ERROR;
}

void FabricTable::Forget(FabricIndex fabricIndex)
{
    VerifyOrReturn(IsValidFabricIndex(fabricIndex));

    ChipLogProgress(FabricProvisioning, "Forgetting fabric 0x%x", static_cast<unsigned>(fabricIndex));

    auto * fabricInfo = FindFabricWithIndex(fabricIndex);
    VerifyOrReturn(fabricInfo != nullptr);

    RevertPendingFabricData();
    fabricInfo->Reset();
}

void FabricTable::Shutdown()
{
    VerifyOrReturn(mStorage != nullptr);
    ChipLogProgress(FabricProvisioning, "Shutting down FabricTable");

    // Remove all links to every delegate
    FabricTable::Delegate * delegate = mDelegateListRoot;
    while (delegate)
    {
        FabricTable::Delegate * temp = delegate->next;
        delegate->next               = nullptr;
        delegate                     = temp;
    }

    RevertPendingFabricData();
    for (FabricInfo & fabricInfo : mStates)
    {
        // Clear-out any FabricInfo-owned operational keys and make sure any further
        // direct lookups fail.
        fabricInfo.Reset();
    }

    mStorage = nullptr;
}

CHIP_ERROR FabricTable::AddFabricDelegate(FabricTable::Delegate * delegate)
{
    VerifyOrReturnError(delegate != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    for (FabricTable::Delegate * iter = mDelegateListRoot; iter != nullptr; iter = iter->next)
    {
        if (iter == delegate)
        {
            return CHIP_NO_ERROR;
        }
    }
    delegate->next    = mDelegateListRoot;
    mDelegateListRoot = delegate;
    return CHIP_NO_ERROR;
}

void FabricTable::RemoveFabricDelegate(FabricTable::Delegate * delegateToRemove)
{
    VerifyOrReturn(delegateToRemove != nullptr);

    if (delegateToRemove == mDelegateListRoot)
    {
        // Removing head of the list, keep link to next, may
        // be nullptr.
        mDelegateListRoot = mDelegateListRoot->next;
    }
    else
    {
        // Removing some other item: check if next, and
        // remove the link, keeping its neighbour.
        FabricTable::Delegate * currentNode = mDelegateListRoot;

        while (currentNode)
        {
            if (currentNode->next == delegateToRemove)
            {
                FabricTable::Delegate * temp = delegateToRemove->next;
                currentNode->next            = temp;
                delegateToRemove->next       = nullptr;
                return;
            }

            currentNode = currentNode->next;
        }
    }
}

CHIP_ERROR FabricTable::SetLastKnownGoodChipEpochTime(System::Clock::Seconds32 lastKnownGoodChipEpochTime)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    // Find our latest NotBefore time for any installed certificate.
    System::Clock::Seconds32 latestNotBefore = System::Clock::Seconds32(0);
    for (auto & fabric : mStates)
    {
        if (!fabric.IsInitialized())
        {
            continue;
        }
        {
            uint8_t rcacBuf[kMaxCHIPCertLength];
            MutableByteSpan rcacSpan{ rcacBuf };
            SuccessOrExit(err = FetchRootCert(fabric.GetFabricIndex(), rcacSpan));
            chip::System::Clock::Seconds32 rcacNotBefore;
            SuccessOrExit(err = Credentials::ExtractNotBeforeFromChipCert(rcacSpan, rcacNotBefore));
            latestNotBefore = rcacNotBefore > latestNotBefore ? rcacNotBefore : latestNotBefore;
        }
        {
            uint8_t icacBuf[kMaxCHIPCertLength];
            MutableByteSpan icacSpan{ icacBuf };
            SuccessOrExit(err = FetchICACert(fabric.GetFabricIndex(), icacSpan));
            if (!icacSpan.empty())
            {
                chip::System::Clock::Seconds32 icacNotBefore;
                ReturnErrorOnFailure(Credentials::ExtractNotBeforeFromChipCert(icacSpan, icacNotBefore));
                latestNotBefore = icacNotBefore > latestNotBefore ? icacNotBefore : latestNotBefore;
            }
        }
        {
            uint8_t nocBuf[kMaxCHIPCertLength];
            MutableByteSpan nocSpan{ nocBuf };
            SuccessOrExit(err = FetchNOCCert(fabric.GetFabricIndex(), nocSpan));
            chip::System::Clock::Seconds32 nocNotBefore;
            ReturnErrorOnFailure(Credentials::ExtractNotBeforeFromChipCert(nocSpan, nocNotBefore));
            latestNotBefore = nocNotBefore > latestNotBefore ? nocNotBefore : latestNotBefore;
        }
    }
    // Pass this to the LastKnownGoodTime object so it can make determination
    // of the legality of our new proposed time.
    SuccessOrExit(err = mLastKnownGoodTime.SetLastKnownGoodChipEpochTime(lastKnownGoodChipEpochTime, latestNotBefore));
exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(FabricProvisioning, "Failed to update Known Good Time: %" CHIP_ERROR_FORMAT, err.Format());
    }
    return err;
}

namespace {
// Increment a fabric index in a way that ensures that it stays in the valid
// range [kMinValidFabricIndex, kMaxValidFabricIndex].
FabricIndex NextFabricIndex(FabricIndex fabricIndex)
{
    if (fabricIndex == kMaxValidFabricIndex)
    {
        return kMinValidFabricIndex;
    }

    return static_cast<FabricIndex>(fabricIndex + 1);
}
} // anonymous namespace

void FabricTable::UpdateNextAvailableFabricIndex()
{
    // Only called when mNextAvailableFabricIndex.HasValue()
    for (FabricIndex candidate = NextFabricIndex(mNextAvailableFabricIndex.Value()); candidate != mNextAvailableFabricIndex.Value();
         candidate             = NextFabricIndex(candidate))
    {
        if (!FindFabricWithIndex(candidate))
        {
            mNextAvailableFabricIndex.SetValue(candidate);
            return;
        }
    }

    mNextAvailableFabricIndex.ClearValue();
}

CHIP_ERROR FabricTable::StoreFabricIndexInfo() const
{
    uint8_t buf[IndexInfoTLVMaxSize()];
    TLV::TLVWriter writer;
    writer.Init(buf);

    TLV::TLVType outerType;
    ReturnErrorOnFailure(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outerType));

    if (mNextAvailableFabricIndex.HasValue())
    {
        writer.Put(kNextAvailableFabricIndexTag, mNextAvailableFabricIndex.Value());
    }
    else
    {
        writer.PutNull(kNextAvailableFabricIndexTag);
    }

    TLV::TLVType innerContainerType;
    ReturnErrorOnFailure(writer.StartContainer(kFabricIndicesTag, TLV::kTLVType_Array, innerContainerType));
    // Only enumerate the fabrics that are initialized.
    for (const auto & fabric : *this)
    {
        writer.Put(TLV::AnonymousTag(), fabric.GetFabricIndex());
    }
    ReturnErrorOnFailure(writer.EndContainer(innerContainerType));
    ReturnErrorOnFailure(writer.EndContainer(outerType));

    const auto indexInfoLength = writer.GetLengthWritten();
    VerifyOrReturnError(CanCastTo<uint16_t>(indexInfoLength), CHIP_ERROR_BUFFER_TOO_SMALL);

    DefaultStorageKeyAllocator keyAlloc;
    ReturnErrorOnFailure(mStorage->SyncSetKeyValue(keyAlloc.FabricIndexInfo(), buf, static_cast<uint16_t>(indexInfoLength)));

    return CHIP_NO_ERROR;
}

void FabricTable::EnsureNextAvailableFabricIndexUpdated()
{
    if (!mNextAvailableFabricIndex.HasValue() && mFabricCount < kMaxValidFabricIndex)
    {
        // We must have a fabric index available here. This situation could
        // happen if we fail to store fabric index info when deleting a
        // fabric.
        mNextAvailableFabricIndex.SetValue(kMinValidFabricIndex);
        if (FindFabricWithIndex(kMinValidFabricIndex))
        {
            UpdateNextAvailableFabricIndex();
        }
    }
}

CHIP_ERROR FabricTable::ReadFabricInfo(TLV::ContiguousBufferTLVReader & reader)
{
    ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Structure, TLV::AnonymousTag()));
    TLV::TLVType containerType;
    ReturnErrorOnFailure(reader.EnterContainer(containerType));

    ReturnErrorOnFailure(reader.Next(kNextAvailableFabricIndexTag));
    if (reader.GetType() == TLV::kTLVType_Null)
    {
        mNextAvailableFabricIndex.ClearValue();
    }
    else
    {
        ReturnErrorOnFailure(reader.Get(mNextAvailableFabricIndex.Emplace()));
    }

    ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Array, kFabricIndicesTag));
    TLV::TLVType arrayType;
    ReturnErrorOnFailure(reader.EnterContainer(arrayType));

    CHIP_ERROR err;
    while ((err = reader.Next()) == CHIP_NO_ERROR)
    {
        if (mFabricCount >= ArraySize(mStates))
        {
            // We have nowhere to deserialize this fabric info into.
            return CHIP_ERROR_NO_MEMORY;
        }

        auto & fabric                  = mStates[mFabricCount];
        FabricIndex currentFabricIndex = kUndefinedFabricIndex;
        ReturnErrorOnFailure(reader.Get(currentFabricIndex));

        err = LoadFromStorage(&fabric, currentFabricIndex);
        if (err == CHIP_NO_ERROR)
        {
            ++mFabricCount;
        }
        else
        {
            // This could happen if we failed to store our fabric index info
            // after we deleted the fabric from storage.  Just ignore this
            // fabric index and keep going.
        }
    }

    if (err != CHIP_END_OF_TLV)
    {
        return err;
    }

    ReturnErrorOnFailure(reader.ExitContainer(arrayType));

    ReturnErrorOnFailure(reader.ExitContainer(containerType));
    ReturnErrorOnFailure(reader.VerifyEndOfContainer());

    EnsureNextAvailableFabricIndexUpdated();

    return CHIP_NO_ERROR;
}

bool FabricTable::HasOperationalKeyForFabric(FabricIndex fabricIndex) const
{
    const FabricInfo * fabricInfo = FindFabricWithIndex(fabricIndex);
    VerifyOrReturnError(fabricInfo != nullptr, false);

    if (fabricInfo->HasOperationalKey())
    {
        // Legacy case of manually injected keys: delegate to FabricInfo directly
        return true;
    }
    if (mOperationalKeystore != nullptr)
    {
        return mOperationalKeystore->HasOpKeypairForFabric(fabricIndex);
    }

    return false;
}

CHIP_ERROR FabricTable::SignWithOpKeypair(FabricIndex fabricIndex, ByteSpan message, P256ECDSASignature & outSignature) const
{
    const FabricInfo * fabricInfo = FindFabricWithIndex(fabricIndex);
    VerifyOrReturnError(fabricInfo != nullptr, CHIP_ERROR_KEY_NOT_FOUND);

    if (fabricInfo->HasOperationalKey())
    {
        // Legacy case of manually injected FabricInfo: delegate to FabricInfo directly
        return fabricInfo->SignWithOpKeypair(message, outSignature);
    }
    if (mOperationalKeystore != nullptr)
    {
        return mOperationalKeystore->SignWithOpKeypair(fabricIndex, message, outSignature);
    }

    return CHIP_ERROR_KEY_NOT_FOUND;
}

bool FabricTable::HasPendingOperationalKey(bool & outIsPendingKeyForUpdateNoc) const
{
    // We can only manage commissionable pending fail-safe state if we have a keystore
    bool hasOpKeyPending = mStateFlags.Has(StateFlags::kIsOperationalKeyPending);

    if (hasOpKeyPending)
    {
        // We kept track of whether the last `AllocatePendingOperationalKey` for was for an update,
        // so give it back out here.
        outIsPendingKeyForUpdateNoc = mStateFlags.Has(StateFlags::kIsPendingKeyForUpdateNoc);
    }

    return hasOpKeyPending;
}

bool FabricTable::SetPendingDataFabricIndex(FabricIndex fabricIndex)
{
    bool isLegal = (mFabricIndexWithPendingState == kUndefinedFabricIndex) || (mFabricIndexWithPendingState == fabricIndex);

    if (isLegal)
    {
        mFabricIndexWithPendingState = fabricIndex;
    }
    return isLegal;
}

CHIP_ERROR FabricTable::AllocatePendingOperationalKey(Optional<FabricIndex> fabricIndex, MutableByteSpan & outputCsr)
{
    // We can only manage commissionable pending fail-safe state if we have a keystore
    VerifyOrReturnError(mOperationalKeystore != nullptr, CHIP_ERROR_INCORRECT_STATE);

    // We can only allocate a pending key if no pending state (NOC, ICAC) already present,
    // since there can only be one pending state per fail-safe.
    VerifyOrReturnError(!mStateFlags.Has(StateFlags::kIsPendingFabricDataPresent), CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(outputCsr.size() >= Crypto::kMAX_CSR_Length, CHIP_ERROR_BUFFER_TOO_SMALL);

    EnsureNextAvailableFabricIndexUpdated();
    FabricIndex fabricIndexToUse = kUndefinedFabricIndex;

    if (fabricIndex.HasValue())
    {
        // Check we not are trying to do an update but also change the root: forbidden
        ReturnErrorCodeIf(mStateFlags.Has(StateFlags::kIsTrustedRootPending), CHIP_ERROR_INCORRECT_STATE);

        // Fabric udpate case (e.g. UpdateNOC): we already know the fabric index
        fabricIndexToUse = fabricIndex.Value();
        mStateFlags.Set(StateFlags::kIsPendingKeyForUpdateNoc);
    }
    else if (mNextAvailableFabricIndex.HasValue())
    {
        // Fabric addition case (e.g. AddNOC): we need to allocate for the next pending fabric index
        fabricIndexToUse = mNextAvailableFabricIndex.Value();
        mStateFlags.Clear(StateFlags::kIsPendingKeyForUpdateNoc);
    }
    else
    {
        // Fabric addition, but adding NOC would fail on table full: let's not allocate a key
        return CHIP_ERROR_NO_MEMORY;
    }

    VerifyOrReturnError(IsValidFabricIndex(fabricIndexToUse), CHIP_ERROR_INVALID_FABRIC_INDEX);
    VerifyOrReturnError(SetPendingDataFabricIndex(fabricIndexToUse), CHIP_ERROR_INCORRECT_STATE);
    ReturnErrorOnFailure(mOperationalKeystore->NewOpKeypairForFabric(mFabricIndexWithPendingState, outputCsr));
    mStateFlags.Set(StateFlags::kIsOperationalKeyPending);

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::ActivatePendingOperationalKey(const Crypto::P256PublicKey & nocSubjectPublicKey)
{
    // We can only manage commissionable pending fail-safe state if we have a keystore
    VerifyOrReturnError(mOperationalKeystore != nullptr, CHIP_ERROR_INCORRECT_STATE);

    VerifyOrReturnError(!mStateFlags.Has(StateFlags::kIsOperationalKeyPending), CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(IsValidFabricIndex(mFabricIndexWithPendingState), CHIP_ERROR_INCORRECT_STATE);

    return mOperationalKeystore->ActivateOpKeypairForFabric(mFabricIndexWithPendingState, nocSubjectPublicKey);
}

CHIP_ERROR FabricTable::AddNewPendingTrustedRootCert(const ByteSpan & rcac)
{
    VerifyOrReturnError(mOpCertStore != nullptr, CHIP_ERROR_INCORRECT_STATE);

    // We should not already have pending NOC chain elements when we get here
    ReturnErrorCodeIf(
        mStateFlags.HasAny(StateFlags::kIsTrustedRootPending, StateFlags::kIsUpdatePending, StateFlags::kIsAddPending),
        CHIP_ERROR_INCORRECT_STATE);

    EnsureNextAvailableFabricIndexUpdated();
    FabricIndex fabricIndexToUse = kUndefinedFabricIndex;

    if (mNextAvailableFabricIndex.HasValue())
    {
        // Mark we have some pending data for a given fabric.
        fabricIndexToUse = mNextAvailableFabricIndex.Value();
    }
    else
    {
        // Fabric addition, but adding root would fail on table full: let's not allocate a fabric
        return CHIP_ERROR_NO_MEMORY;
    }

    VerifyOrReturnError(IsValidFabricIndex(fabricIndexToUse), CHIP_ERROR_INVALID_FABRIC_INDEX);
    VerifyOrReturnError(SetPendingDataFabricIndex(fabricIndexToUse), CHIP_ERROR_INCORRECT_STATE);
    ReturnErrorOnFailure(mOpCertStore->AddNewTrustedRootCertForFabric(fabricIndexToUse, rcac));

    mStateFlags.Set(StateFlags::kIsPendingFabricDataPresent);
    mStateFlags.Set(StateFlags::kIsTrustedRootPending);
    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::FindExistingFabricByNocChaining(FabricIndex pendingFabricIndex, const ByteSpan & noc,
                                                        FabricIndex & outMatchingFabricIndex) const
{
    // Check whether we already have a matching fabric from a cert chain perspective.
    // To do so we have to extract the FabricID from the NOC and the root public key from the RCAC.
    // We assume the RCAC is currently readable from OperationalCertificateStore, whether pending
    // or persisted.
    FabricId fabricId;
    {
        NodeId unused;
        ReturnErrorOnFailure(ExtractNodeIdFabricIdFromOpCert(noc, &unused, &fabricId));
    }

    // Try to find the root public key from the current existing fabric
    Crypto::P256PublicKey candidateRootKey;
    {
        uint8_t tempRcac[kMaxCHIPCertLength];
        MutableByteSpan tempRcacSpan{ tempRcac };
        Credentials::P256PublicKeySpan publicKeySpan;
        ReturnErrorOnFailure(FetchRootCert(pendingFabricIndex, tempRcacSpan));
        ReturnErrorOnFailure(ExtractPublicKeyFromChipCert(tempRcacSpan, publicKeySpan));
        candidateRootKey = publicKeySpan;
    }

    for (auto & existingFabric : *this)
    {
        if (existingFabric.GetFabricId() == fabricId)
        {
            P256PublicKey existingRootKey;
            ReturnErrorOnFailure(FetchRootPubkey(existingFabric.GetFabricIndex(), existingRootKey));

            if (existingRootKey.Matches(candidateRootKey))
            {
                outMatchingFabricIndex = existingFabric.GetFabricIndex();
                return CHIP_NO_ERROR;
            }
        }
    }

    // Did not find: set outMatchingFabricIndex to kUndefinedFabricIndex
    outMatchingFabricIndex = kUndefinedFabricIndex;
    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::AddNewPendingFabricCommon(const ByteSpan & noc, const ByteSpan & icac, uint16_t vendorId,
                                                  Crypto::P256Keypair * existingOpKey, bool isExistingOpKeyExternallyOwned,
                                                  FabricIndex * outNewFabricIndex)
{
    VerifyOrReturnError(mOpCertStore != nullptr, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(outNewFabricIndex != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    static_assert(kMaxValidFabricIndex <= UINT8_MAX, "Cannot create more fabrics than UINT8_MAX");

    // We should already have a pending root when we get here
    VerifyOrReturnError(mStateFlags.Has(StateFlags::kIsTrustedRootPending), CHIP_ERROR_INCORRECT_STATE);
    // We should not have pending update when we get here
    VerifyOrReturnError(!mStateFlags.Has(StateFlags::kIsUpdatePending), CHIP_ERROR_INCORRECT_STATE);

    EnsureNextAvailableFabricIndexUpdated();
    FabricIndex fabricIndexToUse = kUndefinedFabricIndex;
    if (mNextAvailableFabricIndex.HasValue())
    {
        // Mark we have some pending data for a given fabric.
        fabricIndexToUse = mNextAvailableFabricIndex.Value();
    }
    else
    {
        // Fabric addition, but adding fabric would fail on table full: let's not allocate a fabric
        return CHIP_ERROR_NO_MEMORY;
    }

    if (existingOpKey == nullptr)
    {
        // If existing operational key not provided, we need to have a keystore present.
        // It should already have an operational key pending.
        VerifyOrReturnError(mOperationalKeystore != nullptr, CHIP_ERROR_KEY_NOT_FOUND);
        // Make sure we have an operational key, pending or not
        VerifyOrReturnError(mOperationalKeystore->HasOpKeypairForFabric(fabricIndexToUse) ||
                                mOperationalKeystore->HasPendingOpKeypair(),
                            CHIP_ERROR_KEY_NOT_FOUND);
    }

    VerifyOrReturnError(IsValidFabricIndex(fabricIndexToUse), CHIP_ERROR_INVALID_FABRIC_INDEX);

    // Check for new fabric colliding with an existing fabric
    if (!mStateFlags.Has(StateFlags::kAreCollidingFabricsIgnored))
    {
        FabricIndex collidingFabricIndex = kUndefinedFabricIndex;
        ReturnErrorOnFailure(FindExistingFabricByNocChaining(fabricIndexToUse, noc, collidingFabricIndex));
        ReturnErrorCodeIf(collidingFabricIndex != kUndefinedFabricIndex, CHIP_ERROR_FABRIC_EXISTS);
    }

    // We don't have a collision, handle the temp insert of NOC/ICAC
    ReturnErrorOnFailure(mOpCertStore->AddNewOpCertsForFabric(fabricIndexToUse, noc, icac));
    VerifyOrReturnError(SetPendingDataFabricIndex(fabricIndexToUse), CHIP_ERROR_INCORRECT_STATE);

    CHIP_ERROR err =
        AddOrUpdateInner(kUndefinedFabricIndex, existingOpKey, isExistingOpKeyExternallyOwned, vendorId, outNewFabricIndex);
    if (err != CHIP_NO_ERROR)
    {
        // Revert partial state added on error
        mOpCertStore->RevertPendingOpCertsExceptRoot();
        return err;
    }

    if (fabricIndexToUse != *outNewFabricIndex)
    {
        ChipLogError(FabricProvisioning,
                     "Fabric addition inconsistency! Determined we needed to add index 0x%x but added 0x%x. Reverting!",
                     static_cast<unsigned>(fabricIndexToUse), static_cast<unsigned>(*outNewFabricIndex));
        RevertPendingFabricData();

        // After reverting, let's fatal if possible, as this should never happen.
        VerifyOrDie(fabricIndexToUse == *outNewFabricIndex);

        return CHIP_ERROR_INTERNAL;
    }

    mStateFlags.Set(StateFlags::kIsAddPending);
    mStateFlags.Set(StateFlags::kIsPendingFabricDataPresent);

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::UpdatePendingFabricCommon(FabricIndex fabricIndex, const ByteSpan & noc, const ByteSpan & icac,
                                                  Crypto::P256Keypair * existingOpKey, bool isExistingOpKeyExternallyOwned)
{
    VerifyOrReturnError(mOpCertStore != nullptr, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(IsValidFabricIndex(fabricIndex), CHIP_ERROR_INVALID_ARGUMENT);

    if (existingOpKey == nullptr)
    {
        // If existing operational key not provided, we need to have a keystore present.
        // It should already have an operational key pending.
        VerifyOrReturnError(mOperationalKeystore != nullptr, CHIP_ERROR_KEY_NOT_FOUND);
        // Make sure we have an operational key, pending or not
        VerifyOrReturnError(mOperationalKeystore->HasOpKeypairForFabric(fabricIndex) || mOperationalKeystore->HasPendingOpKeypair(),
                            CHIP_ERROR_KEY_NOT_FOUND);
    }

    // We should should not have a pending root when we get here, since we can't update root on update
    VerifyOrReturnError(!mStateFlags.Has(StateFlags::kIsTrustedRootPending), CHIP_ERROR_INCORRECT_STATE);

    // We should not have pending add when we get here, due to internal interlocks
    VerifyOrReturnError(!mStateFlags.Has(StateFlags::kIsAddPending), CHIP_ERROR_INCORRECT_STATE);

    // Make sure we are updating at least an existing FabricIndex
    const auto * fabricInfo = FindFabricWithIndex(fabricIndex);
    ReturnErrorCodeIf(fabricInfo == nullptr, CHIP_ERROR_INVALID_FABRIC_INDEX);

    // Check for an existing fabric matching RCAC and FabricID. We must find a correct
    // existing fabric that chains to same root. We assume the stored root is correct.
    {
        FabricIndex collidingFabricIndex = kUndefinedFabricIndex;
        ReturnErrorOnFailure(FindExistingFabricByNocChaining(fabricIndex, noc, collidingFabricIndex));
        ReturnErrorCodeIf(collidingFabricIndex != fabricIndex, CHIP_ERROR_INVALID_FABRIC_INDEX);
    }

    // Handle the temp insert of NOC/ICAC
    ReturnErrorOnFailure(mOpCertStore->UpdateOpCertsForFabric(fabricIndex, noc, icac));
    VerifyOrReturnError(SetPendingDataFabricIndex(fabricIndex), CHIP_ERROR_INCORRECT_STATE);

    FabricIndex newFabricIndex = kUndefinedFabricIndex;
    CHIP_ERROR err =
        AddOrUpdateInner(fabricIndex, existingOpKey, isExistingOpKeyExternallyOwned, fabricInfo->GetVendorId(), &newFabricIndex);
    if (err != CHIP_NO_ERROR)
    {
        // Revert partial state added on error
        mOpCertStore->RevertPendingOpCertsExceptRoot();
        return err;
    }

    if (fabricIndex != newFabricIndex)
    {
        ChipLogError(FabricProvisioning,
                     "Fabric update inconsistency! Determined we needed to update index 0x%x but added 0x%x. Reverting!",
                     static_cast<unsigned>(fabricIndex), static_cast<unsigned>(newFabricIndex));
        RevertPendingFabricData();

        // After reverting, let's fatal if possible, as this should never happen.
        VerifyOrDie(fabricIndex == newFabricIndex);

        return CHIP_ERROR_INTERNAL;
    }

    mStateFlags.Set(StateFlags::kIsUpdatePending);
    mStateFlags.Set(StateFlags::kIsPendingFabricDataPresent);

    // Notify that NOC was updated (at least transiently)
    NotifyFabricUpdated(fabricIndex);

    return CHIP_NO_ERROR;
}

CHIP_ERROR FabricTable::CommitPendingFabricData()
{
    VerifyOrReturnError((mStorage != nullptr) && (mOpCertStore != nullptr), CHIP_ERROR_INCORRECT_STATE);

    bool haveNewTrustedRoot      = mStateFlags.Has(StateFlags::kIsTrustedRootPending);
    bool isAdding                = mStateFlags.Has(StateFlags::kIsAddPending);
    bool isUpdating              = mStateFlags.Has(StateFlags::kIsUpdatePending);
    bool hasPending              = mStateFlags.Has(StateFlags::kIsPendingFabricDataPresent);
    bool hasInvalidInternalState = hasPending && (!IsValidFabricIndex(mFabricIndexWithPendingState) || !(isAdding || isUpdating));

    FabricIndex fabricIndex = mFabricIndexWithPendingState;

    // Proceed with Update/Add pre-flight checks
    if (hasPending && !hasInvalidInternalState)
    {
        if ((isAdding && isUpdating) || (isAdding && !haveNewTrustedRoot))
        {
            ChipLogError(FabricProvisioning, "Found inconsistent interlocks during commit %u/%u/%u!",
                         static_cast<unsigned>(isAdding), static_cast<unsigned>(isUpdating),
                         static_cast<unsigned>(haveNewTrustedRoot));
            hasInvalidInternalState = true;
        }
    }

    // Make sure we actually have a pending fabric
    FabricInfo * pendingFabricEntry = FindFabricWithIndex(fabricIndex);

    if (isUpdating && hasPending && !hasInvalidInternalState)
    {
        if (!mPendingFabric.IsInitialized() || (mPendingFabric.GetFabricIndex() != fabricIndex) || (pendingFabricEntry == nullptr))
        {
            ChipLogError(FabricProvisioning, "Missing pending fabric on update during commit!");
            hasInvalidInternalState = true;
        }
    }

    if (isAdding && hasPending && !hasInvalidInternalState)
    {
        bool opCertStoreHasRoot = mOpCertStore->HasCertificateForFabric(fabricIndex, CertChainElement::kRcac);
        if (!mStateFlags.Has(StateFlags::kIsTrustedRootPending) || !opCertStoreHasRoot)
        {
            ChipLogError(FabricProvisioning, "Missing trusted root for fabric add during commit!");
            hasInvalidInternalState = true;
        }
    }

    if ((isAdding || isUpdating) && hasPending && !hasInvalidInternalState)
    {
        if (!HasOperationalKeyForFabric(fabricIndex))
        {
            ChipLogError(FabricProvisioning, "Could not find an operational key during commit!");
            hasInvalidInternalState = true;
        }
    }

    // If there was nothing pending, we are either in a completely OK state, or weird internally inconsistent
    // state. In either case, let's clear all pending state anyway, in case it was partially stale!
    if (!hasPending || hasInvalidInternalState)
    {
        CHIP_ERROR err = CHIP_NO_ERROR;
        if (hasInvalidInternalState)
        {
            ChipLogError(FabricProvisioning, "Failed to commit: internally inconsistent state!");
            err = CHIP_ERROR_INTERNAL;
        }
        else if (haveNewTrustedRoot)
        {
            ChipLogError(FabricProvisioning,
                         "Failed to commit: tried to commit with only a new trusted root cert. No data committed.");
            hasInvalidInternalState = true;
            err                     = CHIP_ERROR_INCORRECT_STATE;
        }

        // Clear all pending state anyway, in case it was partially stale!
        {
            mStateFlags.ClearAll();
            mFabricIndexWithPendingState = kUndefinedFabricIndex;
            mPendingFabric.Reset();
            mOpCertStore->RevertPendingOpCerts();
            if (mOperationalKeystore != nullptr)
            {
                mOperationalKeystore->RevertPendingKeypair();
            }
        }

        return err;
    }

    // ==== Start of actual commit transaction after pre-flight checks ====

    CHIP_ERROR stickyError = CHIP_NO_ERROR;
    {
        // This scope block is to illustrate the complete commit transaction
        // state. We can see it contains a LARGE number of items...

        // Atomically assume data no longer pending, since we are commit it. Do so here
        // so that FindFabricBy* will return real data and never pending.
        mStateFlags.Clear(StateFlags::kIsPendingFabricDataPresent);

        if (isUpdating)
        {
            // This will get the non-pending fabric
            FabricInfo * existingFabricToUpdate = FindFabricWithIndex(fabricIndex);

            // Multiple interlocks validated the below, so it's fatal if we are somehow incoherent here
            VerifyOrDie((existingFabricToUpdate != nullptr) && (existingFabricToUpdate != &mPendingFabric));

            // Commit the pending entry to local in-memory fabric metadata, which
            // also moves operational keys if not backed by OperationalKeystore
            *existingFabricToUpdate = std::move(mPendingFabric);
        }

        // Store pending metadata first
        FabricInfo * liveFabricEntry = FindFabricWithIndex(fabricIndex);
        VerifyOrDie(liveFabricEntry != nullptr);

        CHIP_ERROR metadataErr = StoreFabricMetadata(liveFabricEntry);
        if (metadataErr != CHIP_NO_ERROR)
        {
            ChipLogError(FabricProvisioning, "Failed to commit pending fabric metadata: %" CHIP_ERROR_FORMAT, metadataErr.Format());
        }
        stickyError = (stickyError != CHIP_NO_ERROR) ? stickyError : metadataErr;

        // We can only manage commissionable pending fail-safe state if we have a keystore
        CHIP_ERROR keyErr = CHIP_NO_ERROR;
        if ((mOperationalKeystore != nullptr) && mOperationalKeystore->HasOpKeypairForFabric(fabricIndex) &&
            mOperationalKeystore->HasPendingOpKeypair())
        {
            keyErr = mOperationalKeystore->CommitOpKeypairForFabric(fabricIndex);
            if (keyErr != CHIP_NO_ERROR)
            {
                ChipLogError(FabricProvisioning, "Failed to commit pending operational keypair %" CHIP_ERROR_FORMAT,
                             keyErr.Format());
                mOperationalKeystore->RevertPendingKeypair();
            }
        }
        stickyError = (stickyError != CHIP_NO_ERROR) ? stickyError : keyErr;

        // Commit operational certs
        CHIP_ERROR opCertErr = mOpCertStore->CommitOpCertsForFabric(fabricIndex);
        if (opCertErr != CHIP_NO_ERROR)
        {
            ChipLogError(FabricProvisioning, "Failed to commit pending operational certificates %" CHIP_ERROR_FORMAT,
                         opCertErr.Format());
            mOpCertStore->RevertPendingOpCerts();
        }
        stickyError = (stickyError != CHIP_NO_ERROR) ? stickyError : opCertErr;

        // Update failure of Last Known Good Time is non-fatal.  If Last
        // Known Good Time is unknown during incoming certificate validation
        // for CASE and current time is also unknown, the certificate
        // validity policy will see this condition and can act appropriately.
        mLastKnownGoodTime.UpdateLastKnownGoodChipEpochTime(mPendingLastKnownGoodTime);

        CHIP_ERROR lkgtErr = CommitLastKnownGoodChipEpochTime();
        if (lkgtErr != CHIP_NO_ERROR)
        {
            // Log but this is not sticky...
            ChipLogError(FabricProvisioning, "Failed to commit Last Known Good Time: %" CHIP_ERROR_FORMAT, lkgtErr.Format());
        }

        // If an Add occurred, let's update the fabric index
        CHIP_ERROR fabricIndexErr = CHIP_NO_ERROR;
        if (mStateFlags.Has(StateFlags::kIsAddPending))
        {
            UpdateNextAvailableFabricIndex();
            fabricIndexErr = StoreFabricIndexInfo();
            if (fabricIndexErr != CHIP_NO_ERROR)
            {
                ChipLogError(FabricProvisioning, "Failed to commit pending fabric indices: %" CHIP_ERROR_FORMAT,
                             fabricIndexErr.Format());
            }
        }
        stickyError = (stickyError != CHIP_NO_ERROR) ? stickyError : fabricIndexErr;
    }

    FabricIndex previouslyPendingFabricIndex = fabricIndex;

    // Must have same side-effect as reverting all pending data
    mStateFlags.ClearAll();
    mFabricIndexWithPendingState = kUndefinedFabricIndex;
    mPendingFabric.Reset();

    if (stickyError != CHIP_NO_ERROR)
    {
        // Blow-away everything if we got past any storage, even on Update: system state is broken
        // TODO: Develop a way to properly revert in the future, but this is very difficult
        Delete(previouslyPendingFabricIndex);

        RevertPendingFabricData();
    }
    else
    {
        NotifyFabricCommitted(fabricIndex);
    }

    return stickyError;
}

void FabricTable::RevertPendingFabricData()
{
    // Will clear pending UpdateNoc/AddNOC
    RevertPendingOpCertsExceptRoot();

    if (mOperationalKeystore != nullptr)
    {
        mOperationalKeystore->RevertPendingKeypair();
    }

    // Clear everything else
    if (mOpCertStore != nullptr)
    {
        mOpCertStore->RevertPendingOpCerts();
    }

    mStateFlags.ClearAll();
    mFabricIndexWithPendingState = kUndefinedFabricIndex;
}

void FabricTable::RevertPendingOpCertsExceptRoot()
{
    mPendingFabric.Reset();

    if (mStateFlags.Has(StateFlags::kIsPendingFabricDataPresent))
    {
        ChipLogError(FabricProvisioning, "Reverting pending fabric data for fabric 0x%u",
                     static_cast<unsigned>(mFabricIndexWithPendingState));
    }

    if (mOpCertStore != nullptr)
    {
        mOpCertStore->RevertPendingOpCertsExceptRoot();
    }

    if (mStateFlags.Has(StateFlags::kIsAddPending))
    {
        // If we have a pending add, let's make sure to kill the pending fabric metadata and return it to viable state.
        Delete(mFabricIndexWithPendingState);
    }

    mStateFlags.Clear(StateFlags::kIsAddPending);
    mStateFlags.Clear(StateFlags::kIsUpdatePending);
    if (!mStateFlags.Has(StateFlags::kIsTrustedRootPending))
    {
        mFabricIndexWithPendingState = kUndefinedFabricIndex;
    }
}

CHIP_ERROR FabricTable::SetFabricLabel(FabricIndex fabricIndex, const CharSpan & fabricLabel)
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(IsValidFabricIndex(fabricIndex), CHIP_ERROR_INVALID_ARGUMENT);

    ReturnErrorCodeIf(fabricLabel.size() > kFabricLabelMaxLengthInBytes, CHIP_ERROR_INVALID_ARGUMENT);

    FabricInfo * fabricInfo  = FindFabricWithIndex(fabricIndex);
    bool fabricIsInitialized = (fabricInfo != nullptr) && fabricInfo->IsInitialized();
    VerifyOrReturnError(fabricIsInitialized, CHIP_ERROR_INCORRECT_STATE);

    // Update fabric tabel in-memory entry, whether pending or not
    ReturnErrorOnFailure(fabricInfo->SetFabricLabel(fabricLabel));

    if (!mStateFlags.HasAny(StateFlags::kIsAddPending, StateFlags::kIsUpdatePending) && (fabricInfo != &mPendingFabric))
    {
        // Nothing is pending, we have to store immediately.
        ReturnErrorOnFailure(StoreFabricMetadata(fabricInfo));
    }

    return CHIP_NO_ERROR;
}

} // namespace chip
