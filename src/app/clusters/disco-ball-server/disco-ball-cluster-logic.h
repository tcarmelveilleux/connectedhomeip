/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <stdint.h>

#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/support/Span.h>
#include <lib/support/BitFlags.h>

#include <protocols/interaction_model/StatusCode.h>

#include <app/ConcreteAttributePath.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enums.h>

namespace chip {
namespace app {

static constexpr size_t kDiscoBallPasscodeMaxSize = 6;

class DiscoBallPatternStructBacking
{
public:
    DiscoBallPatternStructBacking() = default;

    CHIP_ERROR SetValue(const Clusters::DiscoBall::Structs::PatternStruct::Type & value);
    const Clusters::DiscoBall::Structs::PatternStruct::Type & GetValue() const;
private:
    Clusters::DiscoBall::Structs::PatternStruct::Type mStorage;
    // Storage for the passcode (if present). `mStorage`'s `passcode` being present with `.size()` > 0
    // indicates that the `.data()` will point to this.
    char mPasscodeBacking[kDiscoBallPasscodeMaxSize];
};

struct DiscoBallClusterState
{
    // TODO: Not clear that it's 16 total, or 16 per fabric. Assume 16 total.
    // TODO: Not clear what to do when a pattern recall with password is used...
    static constexpr size_t kNumPatterns = 16;
    static constexpr size_t kNameMaxSize = 32; // Using SuperDiscoBall superset value

    class NonVolatileStorageInterface
    {
    public:
        virtual ~NonVolatileStorageInterface() = default;

        virtual CHIP_ERROR SaveToStorage(const DiscoBallClusterState & attributes) = 0;
        virtual CHIP_ERROR LoadFromStorage(DiscoBallClusterState & attributes) = 0;
        virtual void RemoveDataForFabric(FabricIndex fabric_index) = 0;
        virtual void RemoveDataForAllFabrics() = 0;
    };

    DiscoBallClusterState() = default;

// TODO: Consider observer registration to record attribute changes from driver ?
    CHIP_ERROR Init(EndpointId endpoint_id, NonVolatileStorageInterface & storage);
    void Deinit()
    {
        mStorage = nullptr;
        mIsInitialized = false;
    }

    bool IsInitialized() const { return mStorage != nullptr; }

    // No shutdown: all non-volatile mutations are immediately stored.

    CHIP_ERROR ReloadFromStorage();
    CHIP_ERROR SaveToStorage();

// TODO: Record supported features (from driver!!!)
// TODO: Label

    // For StatsRequest command.
    uint32_t last_run_statistic;
    uint32_t patterns_statistic;

    // Whether we have been started or not.
    bool is_running = false;
    // Whether we are playing a pattern.
    bool is_pattern_running = false;
    // Current playing pattern if pattern sequence ongoing.
    size_t current_pattern_idx = 0;

    bool SetRunningPasscode(CharSpan passcode);
    CharSpan GetRunningPasscode() const;

    bool SetNameAttribute(CharSpan name);

    // ------------------------
    // Attributes storage start
    // ------------------------

    // 0x0000  s| Run           | bool                        | all^*^         |         | 0       | R V T^*^ | M
    bool run_attribute;

    // 0x0001  s| Rotate        | <<ref_RotateEnum>>          | all            |         | 0       | R V      | M
    Clusters::DiscoBall::RotateEnum rotate_attribute;

    // 0x0002  s| Speed         | uint8                       | 0 to 200^*^    |         | 0       | R V      | M
    uint8_t speed_attribute;

    // 0x0003  s| Axis          | uint8                       | 0 to 90        |         | 0       | RW VO    | AX \| WBL
    uint8_t axis_attribute;

    // 0x0004  s| WobbleSpeed   | uint8                       | 0 to 200       |         | 0       | RW VO    | WBL
    uint8_t wobble_speed_attribute;

    // 0x0005  s| Pattern       | list[<<ref_PatternStruct>>] | max 16^*^      | N       | 0       | RW VM    | PAT
    size_t num_patterns = 0;
    DiscoBallPatternStructBacking pattern_attribute[kNumPatterns];

    // 0x0006  s| Name          | string                      | max 16         | N^*^    | 0       | RW VM    | P, O
    char name_backing[kNameMaxSize];
    CharSpan name_attribute;

    // 0x0007  s| WobbleSupport | <<ref_WobbleBitmap>>        | desc           |         |         | R V      | [WBL]
    // --> This attribute is directly obtained from the driver.

    // 0x0008  s| WobbleSetting | <<ref_WobbleBitmap>>        | desc           |         |         | RW VM    | [WBL]
    BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting_attribute;

    // ----------------------
    // Attributes storage end
    // ----------------------
  private:
    bool mIsInitialized = false;
    NonVolatileStorageInterface * mStorage = nullptr;
    EndpointId mEndpointId = kInvalidEndpointId;

    CharSpan mCurrentRunningPasscode{};
    char mCurrentRunningPasscodeBacking[kDiscoBallPasscodeMaxSize];
};

// TODO: Account for MS commands
// TODO: How to go from DriverInterface to attribute changes? Observer?

typedef void (*DiscoBallTimerCallback)(EndpointId id, void *ctx);

struct DiscoBallCapabilities
{
    DiscoBallCapabilities();

    BitFlags<Clusters::DiscoBall::Feature> supported_features;

    uint8_t min_speed_value;
    uint8_t max_speed_value;

    uint8_t min_axis_value;
    uint8_t max_axis_value;

    // Only valid if supported_features.Has(DiscoBall::Feature::kWobble)
    uint8_t min_wobble_speed_value;
    uint8_t max_wobble_speed_value;

    // Only valid if supported_features.Has(DiscoBall::Feature::kWobble)
    BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_support;
};

enum DiscoBallFunction : uint16_t
{
    kRunning = (1 << 0),
    kRotation = (1 << 1),
    kSpeed = (1 << 2),
    kAxis = (1 << 3),
    kWobbleSpeed = (1 << 4),
    kWobbleSetting = (1 << 5),
    kName = (1 << 6),
};

class DiscoBallClusterLogic
{
public:
    class DriverInterface
    {
    public:
        virtual ~DriverInterface() = default;
        virtual void Init() {}
        virtual DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const = 0;

        virtual Protocols::InteractionModel::Status OnClusterStateChange(EndpointId endpoint_id, BitFlags<DiscoBallFunction> changes, DiscoBallClusterLogic & cluster) = 0;
        virtual void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoBallTimerCallback timer_cb, void * ctx) = 0;
        virtual void CancelPatternTimer(EndpointId endpoint_id) = 0;
        virtual void MarkAttributeDirty(const ConcreteAttributePath& path) = 0;
    };

    DiscoBallClusterLogic() = default;

    // Not copyable.
    DiscoBallClusterLogic(DiscoBallClusterLogic const&) = delete;
    DiscoBallClusterLogic& operator=(DiscoBallClusterLogic const&) = delete;

    CHIP_ERROR Init(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DriverInterface & driver);
    void Deinit()
    {
        mDriver = nullptr;
        mEndpointId = kInvalidEndpointId;
    }

    EndpointId GetEndpointId() const { return mEndpointId; }
    ClusterId GetClusterId() const { return Clusters::DiscoBall::Id; }

    // 0x0000  s| Run           | bool                        | all^*^         |         | 0       | R V T^*^ | M
    // 0x0001  s| Rotate        | <<ref_RotateEnum>>          | all            |         | 0       | R V      | M
    // 0x0002  s| Speed         | uint8                       | 0 to 200^*^    |         | 0       | R V      | M
    // 0x0003  s| Axis          | uint8                       | 0 to 90        |         | 0       | RW VO    | AX \| WBL
    // 0x0004  s| WobbleSpeed   | uint8                       | 0 to 200       |         | 0       | RW VO    | WBL
    // 0x0005  s| Pattern       | list[<<ref_PatternStruct>>] | max 16^*^      | N       | 0       | RW VM    | PAT
    // 0x0006  s| Name          | string                      | max 16         | N^*^    | 0       | RW VM    | P, O
    // 0x0007  s| WobbleSupport | <<ref_WobbleBitmap>>        | desc           |         |         | R V      | [WBL]
    // 0x0008  s| WobbleSetting | <<ref_WobbleBitmap>>        | desc           |         |         | RW VM    | [WBL]

    // Attribute reads and writes are indirected to the DiscoBallClusterState by the DiscoBallClusterLogic.
    bool GetRunAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetRunAttribute(bool run_state);

    Clusters::DiscoBall::RotateEnum GetRotateAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetRotateAttribute(Clusters::DiscoBall::RotateEnum rotate_state);

    uint8_t GetSpeedAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetSpeedAttribute(uint8_t speed);

    uint8_t GetAxisAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetAxisAttribute(uint8_t axis);

    uint8_t GetWobbleSpeedAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetWobbleSpeedAttribute(uint8_t wobble_speed);

    size_t GetNumPatterns(FabricIndex fabric_idx) const;
    chip::Optional<DiscoBallPatternStructBacking> GetPatternAttributeEntry(FabricIndex fabric_idx, size_t pattern_idx) const;
    CHIP_ERROR ClearPattern(FabricIndex fabric_idx, size_t pattern_idx);
    Protocols::InteractionModel::ClusterStatusCode SetPattern(FabricIndex fabric_idx, const Clusters::DiscoBall::Structs::PatternStruct::Type & pattern);

    CharSpan GetNameAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetNameAttribute(CharSpan name);

    BitFlags<Clusters::DiscoBall::WobbleBitmap> GetWobbleSupportAttribute() const;

    BitFlags<Clusters::DiscoBall::WobbleBitmap> GetWobbleSettingAttribute() const;
    Protocols::InteractionModel::ClusterStatusCode SetWobbleSettingAttribute(BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting);

    BitFlags<Clusters::DiscoBall::Feature> GetSupportedFeatures() const;

    // 0x00  s| StartRequest   | client => server | Y                 | O T^*^ | M
    // 0x01  s| StopRequest    | client => server | Y                 | O      | M
    // 0x02  s| ReverseRequest | client => server | Y                 | O      | REV
    // 0x03  s| WobbleRequest  | client => server | Y                 | O      | WBL
    // 0x04  s| PatternRequest | client => server | Y                 | M      | PAT
    // 0x05  s| StatsRequest   | client => server | StatsResponse^**^ | O      | STA
    // 0x06  s| StatsResponse  | client <= server | N                 | O      | STA
    Protocols::InteractionModel::ClusterStatusCode HandleStartRequest(const Clusters::DiscoBall::Commands::StartRequest::DecodableType & args);
    Protocols::InteractionModel::ClusterStatusCode HandleStopRequest();
    Protocols::InteractionModel::ClusterStatusCode HandleReverseRequest();
    Protocols::InteractionModel::ClusterStatusCode HandleWobbleRequest();
    Protocols::InteractionModel::ClusterStatusCode HandlePatternRequest(FabricIndex fabric_index, const Clusters::DiscoBall::Commands::PatternRequest::DecodableType & args);
    Protocols::InteractionModel::ClusterStatusCode HandleStatsRequest(Clusters::DiscoBall::Commands::StatsResponse::Type & out_stats_response);

private:
    EndpointId mEndpointId = kInvalidEndpointId;
    DriverInterface * mDriver = nullptr;

    DiscoBallCapabilities mCapabilities;

    // All possibly volatile data state the for the cluster.
    DiscoBallClusterState mClusterState{};
};

} // namespace app
} // namespace chip
