/**
 *
 *    Copyright (c) 2020 Project CHIP Authors
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
 *
 */

#include "basic.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/EventLogging.h>
#include <app/util/attribute-storage.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ConfigurationManager.h>
#include <platform/PlatformManager.h>

#include <cstddef>
#include <cstring>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::Basic;
using namespace chip::app::Clusters::Basic::Attributes;
using namespace chip::DeviceLayer;

namespace {

class BasicAttrAccess : public AttributeAccessInterface
{
public:
    // Register for the Basic cluster on all endpoints.
    BasicAttrAccess() : AttributeAccessInterface(Optional<EndpointId>::Missing(), Basic::Id) {}

    CHIP_ERROR Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override;
    CHIP_ERROR Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder) override;

private:
    CHIP_ERROR ReadLocation(AttributeValueEncoder & aEncoder);
    CHIP_ERROR WriteLocation(AttributeValueDecoder & aDecoder);
};

BasicAttrAccess gAttrAccess;

CHIP_ERROR BasicAttrAccess::Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder)
{
    if (aPath.mClusterId != Basic::Id)
    {
        // We shouldn't have been called at all.
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    CHIP_ERROR status = CHIP_ERROR_INTERNAL;

    switch (aPath.mAttributeId)
    {
    case Location::Id:
        status = ReadLocation(aEncoder);
        break;

    case VendorName::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxVendorNameLength;
        char vendorName[kMaxLen + 1];
        status = ConfigurationMgr().GetVendorName(vendorName, sizeof(vendorName));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(vendorName, strnlen(vendorName, kMaxLen)));
        }
        break;
    }

    case VendorID::Id: {
        uint16_t vendorId = 0;
        status = ConfigurationMgr().GetVendorId(vendorId);
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(vendorId);
        }
        break;
    }

    case ProductName::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxProductNameLength;
        char productName[kMaxLen + 1];
        status = ConfigurationMgr().GetProductName(productName, sizeof(productName));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(productName, strnlen(productName, kMaxLen)));
        }
        break;
    }

    case ProductID::Id: {
        uint16_t productId = 0;
        status = ConfigurationMgr().GetProductId(productId);
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(productId);
        }
        break;
    }

    case HardwareVersion::Id: {
        uint16_t hardwareVersion = 0;
        status = ConfigurationMgr().GetHardwareVersion(hardwareVersion);
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(hardwareVersion);
        }
        break;
    }

    case HardwareVersionString::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxHardwareVersionStringLength;
        char hardwareVersionString[kMaxLen + 1];
        status = ConfigurationMgr().GetHardwareVersionString(hardwareVersionString, sizeof(hardwareVersionString));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(hardwareVersionString, strnlen(hardwareVersionString, kMaxLen)));
        }
        break;
    }

    case SoftwareVersion::Id: {
        uint32_t softwareVersion = 0;
        status = ConfigurationMgr().GetSoftwareVersion(softwareVersion);
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(softwareVersion);
        }
        break;
    }

    case SoftwareVersionString::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxSoftwareVersionStringLength;
        char softwareVersionString[kMaxLen + 1];
        status = ConfigurationMgr().GetSoftwareVersionString(softwareVersionString, sizeof(softwareVersionString));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(softwareVersionString, strnlen(softwareVersionString, kMaxLen)));
        }
        break;
    }

    case ManufacturingDate::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxManufacturingDateLength;
        char manufacturingDateString[kMaxLen + 1];
        uint16_t manufacturingYear = 2020;
        uint8_t manufacturingMonth = 1;
        uint8_t manufacturingDayOfMonth = 1;
        status = ConfigurationMgr().GetManufacturingDate(manufacturingYear, manufacturingMonth, manufacturingDayOfMonth);
        if (status == CHIP_NO_ERROR)
        {
            // Format is YYYYMMDD
            snprintf(manufacturingDateString, sizeof(manufacturingDateString), "%04" PRIu16 "%02" PRIu8 "%02" PRIu8,
                     manufacturingYear, manufacturingMonth, manufacturingDayOfMonth);
            status = aEncoder.Encode(chip::CharSpan(manufacturingDateString, strnlen(manufacturingDateString, kMaxLen)));
        }
        else if (status == CHIP_ERROR_NOT_IMPLEMENTED || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            status = CHIP_ERROR_NOT_FOUND;
        }
        break;
    }

    case PartNumber::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxPartNumberLength;
        char partNumber[kMaxLen + 1];
        status = ConfigurationMgr().GetPartNumber(partNumber, sizeof(partNumber));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(partNumber, strnlen(partNumber, kMaxLen)));
        }
        else if (status == CHIP_ERROR_NOT_IMPLEMENTED || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            status = CHIP_ERROR_NOT_FOUND;
        }
        break;
    }

    case ProductURL::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxProductURLLength;
        char productUrl[kMaxLen + 1];
        status = ConfigurationMgr().GetProductURL(productUrl, sizeof(productUrl));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(productUrl, strnlen(productUrl, kMaxLen)));
        }
        else if (status == CHIP_ERROR_NOT_IMPLEMENTED || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            status = CHIP_ERROR_NOT_FOUND;
        }
        break;
    }

    case ProductLabel::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxProductLabelLength;
        char productLabel[kMaxLen + 1];
        status = ConfigurationMgr().GetProductLabel(productLabel, sizeof(productLabel));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(productLabel, strnlen(productLabel, kMaxLen)));
        }
        else if (status == CHIP_ERROR_NOT_IMPLEMENTED || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            status = CHIP_ERROR_NOT_FOUND;
        }
        break;
    }

    case SerialNumber::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxSerialNumberLength;
        char serialNumberString[kMaxLen + 1];
        status = ConfigurationMgr().GetSerialNumber(serialNumberString, sizeof(serialNumberString));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(serialNumberString, strnlen(serialNumberString, kMaxLen)));
        }
        else if (status == CHIP_ERROR_NOT_IMPLEMENTED || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            status = CHIP_ERROR_NOT_FOUND;
        }
        break;
    }

    case UniqueID::Id: {
        constexpr size_t kMaxLen = DeviceLayer::ConfigurationManager::kMaxUniqueIDLength;
        char uniqueId[kMaxLen + 1];
        status = ConfigurationMgr().GetUniqueId(uniqueId, sizeof(uniqueId));
        if (status == CHIP_NO_ERROR)
        {
            status = aEncoder.Encode(chip::CharSpan(uniqueId, strnlen(uniqueId, kMaxLen)));
        }
        else if (status == CHIP_ERROR_NOT_IMPLEMENTED || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            status = CHIP_ERROR_NOT_FOUND;
        }
        break;
    }

    default:
        // We did not find a processing path, the caller will delegate elsewhere.
        return CHIP_ERROR_NOT_FOUND;
    }

    return status;
}

CHIP_ERROR BasicAttrAccess::ReadLocation(AttributeValueEncoder & aEncoder)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    char location[DeviceLayer::ConfigurationManager::kMaxLocationLength + 1];
    size_t codeLen = 0;

    if (ConfigurationMgr().GetCountryCode(location, sizeof(location), codeLen) == CHIP_NO_ERROR)
    {
        if (codeLen == 0)
        {
            err = aEncoder.Encode(chip::CharSpan("XX", strlen("XX")));
        }
        else
        {
            err = aEncoder.Encode(chip::CharSpan(location, strlen(location)));
        }
    }
    else
    {
        err = aEncoder.Encode(chip::CharSpan("XX", strlen("XX")));
    }

    return err;
}

CHIP_ERROR BasicAttrAccess::Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder)
{
    VerifyOrDie(aPath.mClusterId == Basic::Id);

    switch (aPath.mAttributeId)
    {
    case Location::Id:
        return WriteLocation(aDecoder);
    default:
        break;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR BasicAttrAccess::WriteLocation(AttributeValueDecoder & aDecoder)
{
    chip::CharSpan location;

    ReturnErrorOnFailure(aDecoder.Decode(location));
    VerifyOrReturnError(location.size() <= DeviceLayer::ConfigurationManager::kMaxLocationLength,
                        CHIP_ERROR_INVALID_MESSAGE_LENGTH);

    return DeviceLayer::ConfigurationMgr().StoreCountryCode(location.data(), location.size());
}

class PlatformMgrDelegate : public DeviceLayer::PlatformManagerDelegate
{
    // Gets called by the current Node after completing a boot or reboot process.
    void OnStartUp(uint32_t softwareVersion) override
    {
        ChipLogProgress(Zcl, "PlatformMgrDelegate: OnStartUp");

        for (auto endpoint : EnabledEndpointsWithServerCluster(Basic::Id))
        {
            // If Basic cluster is implemented on this endpoint
            Events::StartUp::Type event{ softwareVersion };
            EventNumber eventNumber;

            if (CHIP_NO_ERROR != LogEvent(event, endpoint, eventNumber, EventOptions::Type::kUrgent))
            {
                ChipLogError(Zcl, "PlatformMgrDelegate: Failed to record StartUp event");
            }
        }
    }

    // Gets called by the current Node prior to any orderly shutdown sequence on a best-effort basis.
    void OnShutDown() override
    {
        ChipLogProgress(Zcl, "PlatformMgrDelegate: OnShutDown");

        for (auto endpoint : EnabledEndpointsWithServerCluster(Basic::Id))
        {
            // If Basic cluster is implemented on this endpoint
            Events::ShutDown::Type event;
            EventNumber eventNumber;

            if (CHIP_NO_ERROR != LogEvent(event, endpoint, eventNumber, EventOptions::Type::kUrgent))
            {
                ChipLogError(Zcl, "PlatformMgrDelegate: Failed to record ShutDown event");
            }
        }
    }
};

PlatformMgrDelegate gPlatformMgrDelegate;

} // anonymous namespace

void emberAfBasicClusterServerInitCallback(chip::EndpointId endpoint)
{
    registerAttributeAccessOverride(&gAttrAccess);

    EmberAfStatus status;

    char nodeLabel[DeviceLayer::ConfigurationManager::kMaxNodeLabelLength + 1];
    if (ConfigurationMgr().GetNodeLabel(nodeLabel, sizeof(nodeLabel)) == CHIP_NO_ERROR)
    {
        status = Attributes::NodeLabel::Set(endpoint, chip::CharSpan(nodeLabel, strlen(nodeLabel)));
        VerifyOrdo(EMBER_ZCL_STATUS_SUCCESS == status, ChipLogError(Zcl, "Error setting Node Label: 0x%02x", status));
    }

    bool localConfigDisabled;
    if (ConfigurationMgr().GetLocalConfigDisabled(localConfigDisabled) == CHIP_NO_ERROR)
    {
        status = Attributes::LocalConfigDisabled::Set(endpoint, localConfigDisabled);
        VerifyOrdo(EMBER_ZCL_STATUS_SUCCESS == status, ChipLogError(Zcl, "Error setting Local Config Disabled: 0x%02x", status));
    }

    bool reachable;
    if (ConfigurationMgr().GetReachable(reachable) == CHIP_NO_ERROR)
    {
        status = Attributes::Reachable::Set(endpoint, reachable);
        VerifyOrdo(EMBER_ZCL_STATUS_SUCCESS == status, ChipLogError(Zcl, "Error setting Reachable: 0x%02x", status));
    }
 }

void MatterBasicPluginServerInitCallback()
{
    PlatformMgr().SetDelegate(&gPlatformMgrDelegate);
}
