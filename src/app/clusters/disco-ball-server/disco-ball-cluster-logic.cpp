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

#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enums.h>

namespace chip {
namespace app {

using ::chip::Protocols::InteractionModel::Status;

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
CHIP_ERROR DiscoBallClusterLogic::Init(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DiscoBallDriverInterface & driver)
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
    return CHIP_NO_ERROR;
}

bool DiscoBallClusterLogic::GetRunAttribute() const
{
    return mClusterState.run_attribute;
}

Status DiscoBallClusterLogic::SetRunAttribute(bool run_state)
{
    return Status::UnsupportedAttribute;
}

Clusters::DiscoBall::RotateEnum DiscoBallClusterLogic::GetRotateAttribute() const
{
    return mClusterState.rotate_attribute;
}

Status DiscoBallClusterLogic::SetRotateAttribute(Clusters::DiscoBall::RotateEnum rotate_state)
{
    return Status::UnsupportedAttribute;
}

uint8_t DiscoBallClusterLogic::GetSpeedAttribute() const
{
    return mClusterState.speed_attribute;
}

Status DiscoBallClusterLogic::SetSpeedAttribute(uint8_t speed)
{
    return Status::UnsupportedAttribute;
}

uint8_t DiscoBallClusterLogic::GetAxisAttribute() const
{
    return mClusterState.axis_attribute;
}

Status DiscoBallClusterLogic::SetAxisAttribute(uint8_t axis)
{
    return Status::UnsupportedAttribute;
}

uint8_t DiscoBallClusterLogic::GetWobbleSpeedAttribute() const
{
    return mClusterState.wobble_speed_attribute;
}

Status DiscoBallClusterLogic::SetWobbleSpeedAttribute(uint8_t wobble_speed)
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

Status DiscoBallClusterLogic::SetPattern(FabricIndex fabric_idx, const Clusters::DiscoBall::Structs::PatternStruct::Type & pattern)
{
    return Status::UnsupportedAttribute;
}

CharSpan DiscoBallClusterLogic::GetNameAttribute() const
{
    return mClusterState.name_attribute;
}

Status DiscoBallClusterLogic::SetNameAttribute(CharSpan name)
{
    return Status::UnsupportedAttribute;
}

BitFlags<Clusters::DiscoBall::WobbleBitmap> DiscoBallClusterLogic::GetWobbleSupportAttribute() const
{
    return mCapabilities.wobble_support;
}

BitFlags<Clusters::DiscoBall::WobbleBitmap> DiscoBallClusterLogic::GetWobbleSettingAttribute() const
{
    return mClusterState.wobble_setting_attribute;
}

Status DiscoBallClusterLogic::SetWobbleSettingAttribute(BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting)
{
    return Status::UnsupportedAttribute;
}

BitFlags<Clusters::DiscoBall::Feature> DiscoBallClusterLogic::GetSupportedFeatures() const
{
    return mCapabilities.supported_features;
}

Status HandleStartRequest(const Clusters::DiscoBall::Commands::StartRequest::DecodableType & args)
{
    return Status::UnsupportedCommand;
}

Status HandleStopRequest()
{
    return Status::UnsupportedCommand;
}

Status HandleReverseRequest()
{
    return Status::UnsupportedCommand;
}

Status HandleWobbleRequest()
{
    return Status::UnsupportedCommand;
}

Status HandlePatternRequest(FabricIndex fabric_index, const Clusters::DiscoBall::Commands::PatternRequest::DecodableType & args)
{
    return Status::UnsupportedCommand;
}

Status HandleStatsRequest(Clusters::DiscoBall::Commands::StatsResponse::Type & out_stats_response)
{
    return Status::UnsupportedCommand;
}

} // namespace app
} // namespace chip
