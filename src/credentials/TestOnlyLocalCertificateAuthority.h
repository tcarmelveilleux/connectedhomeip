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
#include <credentials/LocalCertificateAuthority.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>

namespace chip {
namespace Credentials {

/**
 * @brief This class is a basic LocalCertificateAuthority that owns its root keypair, for
 *        unit testing purposes.
 */
class TestOnlyLocalCertificateAuthority : public LocalCertificateAuthority
{
public:
    TestOnlyLocalCertificateAuthority() = default;
    virtual ~TestOnlyLocalCertificateAuthority() {}

    // Non-copyable
    TestOnlyLocalCertificateAuthority(TestOnlyLocalCertificateAuthority const &) = delete;
    void operator=(TestOnlyLocalCertificateAuthority const &) = delete;

    /**
     * @brief Initialize this local test CA with a random root key.
     *
     * `GetStatus()` will return an error code on failure, and `IsSuccess()` will return false
     *
     * @return *this for call chaining.
     */
    CHIP_ERROR Init()
    {
        mCurrentStatus = mInnerRootKeypair.Initialize();
        if (mCurrentStatus != CHIP_NO_ERROR)
        {
            return mCurrentStatus;
        }

        uint32_t kDefaultValiditySeconds = 10 * (365 * 24 * 60 * 60); // 10 years

        // Default to start effective data of 2022 Jan 1
        chip::ASN1::ASN1UniversalTime rootEffectiveTime;
        rootEffectiveTime.Year  = 2022;
        rootEffectiveTime.Month = 1;
        rootEffectiveTime.Day   = 1;

        return LocalCertificateAuthority::Init(&mInnerRootKeypair, rootEffectiveTime, kDefaultValiditySeconds);
    }

    /**
     * @brief Initialize this local test CA with a provided root key pair pass in serialized form.
     *
     * `GetStatus()` will return an error code on failure, and `IsSuccess()` will return false
     *
     * @param rootKeyPair - Serialized root key pair to use. Internal keypair will copy it.
     *
     * @return *this for call chaining.
     */
    CHIP_ERROR Init(Crypto::P256SerializedKeypair & rootKeyPair)
    {
        if (rootKeyPair.Length() != 0)
        {
            mInnerRootKeypair.Initialize();
            mCurrentStatus = mInnerRootKeypair.Deserialize(rootKeyPair);
            return mCurrentStatus;
        }

        uint32_t kDefaultValiditySeconds = 10 * (365 * 24 * 60 * 60); // 10 years

        // Default to start effective data of 2022 Jan 1
        chip::ASN1::ASN1UniversalTime rootEffectiveTime;
        rootEffectiveTime.Year  = 2022;
        rootEffectiveTime.Month = 1;
        rootEffectiveTime.Day   = 1;

        return LocalCertificateAuthority::Init(&mInnerRootKeypair, rootEffectiveTime, kDefaultValiditySeconds);
    }

protected:
    Crypto::P256Keypair mInnerRootKeypair;
};

} // namespace Credentials
} // namespace chip
