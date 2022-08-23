/*
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
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

#pragma once

#include <credentials/CHIPCert.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CASEAuthTag.h>
#include <lib/core/CHIPEncoding.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>

namespace chip {
namespace Credentials {

/**
 * @brief Local operational certificate authority. Generates operational certificate
 *        chains rooted to a given root key pair object.
 *
 * The Root Keypair is **externally owned** and the caller has to manage the lifecycle.
 * While instances of this class are live, the root key pair must be available. This is
 * done to support wrapped P256Keypair objects that implement different versions of
 * their internals and that cannot be copied.
 *
 * This class owns the memory for the generated certificates so that the followign methods
 * can safely return spans after successful operations:
 *
 * - GetNoc/GetIcac/GetRcac --> Return Matter Operational Certificate Encoding (TLV) format
 * - GetDerNoc/GetDerIcac/GetDerRcac --> Return ASN.1 DER X.509 format
 *
 */
class LocalCertificateAuthority
{
public:
    LocalCertificateAuthority() = default;

    virtual ~LocalCertificateAuthority()
    {
        mRootKeypair = nullptr;
        mDerRcac.Free();
        mTlvRcac.Free();
        mDerIcac.Free();
        mTlvIcac.Free();
        mDerNoc.Free();
        mTlvNoc.Free();
    }

    // Non-copyable
    LocalCertificateAuthority(LocalCertificateAuthority const &) = delete;
    void operator=(LocalCertificateAuthority const &) = delete;

    // TODO: Allow passing existing root cert
    CHIP_ERROR Init(Crypto::P256Keypair * rootKeypair, chip::ASN1::ASN1UniversalTime effectiveTime,
                                     uint32_t validitySeconds)
    {
        uint8_t hashBuf[Crypto::kSHA256_Hash_Length];
        Crypto::P256PublicKey rootPublicKey;
        uint64_t rootIdentifier              = 0;
        uint32_t matterEffectiveTime         = 0;
        constexpr uint32_t kOneHourAsSeconds = (60 * 60);

        VerifyOrExit(rootKeypair != nullptr, mCurrentStatus = CHIP_ERROR_INVALID_ARGUMENT);
        VerifyOrExit(validitySeconds >= kOneHourAsSeconds, mCurrentStatus = CHIP_ERROR_INVALID_ARGUMENT);

        mDerRcac.Free();
        mTlvRcac.Free();
        mRootCertAvailable = false;

        // Truncated SHA256 of root public key is going to be the RCAC ID
        rootPublicKey = rootKeypair->Pubkey();
        SuccessOrExit(Crypto::Hash_SHA256(rootPublicKey.ConstBytes(), rootPublicKey.Length(), &hashBuf[0]));
        rootIdentifier = Encoding::BigEndian::Get64(&hashBuf[0]);

        mRootKeypair = rootKeypair;

        mCurrentStatus = ASN1ToChipEpochTime(effectiveTime, matterEffectiveTime);
        SuccessOrExit(mCurrentStatus);

        mMatterEffectiveTime = matterEffectiveTime;
        mValiditySeconds     = validitySeconds;

        mCurrentStatus = GenerateRootCert(rootIdentifier);
        SuccessOrExit(mCurrentStatus);
        mRootCertAvailable = true;
    exit:
        return mCurrentStatus;
    }

    /**
     * @brief Set the validity period for the next GenerateNocChain call.
     *
     * @param effectiveTime - notBefore date to use
     * @param validitySeconds - validaty period in seconds
     *
     * @return *this for call chaining.
     */
    CHIP_ERROR SetValidity(chip::ASN1::ASN1UniversalTime effectiveTime, uint32_t validitySeconds)
    {
        constexpr uint32_t kOneHourAsSeconds = (60 * 60);

        if (validitySeconds < kOneHourAsSeconds)
        {
            mCurrentStatus = CHIP_ERROR_INVALID_ARGUMENT;
        }
        else
        {
            uint32_t matterEffectiveTime = 0;
            mCurrentStatus               = ASN1ToChipEpochTime(effectiveTime, matterEffectiveTime);
            VerifyOrReturnValue(mCurrentStatus == CHIP_NO_ERROR, mCurrentStatus);

            mValiditySeconds     = validitySeconds;
            mMatterEffectiveTime = matterEffectiveTime;
        }

        return mCurrentStatus;
    }

    /**
     * @brief Sets whether the next GenerateNocChain call has an ICAC included in the chain
     *
     * @param includeIcac - include an ICAC if true, omit it if false
     * @return *this for call chaining.
     */
    CHIP_ERROR SetIncludeIcac(bool includeIcac)
    {
        mIncludeIcac   = includeIcac;
        mCurrentStatus = (mCurrentStatus != CHIP_NO_ERROR) ? mCurrentStatus : CHIP_NO_ERROR;
        return mCurrentStatus;
    }

    void ResetIssuer()
    {
        if (mRootCertAvailable)
        {
            mCurrentStatus = CHIP_NO_ERROR;
            mIncludeIcac   = false;
            mDerNoc.Free();
            mTlvNoc.Free();
            mDerIcac.Free();
            mTlvIcac.Free();
        }
    }

    /**
     * @brief Return the status of the last operation.
     *
     * @return A CHIP_ERROR value
     */
    CHIP_ERROR GetStatus() { return mCurrentStatus; }

    /**
     * @return true if the last operation was successful (i.e. GetStatus() == CHIP_NO_ERROR)
     */
    bool IsSuccess() { return mCurrentStatus == CHIP_NO_ERROR; }

    /**
     * @brief Get span pointing to internally-owned TLV version of last generated NOC
     *
     * Return value is empty on error or if not yet initialized.
     *
     * @return a bytespan pointing to internal memory
     */
    ByteSpan GetNoc() const
    {
        VerifyOrReturnValue((mCurrentStatus == CHIP_NO_ERROR) && (mTlvNoc.Get() != nullptr) && mRootCertAvailable, ByteSpan{});
        return ByteSpan{ mTlvNoc.Get(), mTlvNoc.AllocatedSize() };
    }

    /**
     * @brief Get span pointing to internally-owned TLV version of last generated ICAC
     *
     * Return value is empty on error, if not yet initialized or if no ICAC was generated/used
     *
     * @return a bytespan pointing to internal memory
     */
    ByteSpan GetIcac() const
    {
        VerifyOrReturnValue(mIncludeIcac, ByteSpan{});
        VerifyOrReturnValue((mCurrentStatus == CHIP_NO_ERROR) && (mTlvIcac.Get() != nullptr) && mRootCertAvailable, ByteSpan{});
        return ByteSpan{ mTlvIcac.Get(), mTlvIcac.AllocatedSize() };
    }

    /**
     * @brief Get span pointing to internally-owned TLV version of last generated RCAC
     *
     * Return value is empty on error or if not yet initialized
     *
     * @return a bytespan pointing to internal memory
     */
    ByteSpan GetRcac() const
    {
        VerifyOrReturnValue((mCurrentStatus == CHIP_NO_ERROR) && (mTlvRcac.Get() != nullptr) && mRootCertAvailable, ByteSpan{});
        return ByteSpan{ mTlvRcac.Get(), mTlvRcac.AllocatedSize() };
    }

    /**
     * @brief Get span pointing to internally-owned DER version of last generated NOC
     *
     * Return value is empty on error or if not yet initialized.
     *
     * @return a bytespan pointing to internal memory
     */
    ByteSpan GetDerNoc() const
    {
        VerifyOrReturnValue((mCurrentStatus == CHIP_NO_ERROR) && (mDerNoc.Get() != nullptr) && mRootCertAvailable, ByteSpan{});
        return ByteSpan{ mDerNoc.Get(), mDerNoc.AllocatedSize() };
    }

    /**
     * @brief Get span pointing to internally-owned DER version of last generated ICAC
     *
     * Return value is empty on error, if not yet initialized or if no ICAC was generated/used
     *
     * @return a bytespan pointing to internal memory
     */
    ByteSpan GetDerIcac() const
    {
        VerifyOrReturnValue(mIncludeIcac, ByteSpan{});
        VerifyOrReturnValue((mCurrentStatus == CHIP_NO_ERROR) && (mDerIcac.Get() != nullptr) && mRootCertAvailable, ByteSpan{});
        return ByteSpan{ mDerIcac.Get(), mDerIcac.AllocatedSize() };
    }

    /**
     * @brief Get span pointing to internally-owned DER version of last generated RCAC
     *
     * Return value is empty on error or if not yet initialized
     *
     * @return a bytespan pointing to internal memory
     */
    ByteSpan GetDerRcac() const
    {
        VerifyOrReturnValue((mCurrentStatus == CHIP_NO_ERROR) && (mDerRcac.Get() != nullptr) && mRootCertAvailable, ByteSpan{});
        return ByteSpan{ mDerRcac.Get(), mDerRcac.AllocatedSize() };
    }

    /**
     * @brief Generate a NOC chain for a given identity, given the subject public key.
     *
     * If `SetIncludeIcac(true)` had been called, an ICAC will be generated with a random
     * key, and with an ICA ID containing the first 64 bits of the SHA256 of the ICA subject
     * public key. This policy is mostly useful just for testing.
     *
     * notBefore date and validity period will match that given at time of Init or set by the last
     * call to SetValidity.
     *
     * On failure, `GetStatus()` will return an error code, and `IsSuccess()` will return false.
     *
     * @param fabricId - fabric ID to use
     * @param nodeId - node ID to use
     * @param cats - cat tags to include
     * @param nocPublicKey - subject public key for the NOC
     *
     * @return *this for call chaining.
     */
    virtual CHIP_ERROR GenerateNocChain(FabricId fabricId, NodeId nodeId, const CATValues & cats,
                                                         const Crypto::P256PublicKey & nocPublicKey)
    {
        if (mCurrentStatus != CHIP_NO_ERROR)
        {
            return mCurrentStatus;
        }

        if (mRootKeypair == nullptr)
        {
            mCurrentStatus = CHIP_ERROR_INCORRECT_STATE;
            return mCurrentStatus;
        }

        mTlvIcac.Free();
        mDerIcac.Free();
        mTlvNoc.Free();
        mDerNoc.Free();

        mCurrentStatus = GenerateCertChainInternal(fabricId, nodeId, cats, nocPublicKey);
        return mCurrentStatus;
    }

    /**
     * @brief Same as GenerateNocChain above but with no CAT tags included
     */
    virtual CHIP_ERROR GenerateNocChain(FabricId fabricId, NodeId nodeId,
                                                         const Crypto::P256PublicKey & nocPublicKey)
    {
        return GenerateNocChain(fabricId, nodeId, kUndefinedCATs, nocPublicKey);
    }

    /**
     * @brief Generate a NOC chain for a given identity, given a CSR for the subject public key.
     *
     * If `SetIncludeIcac(true)` had been called, an ICAC will be generated with a random
     * key, and with an ICA ID containing the first 64 bits of the SHA256 of the ICA subject
     * public key. This policy is mostly useful just for testing.
     *
     * notBefore date and validity period will match that given at time of Init or set by the last
     * call to SetValidity.
     *
     * On failure, `GetStatus()` will return an error code, and `IsSuccess()` will return false.
     *
     * @param fabricId - fabric ID to use
     * @param nodeId - node ID to use
     * @param cats - cat tags to include
     * @param csr - span containing a PKCS10 CSR for the subject public key to use
     *
     * @return *this for call chaining.
     */
    virtual CHIP_ERROR GenerateNocChain(FabricId fabricId, NodeId nodeId, const CATValues & cats,
                                                         const ByteSpan & csr)
    {
        if (mCurrentStatus != CHIP_NO_ERROR)
        {
            return mCurrentStatus;
        }

        Crypto::P256PublicKey nocPublicKey;
        mCurrentStatus = Crypto::VerifyCertificateSigningRequest(csr.data(), csr.size(), nocPublicKey);
        if (mCurrentStatus != CHIP_NO_ERROR)
        {
            return mCurrentStatus;
        }

        return GenerateNocChain(fabricId, nodeId, cats, nocPublicKey);
    }

    /**
     * @brief Same as above but with no CAT tags included.
     */
    virtual CHIP_ERROR GenerateNocChain(FabricId fabricId, NodeId nodeId, const ByteSpan & csr)
    {
        return GenerateNocChain(fabricId, nodeId, kUndefinedCATs, csr);
    }

protected:
    virtual CHIP_ERROR GenerateCertChainInternal(FabricId fabricId, NodeId nodeId, const CATValues & cats,
                                                 const Crypto::P256PublicKey & nocPublicKey)
    {
        ChipDN rcac_dn;
        ChipDN icac_dn;
        ChipDN noc_dn;

        ReturnErrorCodeIf(fabricId == kUndefinedFabricId, CHIP_ERROR_INVALID_ARGUMENT);
        ReturnErrorCodeIf(nodeId == kUndefinedNodeId, CHIP_ERROR_INVALID_ARGUMENT);
        ReturnErrorCodeIf(!cats.AreValid(), CHIP_ERROR_INVALID_ARGUMENT);

        // Get subject DN of RCAC as our issuer field for ICAC and/or NOC depending on if ICAC is present
        ReturnErrorOnFailure(ExtractSubjectDNFromChipCert(ByteSpan{ mTlvRcac.Get(), mTlvRcac.AllocatedSize() }, rcac_dn));

        Crypto::P256Keypair * nocIssuerKeypair = mRootKeypair;
        ChipDN * issuer_dn                     = &rcac_dn;

        Crypto::P256Keypair icacKeypair;

        // Generate ICAC if needed
        if (mIncludeIcac)
        {
            ReturnErrorOnFailure(icacKeypair.Initialize());

            Platform::ScopedMemoryBufferWithSize<uint8_t> derIcacBuf;
            Platform::ScopedMemoryBufferWithSize<uint8_t> tlvIcacBuf;
            ReturnErrorCodeIf(!derIcacBuf.Alloc(Credentials::kMaxDERCertLength), CHIP_ERROR_NO_MEMORY);
            ReturnErrorCodeIf(!tlvIcacBuf.Alloc(Credentials::kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);

            memset(derIcacBuf.Get(), 0, derIcacBuf.AllocatedSize());
            memset(tlvIcacBuf.Get(), 0, tlvIcacBuf.AllocatedSize());

            // ICAC Identifier in subject is first 8 octets of subject public key SHA256
            Crypto::P256PublicKey icacPublicKey = icacKeypair.Pubkey();
            uint8_t hashBuf[Crypto::kSHA256_Hash_Length];
            ReturnErrorOnFailure(Crypto::Hash_SHA256(icacPublicKey.ConstBytes(), icacPublicKey.Length(), &hashBuf[0]));
            uint64_t icacIdentifier = Encoding::BigEndian::Get64(&hashBuf[0]);

            ReturnErrorOnFailure(icac_dn.AddAttribute_MatterFabricId(fabricId));
            ReturnErrorOnFailure(icac_dn.AddAttribute_MatterICACId(icacIdentifier));

            X509CertRequestParams icac_request = { 0, mMatterEffectiveTime, mMatterEffectiveTime + mValiditySeconds, icac_dn,
                                                   rcac_dn };

            MutableByteSpan icacDerSpan{ derIcacBuf.Get(), derIcacBuf.AllocatedSize() };
            ReturnErrorOnFailure(Credentials::NewICAX509Cert(icac_request, icacKeypair.Pubkey(), *mRootKeypair, icacDerSpan));

            MutableByteSpan icacTlvSpan{ tlvIcacBuf.Get(), tlvIcacBuf.AllocatedSize() };
            ReturnErrorOnFailure(Credentials::ConvertX509CertToChipCert(icacDerSpan, icacTlvSpan));

            ReturnErrorCodeIf(!mDerIcac.Alloc(icacDerSpan.size()), CHIP_ERROR_NO_MEMORY);
            ReturnErrorCodeIf(!mTlvIcac.Alloc(icacTlvSpan.size()), CHIP_ERROR_NO_MEMORY);

            memcpy(mDerIcac.Get(), icacDerSpan.data(), icacDerSpan.size());
            memcpy(mTlvIcac.Get(), icacTlvSpan.data(), icacTlvSpan.size());

            nocIssuerKeypair = &icacKeypair;
            issuer_dn        = &icac_dn;
        }
        else
        {
            mDerIcac.Free();
            mTlvIcac.Free();
        }

        // Generate NOC always, either issued from ICAC if present or from RCAC
        {
            Platform::ScopedMemoryBufferWithSize<uint8_t> derNocBuf;
            Platform::ScopedMemoryBufferWithSize<uint8_t> tlvNocBuf;
            ReturnErrorCodeIf(!derNocBuf.Alloc(Credentials::kMaxDERCertLength), CHIP_ERROR_NO_MEMORY);
            ReturnErrorCodeIf(!tlvNocBuf.Alloc(Credentials::kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);

            memset(derNocBuf.Get(), 0, derNocBuf.AllocatedSize());
            memset(tlvNocBuf.Get(), 0, tlvNocBuf.AllocatedSize());

            ReturnErrorOnFailure(noc_dn.AddAttribute_MatterFabricId(fabricId));
            ReturnErrorOnFailure(noc_dn.AddAttribute_MatterNodeId(nodeId));
            ReturnErrorOnFailure(noc_dn.AddCATs(cats));

            X509CertRequestParams noc_request = { 0, mMatterEffectiveTime, mMatterEffectiveTime + mValiditySeconds, noc_dn,
                                                  *issuer_dn };

            MutableByteSpan nocDerSpan{ derNocBuf.Get(), derNocBuf.AllocatedSize() };
            ReturnErrorOnFailure(Credentials::NewNodeOperationalX509Cert(noc_request, nocPublicKey, *nocIssuerKeypair, nocDerSpan));

            MutableByteSpan nocTlvSpan{ tlvNocBuf.Get(), tlvNocBuf.AllocatedSize() };
            ReturnErrorOnFailure(Credentials::ConvertX509CertToChipCert(nocDerSpan, nocTlvSpan));

            ReturnErrorCodeIf(!mDerNoc.Alloc(nocDerSpan.size()), CHIP_ERROR_NO_MEMORY);
            ReturnErrorCodeIf(!mTlvNoc.Alloc(nocTlvSpan.size()), CHIP_ERROR_NO_MEMORY);

            memcpy(mDerNoc.Get(), nocDerSpan.data(), nocDerSpan.size());
            memcpy(mTlvNoc.Get(), nocTlvSpan.data(), nocTlvSpan.size());
        }

        return CHIP_NO_ERROR;
    }

    virtual CHIP_ERROR GenerateRootCert(uint64_t rootIdentifier)
    {
        ChipDN rcac_dn;
        rcac_dn.Clear();

        VerifyOrReturnError(mRootKeypair != nullptr, CHIP_ERROR_INCORRECT_STATE);

        Platform::ScopedMemoryBufferWithSize<uint8_t> derRcacBuf;
        Platform::ScopedMemoryBufferWithSize<uint8_t> tlvRcacBuf;
        VerifyOrReturnError(derRcacBuf.Alloc(Credentials::kMaxDERCertLength), CHIP_ERROR_NO_MEMORY);
        VerifyOrReturnError(tlvRcacBuf.Alloc(Credentials::kMaxCHIPCertLength), CHIP_ERROR_NO_MEMORY);

        memset(derRcacBuf.Get(), 0, derRcacBuf.AllocatedSize());
        memset(tlvRcacBuf.Get(), 0, tlvRcacBuf.AllocatedSize());

        ReturnErrorOnFailure(rcac_dn.AddAttribute_MatterRCACId(rootIdentifier));

        X509CertRequestParams rcac_request = { 0, mMatterEffectiveTime, mMatterEffectiveTime + mValiditySeconds, rcac_dn, rcac_dn };

        MutableByteSpan rcacDerSpan{ derRcacBuf.Get(), derRcacBuf.AllocatedSize() };
        ReturnErrorOnFailure(Credentials::NewRootX509Cert(rcac_request, *mRootKeypair, rcacDerSpan));

        MutableByteSpan rcacTlvSpan{ tlvRcacBuf.Get(), tlvRcacBuf.AllocatedSize() };
        ReturnErrorOnFailure(Credentials::ConvertX509CertToChipCert(rcacDerSpan, rcacTlvSpan));

        VerifyOrReturnError(mDerRcac.Alloc(rcacDerSpan.size()), CHIP_ERROR_NO_MEMORY);
        VerifyOrReturnError(mTlvRcac.Alloc(rcacTlvSpan.size()), CHIP_ERROR_NO_MEMORY);

        memcpy(mDerRcac.Get(), rcacDerSpan.data(), rcacDerSpan.size());
        memcpy(mTlvRcac.Get(), rcacTlvSpan.data(), rcacTlvSpan.size());

        return CHIP_NO_ERROR;
    }

    bool mRootCertAvailable = false;

    uint32_t mMatterEffectiveTime = 0;
    uint32_t mValiditySeconds     = 0;

    CHIP_ERROR mCurrentStatus = CHIP_NO_ERROR;
    bool mIncludeIcac         = false;

    Platform::ScopedMemoryBufferWithSize<uint8_t> mDerNoc;
    Platform::ScopedMemoryBufferWithSize<uint8_t> mTlvNoc;
    Platform::ScopedMemoryBufferWithSize<uint8_t> mDerIcac;
    Platform::ScopedMemoryBufferWithSize<uint8_t> mTlvIcac;
    Platform::ScopedMemoryBufferWithSize<uint8_t> mDerRcac;
    Platform::ScopedMemoryBufferWithSize<uint8_t> mTlvRcac;

    Crypto::P256Keypair * mRootKeypair;
};

} // namespace Credentials
} // namespace chip
