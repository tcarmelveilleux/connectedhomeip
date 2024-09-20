// Copyright 2024 Google. All rights reserved.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <include/platform/DeviceInstanceInfoProvider.h>
#include <include/platform/CommissionableDataProvider.h>

#include <lib/core/CHIPError.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/Span.h>

#ifndef GOOGLE_DEVICE_HARDWARE
#define GOOGLE_DEVICE_HARDWARE "GMD-GENERIC"
#endif // GOOGLE_DEVICE_HARDWARE

namespace google {
namespace matter {

class GoogleMultiDeviceInfoProvider : public chip::DeviceLayer::DeviceInstanceInfoProvider, public chip::DeviceLayer::CommissionableDataProvider
{
  public:
    // =============== DeviceInstanceInfoProvider interface =============
    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, "Google LLC");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetVendorId(uint16_t & vendorId) override
    {
        vendorId = 0x6006u;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, "Google Multi-Device");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductId(uint16_t & productId) override
    {
        productId = 0xFFFEu;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, "GMD-1.4");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, "https://google.com");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, "GMD-1.4");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, "0");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override
    {
        year = 2024u;
        month = 6u;
        day = 17u;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override
    {
        hardwareVersion = 1;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override
    {
        chip::Platform::CopyString(buf, bufSize, GOOGLE_DEVICE_HARDWARE);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan & uniqueIdSpan) override
    {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    // =============== CommissionableDataProvider interface =============
    CHIP_ERROR GetSetupDiscriminator(uint16_t & setupDiscriminator) override
    {
        setupDiscriminator = mDiscriminator;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override
    {
        mDiscriminator = setupDiscriminator;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetSpake2pIterationCount(uint32_t & iterationCount) override
    {
        return mPreviousCommissionableDataProvider->GetSpake2pIterationCount(iterationCount);
    }

    CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan & saltBuf) override
    {
        return mPreviousCommissionableDataProvider->GetSpake2pSalt(saltBuf);
    }

    CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan & verifierBuf, size_t & outVerifierLen) override
    {
        return mPreviousCommissionableDataProvider->GetSpake2pVerifier(verifierBuf, outVerifierLen);
    }

    CHIP_ERROR GetSetupPasscode(uint32_t & setupPasscode) override
    {
        setupPasscode = 20202021;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override
    {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    // =============== Other public interface methods =============
    void InitCommissionableDataProvider()
    {
        if (mPreviousCommissionableDataProvider == nullptr)
        {
            mPreviousCommissionableDataProvider = chip::DeviceLayer::GetCommissionableDataProvider();
            chip::DeviceLayer::SetCommissionableDataProvider(this);
        }
    }

private:
    chip::DeviceLayer::CommissionableDataProvider * mPreviousCommissionableDataProvider = nullptr;
    uint16_t mDiscriminator = 2994;
};

} // namespace matter
} // namespace google
