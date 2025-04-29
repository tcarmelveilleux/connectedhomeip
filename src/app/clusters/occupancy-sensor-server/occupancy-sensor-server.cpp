/**
 *
 *    Copyright (c) 2020-2024 Project CHIP Authors
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

#include <algorithm>

#include <stdint.h>

#include "occupancy-sensor-server.h"

#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/EventLogging.h>
#include <app/SafeAttributePersistenceProvider.h>
#include <app/data-model/Encode.h>
#include <app/reporting/reporting.h>
#include <app/util/attribute-storage.h>
#include <lib/core/CHIPError.h>
#include <lib/support/logging/CHIPLogging.h>

using chip::Protocols::InteractionModel::Status;

namespace chip {
namespace app {
namespace Clusters {
namespace OccupancySensing {

namespace {

// Mapping table from spec:
//
//  | Feature Flag Value    | Value of OccupancySensorTypeBitmap    | Value of OccupancySensorType
//  | PIR | US | PHY        | ===================================== | ============================
//  | 0   | 0  | 0          | PIR ^*^                               | PIR
//  | 1   | 0  | 0          | PIR                                   | PIR
//  | 0   | 1  | 0          | Ultrasonic                            | Ultrasonic
//  | 1   | 1  | 0          | PIR + Ultrasonic                      | PIRAndUltrasonic
//  | 0   | 0  | 1          | PhysicalContact                       | PhysicalContact
//  | 1   | 0  | 1          | PhysicalContact + PIR                 | PIR
//  | 0   | 1  | 1          | PhysicalContact + Ultrasonic          | Ultrasonic
//  | 1   | 1  | 1          | PhysicalContact + PIR + Ultrasonic    | PIRAndUltrasonic

BitMask<OccupancySensorTypeBitmap> FeaturesToOccupancySensorTypeBitmap(BitFlags<Feature> features)
{
    BitMask<OccupancySensorTypeBitmap> occupancySensorTypeBitmap{};

    if (!features.HasAny(Feature::kPassiveInfrared, Feature::kUltrasonic, Feature::kPhysicalContact))
    {
        // No legacy PIR/IS/PHY: assume at least PIR for backwards compatibility.
        occupancySensorTypeBitmap.Set(OccupancySensorTypeBitmap::kPir);
        return occupancySensorTypeBitmap;
    }

    // Otherwise each bit maps directly for PIR/US/PHY.
    occupancySensorTypeBitmap.Set(OccupancySensorTypeBitmap::kPir, features.Has(Feature::kPassiveInfrared));
    occupancySensorTypeBitmap.Set(OccupancySensorTypeBitmap::kUltrasonic, features.Has(Feature::kUltrasonic));
    occupancySensorTypeBitmap.Set(OccupancySensorTypeBitmap::kPhysicalContact, features.Has(Feature::kPhysicalContact));

    return occupancySensorTypeBitmap;
}

OccupancySensorTypeEnum FeaturesToOccupancySensorType(BitFlags<Feature> features)
{
    // Note how the truth table in the comment at the top of this section has
    // the PIR/US/PHY columns not in MSB-descending order. This is OK as we apply the correct
    // bit weighing to make the table equal below.
    unsigned maskFromFeatures = (features.Has(Feature::kPhysicalContact) ? (1 << 2) : 0) |
    (features.Has(Feature::kUltrasonic) ? (1 << 1) : 0) |
    (features.Has(Feature::kPhysicalContact) ? (1 << 0) : 0);

    const OccupancySensorTypeEnum mappingTable[8] = {
      //                                                | PIR | US | PHY | Type value
      OccupancySensorTypeEnum::kPir,                //  | 0   | 0  | 0   | PIR
      OccupancySensorTypeEnum::kPir,                //  | 1   | 0  | 0   | PIR
      OccupancySensorTypeEnum::kUltrasonic,         //  | 0   | 1  | 0   | Ultrasonic
      OccupancySensorTypeEnum::kPIRAndUltrasonic,   //  | 1   | 1  | 0   | PIRAndUltrasonic
      OccupancySensorTypeEnum::kPhysicalContact,    //  | 0   | 0  | 1   | PhysicalContact
      OccupancySensorTypeEnum::kPir,                //  | 1   | 0  | 1   | PIR
      OccupancySensorTypeEnum::kUltrasonic,         //  | 0   | 1  | 1   | Ultrasonic
      OccupancySensorTypeEnum::kPIRAndUltrasonic,   //  | 1   | 1  | 1   | PIRAndUltrasonic
    };

    // This check is to ensure that no changes to the mapping table in the future can overrun.
    if (maskFromFeatures >= sizeof(mappingTable))
    {
        return OccupancySensorTypeEnum::kPir;
    }

    return mappingTable[maskFromFeatures];
}

constexpr uint16_t kLatestClusterRevision = 5u;

} // namespace

CHIP_ERROR Instance::Init()
{
    mDelegate->SetInstance(this);
    mDelegate->Init();
    mFeatures = mDelegate->GetSupportedFeatures();

    VerifyOrReturnError(chip::app::AttributeAccessInterfaceRegistry::Instance().Register(this), CHIP_ERROR_INCORRECT_STATE);

    // TODO: Convert this to new data model APIs when available.
    mHasHoldTime = (nullptr != emberAfLocateAttributeMetadata(mEndpointId, OccupancySensing::Id,  Attributes::HoldTime::Id));
    mHasPirOccupiedToUnoccupiedDelaySeconds = (nullptr != emberAfLocateAttributeMetadata(mEndpointId, OccupancySensing::Id,  Attributes::PIROccupiedToUnoccupiedDelay::Id));

    if (mHasPirOccupiedToUnoccupiedDelaySeconds && !mHasHoldTime)
    {
        ChipLogError(Zcl, "PIROccupiedToUnoccupiedDelay requires HoldTime!");
        return CHIP_ERROR_INCORRECT_STATE;
    }

    if (!mHasHoldTime)
    {
        return CHIP_NO_ERROR;
    }

    SafeAttributePersistenceProvider * persister = GetSafeAttributePersistenceProvider();
    VerifyOrReturnError(persister != nullptr, CHIP_ERROR_INCORRECT_STATE);

    Structs::HoldTimeLimitsStruct::Type holdTimeLimits = mDelegate->GetHoldTimeLimits();

    auto holdTimePath = ConcreteAttributePath{mEndpointId, OccupancySensing::Id,  Attributes::HoldTime::Id};
    uint16_t holdTimeSeconds = holdTimeLimits.holdTimeDefault;
    if (persister->ReadScalarValue(holdTimePath, holdTimeSeconds) != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "Warning: did not find persisted HoldTime in persisted storage for EP %u", static_cast<unsigned>(mEndpointId));
    }
    else
    {
        holdTimeSeconds = std::max(holdTimeLimits.holdTimeMin, holdTimeSeconds);
        holdTimeSeconds = std::min(holdTimeSeconds, holdTimeLimits.holdTimeMax);
    }

    mHoldTimeSeconds = holdTimeSeconds;
    return CHIP_NO_ERROR;
}

Instance::~Instance() {
    chip::app::AttributeAccessInterfaceRegistry::Instance().Unregister(this);
}

CHIP_ERROR Instance::Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder)
{
    VerifyOrDie((aPath.mClusterId == app::Clusters::OccupancySensing::Id) && (mDelegate != nullptr));

    switch (aPath.mAttributeId)
    {
    case Attributes::ClusterRevision::Id: {
        return aEncoder.Encode(kLatestClusterRevision);
    }
    case Attributes::FeatureMap::Id: {
        return aEncoder.Encode(mFeatures);
    }
    case Attributes::HoldTime::Id:
    case Attributes::PIROccupiedToUnoccupiedDelay::Id:
    case Attributes::UltrasonicOccupiedToUnoccupiedDelay::Id:
    case Attributes::PhysicalContactOccupiedToUnoccupiedDelay::Id: {
        // Both attributes have to track.
        return aEncoder.Encode(mHoldTimeSeconds);
    }
    case Attributes::HoldTimeLimits::Id: {
        Structs::HoldTimeLimitsStruct::Type holdTimeLimitsStruct = mDelegate->GetHoldTimeLimits();

        // Ensure minimum of 1 is always met.
        holdTimeLimitsStruct.holdTimeMin = std::max(static_cast<uint16_t>(1u), holdTimeLimitsStruct.holdTimeMin);

        return aEncoder.Encode(holdTimeLimitsStruct);
    }
    case Attributes::OccupancySensorType::Id: {
        return aEncoder.Encode(FeaturesToOccupancySensorType(mFeatures));
    }
    case Attributes::OccupancySensorTypeBitmap::Id: {
        return aEncoder.Encode(FeaturesToOccupancySensorTypeBitmap(mFeatures));
    }
    case Attributes::Occupancy::Id: {
        chip::BitMask<chip::app::Clusters::OccupancySensing::OccupancyBitmap> occupancy;
        if (mOccupancyDetected)
        {
            occupancy.Set(OccupancyBitmap::kOccupied);
        }
        return aEncoder.Encode(occupancy);
    }

    // NOTE: All the legacy (non-hold-time) timing attributes are not implemented here
    //       and they will flow-through to legacy ember handling.
    //       It then becomes important to manage the timing of those parameters properly
    //       against the HoldTime attribute.
    // TODO: Implement a mode that supports Version 4 of the cluster in common code.
    default:
        break;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR Instance::Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder)
{
    VerifyOrDie((aPath.mClusterId == app::Clusters::OccupancySensing::Id) && (mDelegate != nullptr));

    switch (aPath.mAttributeId)
    {
    case Attributes::HoldTime::Id:
    case Attributes::PIROccupiedToUnoccupiedDelay::Id:
    case Attributes::UltrasonicOccupiedToUnoccupiedDelay::Id:
    case Attributes::PhysicalContactOccupiedToUnoccupiedDelay::Id: {
        // Processing is the same for both, since they have to track.
        uint16_t newHoldTimeSeconds = mHoldTimeSeconds;
        ReturnErrorOnFailure(aDecoder.Decode(newHoldTimeSeconds));
        return HandleWriteHoldTime(newHoldTimeSeconds);
    }
    default:
        break;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR Instance::HandleWriteHoldTime(uint16_t newHoldTimeSeconds)
{
    VerifyOrReturnError(mDelegate != nullptr, CHIP_IM_GLOBAL_STATUS(Failure));

    VerifyOrReturnError(mHasHoldTime, CHIP_IM_GLOBAL_STATUS(UnsupportedAttribute));

    SafeAttributePersistenceProvider * persister = GetSafeAttributePersistenceProvider();
    VerifyOrReturnError(persister != nullptr, CHIP_IM_GLOBAL_STATUS(Failure));

    Structs::HoldTimeLimitsStruct::Type currHoldTimeLimits = mDelegate->GetHoldTimeLimits();
    VerifyOrReturnError(newHoldTimeSeconds >= std::max(static_cast<uint16_t>(1), currHoldTimeLimits.holdTimeMin), CHIP_IM_GLOBAL_STATUS(ConstraintError));
    VerifyOrReturnError(newHoldTimeSeconds <= currHoldTimeLimits.holdTimeMax, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    // No change: do nothing.
    if (newHoldTimeSeconds == mHoldTimeSeconds)
    {
        return CHIP_NO_ERROR;
    }

    mHoldTimeSeconds = newHoldTimeSeconds;

    mDelegate->OnHoldTimeWrite(newHoldTimeSeconds);

    auto holdTimePath = ConcreteAttributePath{mEndpointId, OccupancySensing::Id,  Attributes::HoldTime::Id};
    CHIP_ERROR err = persister->WriteScalarValue(holdTimePath, newHoldTimeSeconds);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "Failed HoldTime storage on EP %u: %" CHIP_ERROR_FORMAT, static_cast<unsigned>(mEndpointId), err.Format());
    }

    MatterReportingAttributeChangeCallback(mEndpointId, OccupancySensing::Id, Attributes::HoldTime::Id);
    if (mHasPirOccupiedToUnoccupiedDelaySeconds)
    {
        MatterReportingAttributeChangeCallback(mEndpointId, OccupancySensing::Id, Attributes::PIROccupiedToUnoccupiedDelay::Id);
    }
    if (mHasUltrasonicOccupiedToUnoccupiedDelaySeconds)
    {
        MatterReportingAttributeChangeCallback(mEndpointId, OccupancySensing::Id, Attributes::UltrasonicOccupiedToUnoccupiedDelay::Id);
    }
    if (mHasPhysicalContactOccupiedToUnoccupiedDelaySeconds)
    {
        MatterReportingAttributeChangeCallback(mEndpointId, OccupancySensing::Id, Attributes::PhysicalContactOccupiedToUnoccupiedDelay::Id);
    }

    return (err != CHIP_NO_ERROR) ? CHIP_IM_GLOBAL_STATUS(Failure) : CHIP_NO_ERROR;
}

void Instance::UpdateHoldTimeFromSensor(uint16_t newHoldTimeSeconds)
{
    ChipLogProgress(Zcl, "Got request from delegate to update hold time on EP %u to : %u", static_cast<unsigned>(mEndpointId), static_cast<unsigned>(newHoldTimeSeconds));
    (void)HandleWriteHoldTime(newHoldTimeSeconds);
}

// TODO: Deal with changes of hold time when hold time timer is running.
void Instance::SetOccupancyDetectedFromSensor(bool detected)
{
    mLastOccupancyDetectedFromDelegate = detected;
    if (mOccupancyDetected == detected)
    {
        return;
    }

    // No hold time constraint --> immediately report on change
    if (!mHasHoldTime || (mHoldTimeSeconds == 0))
    {
        InternalSetOccupancy(detected);
        return;
    }

    // Hold time constraints --> let the timer handle falling edges, if needed, from last rising edge
    if (mIsHoldTimerRunning)
    {
        // Let timer manage the situation.
        return;
    }

    // Unoccupied -> Occupied: start the occupancy detected timer.
    if (!mOccupancyDetected && detected)
    {
        InternalSetOccupancy(true);
        ChipLogProgress(Zcl, "Delaying unoccupied state by at least %u seconds on EP %u", static_cast<unsigned>(mHoldTimeSeconds), static_cast<unsigned>(mEndpointId));
        mIsHoldTimerRunning = true;
        mDelegate->StartHoldTimer(mHoldTimeSeconds, [this]() { this->HandleHoldTimerExpiry(); });
    }
    // Occupied -> Unoccupied: timer no longer running, so it's been long enough and we can move to unoccupied.
    else if (mOccupancyDetected && !detected)
    {
        InternalSetOccupancy(false);
    }
    else
    {
        // This is impossible...
    }
}

void Instance::HandleHoldTimerExpiry()
{
    mIsHoldTimerRunning = false;
    ChipLogProgress(Zcl, "Got to end of hold delay on EP %u", static_cast<unsigned>(mEndpointId));

    // If already unoccupied by end of hold timer, there was an unoccupied detection
    // during the hold window. It's now time to apply it.
    if (!mLastOccupancyDetectedFromDelegate)
    {
        InternalSetOccupancy(false);
    }
    else
    {
        // Still occupied --> the next occupied-to-unoccupied can directly hit outside the timer.
    }
}

void Instance::InternalSetOccupancy(bool occupied)
{
    if (occupied == mOccupancyDetected)
    {
        return;
    }

    ChipLogProgress(Zcl, "Applying occupancy change: %d -> %d", static_cast<int>(mOccupancyDetected), static_cast<int>(occupied));
    mOccupancyDetected = occupied;

    if (mDelegate != nullptr)
    {
        chip::BitMask<chip::app::Clusters::OccupancySensing::OccupancyBitmap> occupancy;
        if (occupied)
        {
            occupancy.Set(OccupancyBitmap::kOccupied);
        }
        mDelegate->OnOccupancyChange(occupancy);
    }

    MatterReportingAttributeChangeCallback(mEndpointId, OccupancySensing::Id, Attributes::Occupancy::Id);
}

} // namespace OccupancySensing
} // namespace Clusters
} // namespace app
} // namespace chip

// Instantiate and initialize the OccupancySensing::Instance/Delegate from your application directly! Don't rely
// on global initialization!
void MatterOccupancySensingPluginServerInitCallback() {}
void emberAfOccupancySensingClusterServerInitCallback(chip::EndpointId endpointId) {}

// TODO: Fix linux simulation
#if 0
    uint16_t previousHoldTime = *holdTimeForEndpoint;
    *holdTimeForEndpoint      = newHoldTime;

    if (previousHoldTime != newHoldTime)
    {
        MatterReportingAttributeChangeCallback(endpointId, OccupancySensing::Id, Attributes::HoldTime::Id);
    }

    // Blindly try to write RAM-backed legacy attributes (will fail silently if absent)
    // to keep them in sync.
    (void) Attributes::PIROccupiedToUnoccupiedDelay::Set(endpointId, newHoldTime);
    (void) Attributes::UltrasonicOccupiedToUnoccupiedDelay::Set(endpointId, newHoldTime);
    (void) Attributes::PhysicalContactOccupiedToUnoccupiedDelay::Set(endpointId, newHoldTime);
#endif
