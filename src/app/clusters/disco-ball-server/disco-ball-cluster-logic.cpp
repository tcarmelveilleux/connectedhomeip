/**
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include "disco-ball-cluster-logic.h"

#include <stdint.h>

#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>
#include <lib/support/BitFlags.h>

#include <protocols/interaction_model/StatusCode.h>

#include <app/ConcreteAttributePath.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enums.h>

namespace chip {
namespace app {

using ::chip::Protocols::InteractionModel::ClusterStatusCode;
using ::chip::Protocols::InteractionModel::Status;
using ::chip::app::Clusters::DiscoBall::Feature;

DiscoBallCapabilities::DiscoBallCapabilities() :
    supported_features(BitFlags<Clusters::DiscoBall::Feature>{0}),
    min_speed_value{0},
    max_speed_value{200},
    min_axis_value{0},
    max_axis_value{90},
    min_wobble_speed_value{0},
    max_wobble_speed_value{100},
    wobble_support(BitFlags<Clusters::DiscoBall::WobbleBitmap>{0}) {}

/* =========================== Start of DiscoBallClusterState =====================*/
CHIP_ERROR DiscoBallClusterState::Init(EndpointId endpoint_id, NonVolatileStorageInterface & storage)
{
    mEndpointId = endpoint_id;
    mStorage = &storage;

    this->last_run_statistic = 0;
    this->patterns_statistic = 0;

    this->is_running = false;
    this->is_pattern_running = false;
    this->current_pattern_idx = 0;

    this->run_attribute = false;
    this->rotate_attribute = Clusters::DiscoBall::RotateEnum::kClockwise;
    this->speed_attribute = 0;
    this->axis_attribute = 0;
    this->wobble_speed_attribute = 0;
    this->num_patterns = 0;
    // Don't touch the actual patterns, the pattern loading below will do it.

    this->name_attribute = CharSpan{};

    this->wobble_setting_attribute = BitFlags<Clusters::DiscoBall::WobbleBitmap>{};

    CHIP_ERROR err = ReloadFromStorage();
    if (err != CHIP_NO_ERROR)
    {
        Deinit();
        return err;
    }

    return CHIP_NO_ERROR;
}

bool DiscoBallClusterState::SetNameAttribute(CharSpan name)
{
    if (name.size() > sizeof(this->name_backing))
    {
        return false;
    }

    memcpy(this->name_backing, name.data(), name.size());
    this->name_attribute = CharSpan(&this->name_backing[0], name.size());

    return true;
}

CHIP_ERROR DiscoBallClusterState::DiscoBallClusterState::ReloadFromStorage()
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INCORRECT_STATE);

    CHIP_ERROR err = mStorage->LoadFromStorage(*this);
    if ((err != CHIP_NO_ERROR) && (err != CHIP_ERROR_NOT_FOUND))
    {
        return err;
    }

    // Not finding anything in storage is fair game.
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiscoBallClusterState::SaveToStorage()
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INCORRECT_STATE);

    return mStorage->SaveToStorage(*this);
}

bool DiscoBallClusterState::SetRunningPasscode(CharSpan passcode)
{
    if (passcode.size() > sizeof(mCurrentRunningPasscodeBacking))
    {
        return false;
    }

    memcpy(mCurrentRunningPasscodeBacking, passcode.data(), passcode.size());
    mCurrentRunningPasscode = CharSpan(&mCurrentRunningPasscodeBacking[0], passcode.size());

    return true;
}

CharSpan DiscoBallClusterState::GetRunningPasscode() const
{
    return mCurrentRunningPasscode;
}

/* =========================== Start of DiscoBallClusterLogic =====================*/
CHIP_ERROR DiscoBallClusterLogic::Init(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DriverInterface & driver)
{
    mEndpointId = endpoint_id;

    mDriver = &driver;
    mCapabilities = mDriver->GetCapabilities(mEndpointId);

    CHIP_ERROR err = mClusterState.Init(endpoint_id, storage);
    if (err != CHIP_NO_ERROR)
    {
        Deinit();
        return err;
    }

    // Override defaults with capabilities;
    mClusterState.speed_attribute = mCapabilities.min_speed_value;
    mClusterState.axis_attribute = mCapabilities.min_axis_value;
    mClusterState.wobble_speed_attribute = mCapabilities.min_wobble_speed_value;

    ChipLogProgress(Zcl, "DiscoBall cluster id 0x%04x initialized on Endpoint %u", static_cast<unsigned>(GetClusterId()), static_cast<unsigned>(GetEndpointId()));

    return CHIP_NO_ERROR;
}

bool DiscoBallClusterLogic::GetRunAttribute() const
{
    return mClusterState.run_attribute;
}

// Never set via attribute, only via command.
ClusterStatusCode DiscoBallClusterLogic::SetRunAttribute(bool run_state)
{
    // TODO: HANDLE EVENT
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);

    bool changed = (run_state != mClusterState.run_attribute);
    if (!changed)
    {
        return Status::Success;
    }

    mClusterState.run_attribute = run_state;
    BitFlags<DiscoBallFunction> changes{DiscoBallFunction::kRunning};
    Status driver_status = mDriver->OnClusterStateChange(mEndpointId, changes, *this);
    VerifyOrReturnValue(driver_status == Status::Success, driver_status);

    mDriver->MarkAttributeDirty(ConcreteAttributePath{mEndpointId, GetClusterId(), Clusters::DiscoBall::Attributes::Run::Id});
    return Status::Success;
}

Clusters::DiscoBall::RotateEnum DiscoBallClusterLogic::GetRotateAttribute() const
{
    return mClusterState.rotate_attribute;
}

// Never set via attribute, only via command.
ClusterStatusCode DiscoBallClusterLogic::SetRotateAttribute(Clusters::DiscoBall::RotateEnum rotate_state)
{
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);
    VerifyOrReturnValue(rotate_state != Clusters::DiscoBall::RotateEnum::kUnknownEnumValue, Status::Failure);

    bool changed = (rotate_state != mClusterState.rotate_attribute);
    if (!changed)
    {
        return Status::Success;
    }

    mClusterState.rotate_attribute = rotate_state;
    BitFlags<DiscoBallFunction> changes{DiscoBallFunction::kRotation};
    Status driver_status = mDriver->OnClusterStateChange(mEndpointId, changes, *this);
    VerifyOrReturnValue(driver_status == Status::Success, driver_status);

    mDriver->MarkAttributeDirty(ConcreteAttributePath{mEndpointId, GetClusterId(), Clusters::DiscoBall::Attributes::Rotate::Id});
    return Status::Success;
}

uint8_t DiscoBallClusterLogic::GetSpeedAttribute() const
{
    return mClusterState.speed_attribute;
}

// Never set via attribute, only via command.
ClusterStatusCode DiscoBallClusterLogic::SetSpeedAttribute(uint8_t speed)
{
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);
    // TODO: Validate speed range fully
    VerifyOrReturnValue(speed >= mCapabilities.min_speed_value, Status::Failure);
    VerifyOrReturnValue(speed <= mCapabilities.max_speed_value, Status::Failure);

    bool changed = (speed != mClusterState.speed_attribute);
    if (!changed)
    {
        return Status::Success;
    }

    mClusterState.speed_attribute = speed;
    BitFlags<DiscoBallFunction> changes{DiscoBallFunction::kSpeed};
    Status driver_status = mDriver->OnClusterStateChange(mEndpointId, changes, *this);
    VerifyOrReturnValue(driver_status == Status::Success, driver_status);

    mDriver->MarkAttributeDirty(ConcreteAttributePath{mEndpointId, GetClusterId(), Clusters::DiscoBall::Attributes::Speed::Id});
    return Status::Success;
}

uint8_t DiscoBallClusterLogic::GetAxisAttribute() const
{
    return mClusterState.axis_attribute;
}

ClusterStatusCode DiscoBallClusterLogic::SetAxisAttribute(uint8_t axis)
{
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);
    VerifyOrReturnValue(mCapabilities.supported_features.Has(Feature::kAxis), Status::UnsupportedAttribute);

    // TODO: Validate speed range fully
    // TODO: Handle actually-supported axis values when spec updated
    VerifyOrReturnValue(axis >= mCapabilities.min_axis_value, Status::ConstraintError);
    VerifyOrReturnValue(axis <= mCapabilities.max_axis_value, Status::ConstraintError);

    bool changed = (axis != mClusterState.axis_attribute);
    if (!changed)
    {
        return Status::Success;
    }

    mClusterState.axis_attribute = axis;
    BitFlags<DiscoBallFunction> changes{DiscoBallFunction::kAxis};
    Status driver_status = mDriver->OnClusterStateChange(mEndpointId, changes, *this);
    VerifyOrReturnValue(driver_status == Status::Success, driver_status);

    mDriver->MarkAttributeDirty(ConcreteAttributePath{mEndpointId, GetClusterId(), Clusters::DiscoBall::Attributes::Axis::Id});
    return Status::Success;
}

uint8_t DiscoBallClusterLogic::GetWobbleSpeedAttribute() const
{
    return mClusterState.wobble_speed_attribute;
}

ClusterStatusCode DiscoBallClusterLogic::SetWobbleSpeedAttribute(uint8_t wobble_speed)
{
    return Status::UnsupportedAttribute;
}

size_t DiscoBallClusterLogic::GetNumPatterns(FabricIndex fabric_idx) const
{
    // With no accessing fabric, return total number of entries.
    if (fabric_idx == kUndefinedFabricIndex)
    {
        return mClusterState.num_patterns;
    }

    // With accessing fabric, return count only of those matching the accessing fabric.
    size_t pattern_count = 0;
    for (size_t pattern_idx = 0; pattern_idx < mClusterState.num_patterns; ++pattern_idx)
    {
        const auto & pattern = mClusterState.pattern_attribute[pattern_idx];
        if (pattern.GetValue().fabricIndex == fabric_idx)
        {
            ++pattern_count;
        }
    }

    return pattern_count;
}

chip::Optional<DiscoBallPatternStructBacking> DiscoBallClusterLogic::GetPatternAttributeEntry(FabricIndex fabric_idx, size_t pattern_idx) const
{
    return chip::NullOptional;
}

CHIP_ERROR DiscoBallClusterLogic::ClearPattern(FabricIndex fabric_idx, size_t pattern_idx)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

ClusterStatusCode DiscoBallClusterLogic::SetPattern(FabricIndex fabric_idx, const Clusters::DiscoBall::Structs::PatternStruct::Type & pattern)
{
    return Status::UnsupportedAttribute;
}

CharSpan DiscoBallClusterLogic::GetNameAttribute() const
{
    return mClusterState.name_attribute;
}

ClusterStatusCode DiscoBallClusterLogic::SetNameAttribute(CharSpan name)
{
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);
    VerifyOrReturnValue(mClusterState.SetNameAttribute(name), Status::ConstraintError);

    CHIP_ERROR storage_error = mClusterState.SaveToStorage();
    if (storage_error != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "Failed to store name to non-volatile storage: %" CHIP_ERROR_FORMAT, storage_error.Format());
        return Status::ResourceExhausted;
    }

    BitFlags<DiscoBallFunction> changes{DiscoBallFunction::kName};
    return mDriver->OnClusterStateChange(mEndpointId, changes, *this);
}

BitFlags<Clusters::DiscoBall::WobbleBitmap> DiscoBallClusterLogic::GetWobbleSupportAttribute() const
{
    return mCapabilities.wobble_support;
}

BitFlags<Clusters::DiscoBall::WobbleBitmap> DiscoBallClusterLogic::GetWobbleSettingAttribute() const
{
    return mClusterState.wobble_setting_attribute;
}

ClusterStatusCode DiscoBallClusterLogic::SetWobbleSettingAttribute(BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting)
{
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);
    VerifyOrReturnValue(mCapabilities.supported_features.Has(Feature::kWobble), Status::UnsupportedAttribute);
    // TODO: Need to return UNSUPPORTED_PATTERN status on failure to support the given wobble.

    mClusterState.wobble_setting_attribute = wobble_setting;

    BitFlags<DiscoBallFunction> changes{DiscoBallFunction::kWobbleSetting};
    return mDriver->OnClusterStateChange(mEndpointId, changes, *this);
}

BitFlags<Clusters::DiscoBall::Feature> DiscoBallClusterLogic::GetSupportedFeatures() const
{
    return mCapabilities.supported_features;
}

ClusterStatusCode DiscoBallClusterLogic::HandleStartRequest(const Clusters::DiscoBall::Commands::StartRequest::DecodableType & args)
{
    // TODO: Use actual driver capabilities for speed/rotate limits.
    VerifyOrReturnValue(mDriver != nullptr, Status::Failure);

    // TODO: Validate these errors.
    VerifyOrReturnValue(args.speed <= mCapabilities.max_speed_value, Status::InvalidCommand);
    if (args.rotate.HasValue())
    {
        VerifyOrReturnValue(args.rotate.Value() != Clusters::DiscoBall::RotateEnum::kUnknownEnumValue, Status::InvalidCommand);
        VerifyOrReturnValue(SetRotateAttribute(args.rotate.Value()) == Status::Success, Status::InvalidCommand);
    }

    VerifyOrReturnValue(SetSpeedAttribute(args.speed) == Status::Success, Status::InvalidCommand);
    VerifyOrReturnValue(SetRunAttribute(true) == Status::Success, Status::InvalidCommand);

    return Status::Success;
}

ClusterStatusCode DiscoBallClusterLogic::HandleStopRequest()
{
    // TODO: Set rotate attribute to off.
    bool success = true;

    success = success && (SetSpeedAttribute(0) == Status::Success);
    success = success && (SetRunAttribute(false) == Status::Success);

    return success ? Status::Success : Status::Failure;
}

ClusterStatusCode DiscoBallClusterLogic::HandleReverseRequest()
{
    return Status::UnsupportedCommand;
}

ClusterStatusCode DiscoBallClusterLogic::HandleWobbleRequest()
{
    return Status::UnsupportedCommand;
}

ClusterStatusCode DiscoBallClusterLogic::HandlePatternRequest(FabricIndex fabric_index, const Clusters::DiscoBall::Commands::PatternRequest::DecodableType & args)
{
    return Status::UnsupportedCommand;
}

ClusterStatusCode DiscoBallClusterLogic::HandleStatsRequest(Clusters::DiscoBall::Commands::StatsResponse::Type & out_stats_response)
{
    return Status::UnsupportedCommand;
}

} // namespace app
} // namespace chip
