/*
 *
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

#include "LinuxCommissionableDataProvider.h"

#include <string.h>

#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/Span.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip::Crypto;

namespace {

CHIP_ERROR GeneratePaseSalt(std::vector<uint8_t> & paseSaltVector)
{
    constexpr size_t kSaltLen = kSpake2p_Max_PBKDF_Salt_Length;
    paseSaltVector.resize(kSaltLen);
    return DRBG_get_bytes(paseSaltVector.data(), paseSaltVector.size());
}

} // namespace

CHIP_ERROR LinuxCommissionableDataProvider::Init(
  chip::Optional<std::vector<uint8_t>> serializedPaseVerifier,
  chip::Optional<std::vector<uint8_t>> paseSalt,
  uint32_t paseIterationCount,
  chip::Optional<uint32_t> setupPasscode,
  uint16_t discriminator
)
{
    VerifyOrReturnError(mIsInitialized == false, CHIP_ERROR_INCORRECT_STATE);

    if (discriminator > 0xFFF)
    {
        ChipLogError(Support, "Discriminator value invalid: %u", static_cast<unsigned>(discriminator));
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if ((paseIterationCount < kSpake2p_Min_PBKDF_Iterations) || (paseIterationCount > kSpake2p_Max_PBKDF_Iterations))
    {
        ChipLogError(Support, "PASE Iteration count invalid: %u", static_cast<unsigned>(paseIterationCount));
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    bool havePaseVerifier = serializedPaseVerifier.HasValue();
    Spake2pVerifier providedVerifier;
    CHIP_ERROR err;
    std::vector<uint8_t> finalSerializedVerifier(kSpake2p_VerifierSerialized_Length);
    if (havePaseVerifier)
    {
        if (serializedPaseVerifier.Value().size() != kSpake2p_VerifierSerialized_Length)
        {
            ChipLogError(Support, "PASE verifier size invalid: %u", static_cast<unsigned>(serializedPaseVerifier.Value().size()));
            return CHIP_ERROR_INVALID_ARGUMENT;
        }

        chip::MutableByteSpan verifierSpan{serializedPaseVerifier.Value().data(), serializedPaseVerifier.Value().size()};
        err = providedVerifier.Deserialize(verifierSpan);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Support, "Failed to deserialized PASE verifier: %" CHIP_ERROR_FORMAT, err.Format());
            return err;
        }

        ChipLogProgress(Support, "Got externally provided verifier, using it.");
    }

    bool havePaseSalt = paseSalt.HasValue();
    if (havePaseVerifier && !havePaseSalt)
    {
        ChipLogError(Support, "LinuxCommissionableDataProvider didn't get a PASE salt, but got a verifier: ambiguous data");
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    size_t paseSaltLength = havePaseSalt ? paseSalt.Value().size() : 0;
    if (havePaseSalt && ((paseSaltLength < kSpake2p_Min_PBKDF_Salt_Length) || (paseSaltLength > kSpake2p_Max_PBKDF_Salt_Length)))
    {
        ChipLogError(Support, "PASE salt length invalid: %u", static_cast<unsigned>(paseSaltLength));
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    else if (!havePaseSalt)
    {
        ChipLogProgress(Support, "LinuxCommissionableDataProvider didn't get a PASE salt, generating one.");
        std::vector<uint8_t> paseSaltVector;
        err = GeneratePaseSalt(paseSaltVector);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Support, "Failed to generate PASE salt: %" CHIP_ERROR_FORMAT, err.Format());
            return err;
        }
        paseSalt.SetValue(std::move(paseSaltVector));
    }

    bool havePasscode = setupPasscode.HasValue();
    Spake2pVerifier passcodeVerifier;
    std::vector<uint8_t> serializedPasscodeVerifier(kSpake2p_VerifierSerialized_Length);
    chip::MutableByteSpan saltSpan{paseSalt.Value().data(), paseSalt.Value().size()};
    if (havePasscode)
    {
        err = passcodeVerifier.Generate(paseIterationCount, saltSpan, setupPasscode.Value());
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Support, "Failed to generate PASE verifier from passcode: %" CHIP_ERROR_FORMAT, err.Format());
            return err;
        }

        chip::MutableByteSpan verifierSpan{serializedPasscodeVerifier.data(), serializedPasscodeVerifier.size()};
        err = passcodeVerifier.Serialize(verifierSpan);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Support, "Failed to serialize PASE verifier from passcode: %" CHIP_ERROR_FORMAT, err.Format());
            return err;
        }
    }

    // Make sure we actually have a verifier
    if (!havePasscode && !havePaseVerifier)
    {
        ChipLogError(Support, "Missing both externally provided verifier and passcode: cannot produce final verifier");
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // If both passcode and external verifier were provided, validate they match, otherwise
    // it's ambiguous.
    if (havePasscode && havePaseVerifier)
    {
        if (serializedPasscodeVerifier != serializedPaseVerifier.Value())
        {
            ChipLogError(Support, "Mismatching verifier between passcode and external verifier. Validate inputs.");
            return CHIP_ERROR_INVALID_ARGUMENT;
        }
        ChipLogProgress(Support, "Validated externally provided passcode matches the one generated from provided passcode.");
    }

    // External PASE verifier takes precedence when present (even though it is identical to passcode-based
    // one when the latter is present).
    if (havePaseVerifier)
    {
        finalSerializedVerifier = serializedPaseVerifier.Value();
    }
    else
    {
        finalSerializedVerifier = serializedPasscodeVerifier;
    }

    mDiscriminator = discriminator;
    mSerializedPaseVerifier = std::move(finalSerializedVerifier);
    mPaseSalt = std::move(paseSalt.Value());
    mPaseIterationCount = paseIterationCount;
    if (havePasscode)
    {
        mSetupPasscode.SetValue(setupPasscode.Value());
    }
    mIsInitialized = true;

    return CHIP_NO_ERROR;
}

CHIP_ERROR LinuxCommissionableDataProvider::GetSetupDiscriminator(uint16_t & setupDiscriminator)
{
    VerifyOrReturnError(mIsInitialized == true, CHIP_ERROR_INCORRECT_STATE);
    setupDiscriminator = mDiscriminator;
    return CHIP_NO_ERROR;
}

CHIP_ERROR LinuxCommissionableDataProvider::GetSpake2pIterationCount(uint32_t & iterationCount)
{
    VerifyOrReturnError(mIsInitialized == true, CHIP_ERROR_INCORRECT_STATE);
    iterationCount = mPaseIterationCount;
    return CHIP_NO_ERROR;
}

CHIP_ERROR LinuxCommissionableDataProvider::GetSpake2pSalt(chip::MutableByteSpan & saltBuf, size_t & outSaltLen)
{
    VerifyOrReturnError(mIsInitialized == true, CHIP_ERROR_INCORRECT_STATE);

    outSaltLen = mPaseSalt.size();
    VerifyOrReturnError(saltBuf.size() >= outSaltLen, CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy(saltBuf.data(), mPaseSalt.data(), mPaseSalt.size());
    saltBuf.reduce_size(mPaseSalt.size());

    return CHIP_NO_ERROR;
}

CHIP_ERROR LinuxCommissionableDataProvider::GetSpake2pVerifier(chip::MutableByteSpan & verifierBuf, size_t & outVerifierLen)
{
    VerifyOrReturnError(mIsInitialized == true, CHIP_ERROR_INCORRECT_STATE);

    // By now, serialized verifier from Init should be correct size
    VerifyOrReturnError(mSerializedPaseVerifier.size() == kSpake2p_VerifierSerialized_Length, CHIP_ERROR_INTERNAL);

    outVerifierLen = mSerializedPaseVerifier.size();
    VerifyOrReturnError(verifierBuf.size() >= outVerifierLen, CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy(verifierBuf.data(), mSerializedPaseVerifier.data(), mSerializedPaseVerifier.size());
    verifierBuf.reduce_size(mSerializedPaseVerifier.size());

    return CHIP_NO_ERROR;
}

CHIP_ERROR LinuxCommissionableDataProvider::GetSetupPasscode(uint32_t & setupPasscode)
{
    VerifyOrReturnError(mIsInitialized == true, CHIP_ERROR_INCORRECT_STATE);

    // Pretend not implemented if we don't have a passcode value externally set
    if (!mSetupPasscode.HasValue())
    {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    setupPasscode = mSetupPasscode.Value();
    return CHIP_NO_ERROR;
}
