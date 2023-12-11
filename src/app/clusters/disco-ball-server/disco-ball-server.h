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

#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>
#include <lib/support/BitFlags.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enum.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterface.h>

namespace chip {
namespace app {

class DiscoBallPatternStructBacking
{
public:
    static constexpr size_t kPasscodeMaxSize = 6;

    DiscoBallPatternStructBacking() = default;

    CHIP_ERROR SetValue(const Clusters::DiscoBall::Structs::PatternStruct::Type & value);
    const Clusters::DiscoBall::Structs::PatternStruct::Type & GetValue() const;
private:
    Clusters::DiscoBall::Structs::PatternStruct::Type mStorage;
    // Storage for the passcode (if present). `mStorage`'s `passcode` being present with `.size()` > 0
    // indicates that the `.data()` will point to this.
    char mPasscodeBacking[kPasscodeMaxSize];
};

class DiscoBallClusterState
{
public:
    // TODO: Not clear that it's 16 total, or 16 per fabric. Assume 16 total.
    // TODO: Not clear what to do when a pattern recall with password is used...
    static constexpr size_t kNumPatterns = 16;
    static constexpr size_t kNameMaxSize = 32; // Using SuperDiscoBall superset value

    class NonVolatileStorageInterface
    {
    public:
        virtual ~NonVolatileStorage = default;

        virtual CHIP_ERROR SaveToStorage(const ClusterState & attributes) = 0;
        virtual CHIP_ERROR LoadFromStorage(ClusterState & attributes) = 0;
    };

    DiscoBallClusterState();

// TODO: Consider observer registration to record attribute changes from driver ?
    CHIP_ERROR Init(NonVolatileStorageInterface & storage);
    void Deinit()
    {
        mStorage = nullptr;
        mInitialized = false;
    }

    bool IsInitialized() const { return mIsInitialized; }

    // No shutdown: all non-volatile mutations are immediately stored.

    CHIP_ERROR ReloadFromStorage();
    CHIP_ERROR SaveToStorage();

// TODO: Record supported features (from driver!!!)
// TODO: Label

    bool GetRun() const;
    void SetRunState(bool run_state);

    DiscoBall::RotateEnum GetRotate() const;
    void SetRotateState(bool DiscoBall::RotateEnum rotate_state);

    uint8_t GetSpeed() const;
    void SetSpeed(bool DiscoBall::RotateEnum rotate_state);

    uint8_t GetAxis() const;
    void SetAxis(uint8_t axis);
    InteractionModel::Status SetAxis(uint8_t axis);

    uint8_t GetWobbleSpeed() const;
    InteractionModel::Status SetWobbleSpeed(uint8_t wobble_speed);

    size_t GetNumPatterns() const;
    DiscoBallPatternStructBacking GetPatternListEntry(size_t pattern_idx) const;
    CHIP_ERROR ClearPattern(size_t pattern_idx);
    InteractionModel::Status SetPattern(size_t pattern_idx, const Clusters::DiscoBall::Structs::PatternStruct::Type & pattern);

    CharSpan GetName() const;
    InteractionModel::Status SetName(CharSpan name);

    BitFlags<DiscoBall::WobbleBitmap> GetWobbleSupport() const;
    void SetWobbleSupport(BitFlags<DiscoBall::WobbleBitmap>) const;

    BitFlags<DiscoBall::WobbleBitmap> GetWobbleSetting() const;
    InteractionModel::Status SetWobbleSetting(BitFlags<DiscoBall::WobbleBitmap> wobble_setting);

private:
    bool mIsInitialized = false;
    NonVolatileStorageInterface * mStorage = nullptr;

    // For StatsRequest command.
    uint32_t mLastRunStatistic;
    uint32_t mPatternsStatistic;

    // ------------------------
    // Attributes storage start
    // ------------------------

    // 0x0000  s| Run           | bool                        | all^*^         |         | 0       | R V T^*^ | M
    bool mRun;

    // 0x0001  s| Rotate        | <<ref_RotateEnum>>          | all            |         | 0       | R V      | M
    DiscoBall::RotateEnum mRotate;

    // 0x0002  s| Speed         | uint8                       | 0 to 200^*^    |         | 0       | R V      | M
    uint8_t mSpeed;

    // 0x0003  s| Axis          | uint8                       | 0 to 90        |         | 0       | RW VO    | AX \| WBL
    uint8_t mAxis;

    // 0x0004  s| WobbleSpeed   | uint8                       | 0 to 200       |         | 0       | RW VO    | WBL
    uint8_t mWobbleSpeed;

    // 0x0005  s| Pattern       | list[<<ref_PatternStruct>>] | max 16^*^      | N       | 0       | RW VM    | PAT
    size_t mNumPatterns = 0;
    DiscoBallPatternStructBacking mPattern[kNumPatterns];

    // 0x0006  s| Name          | string                      | max 16         | N^*^    | 0       | RW VM    | P, O
    char mNameBacking[kNameMaxSize];
    CharSpan mName;

    // 0x0007  s| WobbleSupport | <<ref_WobbleBitmap>>        | desc           |         |         | R V      | [WBL]
    BitFlags<DiscoBall::WobbleBitmap> mWobbleSupport;

    // 0x0008  s| WobbleSetting | <<ref_WobbleBitmap>>        | desc           |         |         | RW VM    | [WBL]
    BitFlags<DiscoBall::WobbleBitmap> mWobbleSetting;

    // ----------------------
    // Attributes storage end
    // ----------------------
};

// TODO: Account for MS commands
// TODO: How to go from DriverInterface to attribute changes? Observer?
class DiscoBallDriverInterface
{
public:
    class DiscoTimerDelegate
    {
    public:
        virtual OnPatternTimerHit(EndpointId id, void *ctx);
    }

    virtual BitFlags<DiscoBall::Feature> GetSupportedFeatures(EndpointId endpoint_id) const = 0;
    virtual BitFlags<DiscoBall::WobbleBitmap> GetWobbleSupport(EdnpointId endpoint_id) const = 0;

    virtual CHIP_ERROR OnStartRequest(EndpointId endpoint_id, DiscoBallClusterState & cluster_state) = 0;
    virtual CHIP_ERROR OnStopRequest(EndpointId endpoint_id, DiscoBallClusterState & cluster_state) = 0;
    virtual CHIP_ERROR OnClusterStateChange(EndpointId endpoint_id, DiscoBallClusterState & cluster_state) = 0;
    virtual void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoTimerDelegate & timer_delegate, void * ctx) = 0
    virtual void CancelPatternTimer(EndpointId endpoint_id) = 0;
};

class DiscoBallClusterLogic
{
public:
// TODO: Add cluster driver
    DiscoBallClusterLogic() = default;

    // Not copyable.
    DiscoBallClusterLogic(DiscoBallClusterLogic const&) = delete;
    DiscoBallClusterLogic& operator=(DiscoBallClusterLogic const&) = delete;

    CHIP_ERROR Init(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DiscoBallDriverInterface & driver);
    void Deinit()
    {
        mDriver = nullptr;
        mEndpointId = kInvalidEndpointId;
    }

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
    DiscoBallClusterState & GetClusterState() { return mClusterState; }

    // 0x00  s| StartRequest   | client => server | Y                 | O T^*^ | M
    // 0x01  s| StopRequest    | client => server | Y                 | O      | M
    // 0x02  s| ReverseRequest | client => server | Y                 | O      | REV
    // 0x03  s| WobbleRequest  | client => server | Y                 | O      | WBL
    // 0x04  s| PatternRequest | client => server | Y                 | M      | PAT
    // 0x05  s| StatsRequest   | client => server | StatsResponse^**^ | O      | STA
    // 0x06  s| StatsResponse  | client <= server | N                 | O      | STA
    InteractionModel::Status HandleStartRequest(const Clusters::DiscoBall::Commands::StartRequest::DecodableType & args);
    InteractionModel::Status HandleStopRequest();
    InteractionModel::Status HandleReverseRequest();
    InteractionModel::Status HandleWobbleRequest();
    InteractionModel::Status HandlePatternRequest(const Clusters::DiscoBall::Commands::PatternRequest::DecodableType & args);
    InteractionModel::Status HandleStatsRequest(Clusters::DiscoBall::Commands::StatsResponse::Type & out_stats_response);

    DiscoBallDriverInterface & GetDriver() { return *mDriver; }
    EndpointId GetEndpointId() const { return mEndpointId; }

private:
    EndpointId mEndpointId = kInvalidEndpointId;
    DiscoBallDriverInterface * mDriver = nullptr;

    // All data state the for the cluster.
    DiscoBallClusterState mClusterState{};
};

class DiscoBallServer : public CommandHandlerInterface, public AttributeAccessInterface
{
public:
    // This is the disco ball command handler for all endpoints (endpoint ID is NullOptional)
    DiscoBallServer() : CommandHandlerInterface(chip::NullOptional, Clusters::DiscoBall::Id), AttributeAccessInterface(chip::NullOptional, Clusters::DiscoBall::Id) {}
    ~DiscoBallServer() = default;

    // Inherited from CommandHandlerInterface
    void InvokeCommand(HandlerContext & handlerContext) override;

    // AttributeAccessInterface
    CHIP_ERROR Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override;
    CHIP_ERROR Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder) override;

    CHIP_ERROR RegisterEndpoint(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DiscoBallDriverInterface & driver);
    void UnregisterEndpoint(EndpointId endpoint_id);
    DiscoBallClusterLogic * FindEndpoint(EndpointId endpoint_id);

private:
// TODO: Handle dynamic memory management of cluster state.
    DiscoBallClusterLogic mEndpoints[1];
};

} // namespace app
} // namespace chip
