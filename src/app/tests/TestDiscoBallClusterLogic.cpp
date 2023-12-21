/*
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

#include <app/clusters/disco-ball-server/disco-ball-cluster-logic.h>

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <nlunit-test.h>
#include <lib/core/CHIPError.h>
#include <lib/core/CHIPPersistentStorageDelegate.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/support/BitFlags.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>
#include <lib/support/UnitTestExtendedAssertions.h>
#include <lib/support/UnitTestRegistration.h>
#include <lib/support/logging/CHIPLogging.h>

#include <protocols/interaction_model/StatusCode.h>

#include <app/ConcreteAttributePath.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enums.h>

using namespace chip;
using namespace chip::app;

using ::chip::Protocols::InteractionModel::ClusterStatusCode;
using ::chip::Protocols::InteractionModel::Status;

namespace {

constexpr EndpointId kExpectedEndpointId = 1;

class FakeDiscoBallDriver: public DiscoBallClusterLogic::DriverInterface
{
  public:
    void SetCapabilities(DiscoBallCapabilities capabilities) { mCapabilities = capabilities; }
    void ForcePatternTimerToExpire()
    {
        VerifyOrReturn(mLastTimerCallback != nullptr);

        auto callback = mLastTimerCallback;
        void * ctx = mLastTimerCallbackContext;
        mLastTimerCallback = nullptr;
        mLastTimerCallbackContext = nullptr;

        callback(kExpectedEndpointId, ctx);
    }

    int GetStartRequestCount() const { return mStartRequestCount; }
    int GetStopRequestCount() const { return mStopRequestCount; }
    int GetClusterStateChangeCount() const { return mClusterStateChangeCount; }
    uint16_t GetLastTimerNumSeconds() const { return mLastTimerNumSeconds; }

    void ResetCounts()
    {
        mStartRequestCount = 0;
        mStopRequestCount = 0;
        mClusterStateChangeCount = 0;
    }

    void ClearDirtyPaths()
    {
        mDirtyPaths.clear();
    }

    bool HasDirtyPath(const chip::app::ConcreteAttributePath& path) const
    {
        return std::find(mDirtyPaths.cbegin(), mDirtyPaths.cend(), path) != mDirtyPaths.cend();
    }

    void Reset()
    {
        ResetCounts();
        ClearDirtyPaths();

        mLastTimerNumSeconds = 0;
        mLastTimerCallback = nullptr;
        mLastTimerCallbackContext = nullptr;
    }

    // DiscoBallClusterLogic::DriverInterface
    DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const override
    {
        return mCapabilities;
    }

    Status OnClusterStateChange(EndpointId endpoint_id, BitFlags<DiscoBallFunction> changes, DiscoBallClusterLogic & cluster) override
    {
        ChipLogProgress(Zcl, "DriverInterface::OnClusterStateChange called, changes: 0x%x", static_cast<unsigned>(changes.Raw()));
        VerifyOrReturnValue(endpoint_id == kExpectedEndpointId, Status::UnsupportedEndpoint);
        ++mClusterStateChangeCount;

        if (changes.Has(DiscoBallFunction::kRunning))
        {
            if (cluster.GetRunAttribute())
            {
                ++mStartRequestCount;
            }
            else
            {
                ++mStopRequestCount;
            }
        }
        return Status::Success;
    }

    void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoBallTimerCallback timer_cb, void * ctx) override
    {
        ChipLogProgress(Zcl, "DriverInterface::StartPatternTimer called for %d seconds", static_cast<int>(num_seconds));
        VerifyOrDie(endpoint_id == kExpectedEndpointId);
        mLastTimerNumSeconds = num_seconds;
        mLastTimerCallback = timer_cb;
        mLastTimerCallbackContext = ctx;
    }

    void CancelPatternTimer(EndpointId endpoint_id) override
    {
        ChipLogProgress(Zcl, "DriverInterface::CancelPatternTimer called");
        VerifyOrDie(endpoint_id == kExpectedEndpointId);
        mLastTimerNumSeconds = 0;
        mLastTimerCallback = nullptr;
        mLastTimerCallbackContext = nullptr;
    }

    void MarkAttributeDirty(const chip::app::ConcreteAttributePath& path) override
    {
        ChipLogProgress(Zcl, "DriverInterface::MarkAttributeDirty called for <EP%u/0x%04x/0x%04x>",
            static_cast<unsigned>(path.mEndpointId), static_cast<unsigned>(path.mClusterId), static_cast<unsigned>(path.mAttributeId));
        VerifyOrDie(path.mEndpointId == kExpectedEndpointId);
        mDirtyPaths.push_back(path);
    }


  private:
    DiscoBallCapabilities mCapabilities{};
    int mStartRequestCount = 0;
    int mStopRequestCount = 0;
    int mClusterStateChangeCount = 0;
    uint16_t mLastTimerNumSeconds = 0;
    DiscoBallTimerCallback mLastTimerCallback = nullptr;
    void * mLastTimerCallbackContext = nullptr;
    std::vector<chip::app::ConcreteAttributePath> mDirtyPaths{};
};

class FakeDiscoBallStorage : public DiscoBallClusterState::NonVolatileStorageInterface
{
  public:
    void ClearStorage() { mStorageSaved = false; }
    int GetLoadFromStorageCount() const { return mLoadFromStorageCount; }
    int GetSaveToStorageCount() const { return mSaveToStorageCount; }
    void ResetCounts()
    {
        mLoadFromStorageCount = 0;
        mSaveToStorageCount = 0;
    }

    // Implementation of DiscoBallClusterState::NonVolatileStorageInterface
    CHIP_ERROR SaveToStorage(const DiscoBallClusterState & attributes) override
    {
        ++mSaveToStorageCount;
        VerifyOrReturnError(attributes.num_patterns < DiscoBallClusterState::kNumPatterns, CHIP_ERROR_INVALID_ARGUMENT);

        VerifyOrReturnError(mClusterState.SetNameAttribute(attributes.name_attribute), CHIP_ERROR_INVALID_ARGUMENT);

        mClusterState.num_patterns = attributes.num_patterns;
        for (size_t pattern_idx = 0; pattern_idx < mClusterState.num_patterns; ++pattern_idx)
        {
            mClusterState.pattern_attribute[pattern_idx] = attributes.pattern_attribute[pattern_idx];
        }

        ChipLogProgress(Zcl, "Stored %d patterns", static_cast<int>(mClusterState.num_patterns));
        mStorageSaved = true;
        return CHIP_NO_ERROR;
    }

    // TODO: Test corrupted storage
    CHIP_ERROR LoadFromStorage(DiscoBallClusterState & attributes) override
    {
        ++mLoadFromStorageCount;
        VerifyOrReturnError(mStorageSaved == true, CHIP_ERROR_NOT_FOUND);

        VerifyOrDie(mClusterState.num_patterns < DiscoBallClusterState::kNumPatterns);
        VerifyOrDie(attributes.SetNameAttribute(mClusterState.name_attribute));

        attributes.num_patterns = mClusterState.num_patterns;
        for (size_t pattern_idx = 0; pattern_idx < mClusterState.num_patterns; ++pattern_idx)
        {
            attributes.pattern_attribute[pattern_idx] = mClusterState.pattern_attribute[pattern_idx];
        }

        ChipLogProgress(Zcl, "Loaded %d patterns", static_cast<int>(mClusterState.num_patterns));
        return CHIP_NO_ERROR;
    }

    void RemoveDataForFabric(FabricIndex fabric_index) override { mClusterState = DiscoBallClusterState{}; }
    void RemoveDataForAllFabrics() override {  mClusterState = DiscoBallClusterState{}; }

  private:
    int mLoadFromStorageCount = 0;
    int mSaveToStorageCount = 0;
    bool mStorageSaved = false;
    DiscoBallClusterState mClusterState{};
};

chip::app::DiscoBallCapabilities BuildFullFeaturedCapabilities()
{
    chip::app::DiscoBallCapabilities capabilities;

    capabilities.supported_features.Set(chip::app::Clusters::DiscoBall::Feature::kAxis);
    capabilities.supported_features.Set(chip::app::Clusters::DiscoBall::Feature::kWobble);
    capabilities.supported_features.Set(chip::app::Clusters::DiscoBall::Feature::kPattern);
    capabilities.supported_features.Set(chip::app::Clusters::DiscoBall::Feature::kStatistics);
    capabilities.supported_features.Set(chip::app::Clusters::DiscoBall::Feature::kReverse);

    // TODO: Make min speed larger
    capabilities.min_speed_value = 0;
    capabilities.max_speed_value = 200;

    capabilities.min_axis_value = 0;
    capabilities.max_axis_value = 90;

    // Only valid if supported_features.Has(DiscoBall::Feature::kWobble)
    capabilities.min_wobble_speed_value = 0;
    capabilities.max_wobble_speed_value = 200;

    // Only valid if supported_features.Has(DiscoBall::Feature::kWobble)
    capabilities.wobble_support.Set(chip::app::Clusters::DiscoBall::WobbleBitmap::kWobbleUpDown);
    capabilities.wobble_support.Set(chip::app::Clusters::DiscoBall::WobbleBitmap::kWobbleLeftRight);
    capabilities.wobble_support.Set(chip::app::Clusters::DiscoBall::WobbleBitmap::kWobbleRound);

    return capabilities;
}

class DiscoBallTestContext
{
  public:
    DiscoBallTestContext() = default;
    ~DiscoBallTestContext() = default;

    void Setup()
    {

        mStorage = std::make_unique<FakeDiscoBallStorage>();
        mDriver = std::make_unique<FakeDiscoBallDriver>();

        // Assume full-featured by default
        mDriver->SetCapabilities(BuildFullFeaturedCapabilities());

        mCluster = std::make_unique<DiscoBallClusterLogic>();

        VerifyOrDie(mCluster->Init(kExpectedEndpointId, *mStorage, *mDriver) == CHIP_NO_ERROR);
    }

    void Teardown()
    {
        mCluster->Deinit();

        mCluster.reset();
        mDriver.reset();
        mStorage.reset();
    }

    FakeDiscoBallStorage & storage() { return *mStorage; }
    FakeDiscoBallDriver & driver() { return *mDriver; }
    DiscoBallClusterLogic & cluster() { return *mCluster; }
  private:
    std::unique_ptr<FakeDiscoBallStorage> mStorage;
    std::unique_ptr<FakeDiscoBallDriver> mDriver;
    std::unique_ptr<DiscoBallClusterLogic> mCluster;
};

const chip::app::ConcreteAttributePath kRunAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::Run::Id};
const chip::app::ConcreteAttributePath kRotateAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::Rotate::Id};
const chip::app::ConcreteAttributePath kSpeedAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::Speed::Id};
const chip::app::ConcreteAttributePath kAxisAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::Axis::Id};
const chip::app::ConcreteAttributePath kWoobleSpeedAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::WobbleSpeed::Id};
const chip::app::ConcreteAttributePath kPatternAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::Pattern::Id};
const chip::app::ConcreteAttributePath kNameAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::Name::Id};
const chip::app::ConcreteAttributePath kWobbleSupportAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::WobbleSupport::Id};
const chip::app::ConcreteAttributePath kWobbleSettingAttributePath{kExpectedEndpointId, Clusters::DiscoBall::Id, Clusters::DiscoBall::Attributes::WobbleSetting::Id};

void TestDiscoBallInitialization(nlTestSuite * inSuite, void * inContext)
{
    // ARRANGE: setup basic fake environment.
    FakeDiscoBallStorage storage;
    FakeDiscoBallDriver driver;

    {
        DiscoBallClusterLogic cluster;
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetLoadFromStorageCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetSaveToStorageCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetEndpointId(), kInvalidEndpointId);

        // ACT: Initialize the cluster.
        NL_TEST_ASSERT_SUCCESS(inSuite, cluster.Init(kExpectedEndpointId, storage, driver));

        // ASSERT: Validate storage was loaded, endpoint correctly set, getting setting works.
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetLoadFromStorageCount(), 1);
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetSaveToStorageCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetEndpointId(), kExpectedEndpointId);
        NL_TEST_ASSERT(inSuite, cluster.GetNameAttribute().empty());

        // ACT AGAIN: Set an attribute that requires init.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.SetNameAttribute("hello"_span), Status::Success);

        // ASSERT AGAIN: Check storage was attempted (due to NV attribute set).
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetSaveToStorageCount(), 1);
        NL_TEST_ASSERT(inSuite, cluster.GetNameAttribute().data_equal("hello"_span));

        // ACT AGAIN: Deinit().
        cluster.Deinit();

        // ASSERT AGAIN: Endpoint returns to invalid, storage fails.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetEndpointId(), kInvalidEndpointId);
        NL_TEST_ASSERT(inSuite, cluster.SetNameAttribute("hello2"_span) != Status::Success);
    }

    // Init a second instance with same storage, ensure it gets loaded properly.
    storage.ResetCounts();

    {
        DiscoBallClusterLogic cluster;
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetLoadFromStorageCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetSaveToStorageCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetEndpointId(), kInvalidEndpointId);

        // ACT: Initialize the cluster.
        NL_TEST_ASSERT_SUCCESS(inSuite, cluster.Init(kExpectedEndpointId, storage, driver));

        // ASSERT: Validate storage was loaded, endpoint correctly set, getting setting works.
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetLoadFromStorageCount(), 1);
        NL_TEST_ASSERT_EQUALS(inSuite, storage.GetSaveToStorageCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetEndpointId(), kExpectedEndpointId);
        NL_TEST_ASSERT(inSuite, cluster.GetNameAttribute().data_equal("hello"_span));
    }
}

void TestDiscoBallRunning(nlTestSuite * inSuite, void * inContext)
{
    // ARRANGE: Get the fresh-from-init context.
    DiscoBallTestContext * context = static_cast<DiscoBallTestContext*>(inContext);
    DiscoBallClusterLogic & cluster = context->cluster();
    FakeDiscoBallDriver & driver = context->driver();

    // Step 1: Start.
    {
        // ACT + ASSERT: Force running, check running.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), false);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 0);

        Clusters::DiscoBall::Commands::StartRequest::DecodableType start_args;
        start_args.speed = 100;
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleStartRequest(start_args), Status::Success);

        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), true);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 100);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kClockwise);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetAxisAttribute(), 0);

        // ASSERT: Validate that driver was correctly called and only changed attributes marked dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 1);
        // TODO: add checks that speed was set against the driver.
        // TODO: add test for rotate attribute setting
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 0);
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kSpeedAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRotateAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kAxisAttributePath));
    }

    driver.ResetCounts();
    driver.ClearDirtyPaths();

    // Step 2: Start again when already started, but modifying speed.
    {
        // ACT + ASSERT: Force running again, check still running.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), true);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 100);

        Clusters::DiscoBall::Commands::StartRequest::DecodableType start_args;
        start_args.speed = 50;
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleStartRequest(start_args), Status::Success);

        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), true);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 50);

        // ASSERT: Validate that driver was correctly called and only changed attributes marked dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 0);
        // TODO: add checks that speed was set against the driver.
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRotateAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kAxisAttributePath));

        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kSpeedAttributePath));
    }

    driver.ResetCounts();
    driver.ClearDirtyPaths();

    // Step 3: Stop and check stopped.
    {
        // ACT + ASSERT: Force stop, check stopped.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleStopRequest(), Status::Success);

        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), false);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 0);
// TODO: FIX the expected rotation
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kClockwise);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetAxisAttribute(), 0);

        // ASSERT: Validate that driver was correctly called and only changed attributes marked dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 1);
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kSpeedAttributePath));
// TODO: Fix the expected rotation
        // NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRotateAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kAxisAttributePath));
    }
}

void TestDiscoBallReverseRequestWhileRunningSucceeds(nlTestSuite * inSuite, void * inContext)
{
    // ARRANGE: Get the fresh-from-init context.
    DiscoBallTestContext * context = static_cast<DiscoBallTestContext*>(inContext);
    DiscoBallClusterLogic & cluster = context->cluster();
    FakeDiscoBallDriver & driver = context->driver();

    // Step 1: Start counter-clockwise.
    {
        // ACT + ASSERT: Start running counter-clockwise, check running.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), false);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kClockwise);

        Clusters::DiscoBall::Commands::StartRequest::DecodableType start_args;
        start_args.speed = 100;
        start_args.rotate = chip::MakeOptional(Clusters::DiscoBall::RotateEnum::kCounterClockwise);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleStartRequest(start_args), Status::Success);

        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), true);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 100);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kCounterClockwise);

        // ASSERT: Validate that driver was correctly called and only changed attributes marked dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 1);

        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 0);
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kSpeedAttributePath));
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRotateAttributePath));
    }

    driver.ResetCounts();
    driver.ClearDirtyPaths();


    // Step 2: Reverse, check now clockwise.
    {
        // ACT: Reverse.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleReverseRequest(), Status::Success);

        // ASSERT: Validate we are now reversed (clockwise) and still running.

        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), true);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 100);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kClockwise);

        // ASSERT: Validate that driver was correctly called and only changed attributes marked dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetClusterStateChangeCount(), 1);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 0);
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kSpeedAttributePath));
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRotateAttributePath));
    }

    driver.ResetCounts();
    driver.ClearDirtyPaths();

    // Step 3: Reverse, check now counter-clockwise.
    {
        // ACT: Reverse.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleReverseRequest(), Status::Success);

        // ASSERT: Validate we are now reversed (counter-clockwise) and still running.

        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), true);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 100);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kCounterClockwise);

        // ASSERT: Validate that driver was correctly called and only changed attributes marked dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetClusterStateChangeCount(), 1);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 0);
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kSpeedAttributePath));
        NL_TEST_ASSERT(inSuite, driver.HasDirtyPath(kRotateAttributePath));
    }
}

void TestDiscoBallReverseRequestWhileStoppedFails(nlTestSuite * inSuite, void * inContext)
{
    // ARRANGE: Get the fresh-from-init context.
    DiscoBallTestContext * context = static_cast<DiscoBallTestContext*>(inContext);
    DiscoBallClusterLogic & cluster = context->cluster();
    FakeDiscoBallDriver & driver = context->driver();

    NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), false);
    NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 0);
    NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kClockwise);

    {
        // ACT: Try to reverse while stopped.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.HandleReverseRequest(), Status::InvalidInState);

        // ASSERT: Validates it didn't change state, and nothing dirty.
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRunAttribute(), false);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetSpeedAttribute(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetRotateAttribute(), Clusters::DiscoBall::RotateEnum::kClockwise);

        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStartRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetStopRequestCount(), 0);
        NL_TEST_ASSERT_EQUALS(inSuite, driver.GetClusterStateChangeCount(), 0);

        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRunAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kSpeedAttributePath));
        NL_TEST_ASSERT(inSuite, !driver.HasDirtyPath(kRotateAttributePath));
    }
}

const nlTest sLifecyleTests[] = {
    NL_TEST_DEF("Test Disco Ball initialization error handling", TestDiscoBallInitialization),
    NL_TEST_SENTINEL()
};

const nlTest sLogicTests[] = {
    NL_TEST_DEF("Test Disco Ball start/stop", TestDiscoBallRunning),
    NL_TEST_DEF("Test ReverseRequest while running", TestDiscoBallReverseRequestWhileRunningSucceeds),
    NL_TEST_DEF("Test ReverseRequest while stopped", TestDiscoBallReverseRequestWhileStoppedFails),
    NL_TEST_SENTINEL()
};

int InitializeSingleTest(void * inContext)
{
    DiscoBallTestContext * context = static_cast<DiscoBallTestContext*>(inContext);
    context->Setup();
    return SUCCESS;
}

int TerminateSingleTest(void * inContext)
{
    DiscoBallTestContext * context = static_cast<DiscoBallTestContext*>(inContext);
    context->Teardown();
    return SUCCESS;
}

int TestSetup(void * inContext)
{
    VerifyOrReturnError(CHIP_NO_ERROR == chip::Platform::MemoryInit(), FAILURE);
    return SUCCESS;
}

int TestTearDown(void * inContext)
{
    chip::Platform::MemoryShutdown();
    return SUCCESS;
}

} // namespace

int TestDiscoBallClusterLogicLifecycle()
{
    nlTestSuite theSuite = { "Test Disco Ball Cluster's common portable logic lifecycle", &sLifecyleTests[0], TestSetup, TestTearDown };
    nlTestRunner(&theSuite, nullptr);
    return nlTestRunnerStats(&theSuite);
}

int TestDiscoBallClusterLogic()
{
    // The context has dynamic re-init for each test.
    DiscoBallTestContext context_for_whole_test;

    nlTestSuite theSuite = { "Test Disco Ball Cluster's common portable logic", &sLogicTests[0], TestSetup, TestTearDown, InitializeSingleTest, TerminateSingleTest };
    nlTestRunner(&theSuite, &context_for_whole_test);
    return nlTestRunnerStats(&theSuite);
}

CHIP_REGISTER_TEST_SUITE(TestDiscoBallClusterLogicLifecycle)
CHIP_REGISTER_TEST_SUITE(TestDiscoBallClusterLogic)
