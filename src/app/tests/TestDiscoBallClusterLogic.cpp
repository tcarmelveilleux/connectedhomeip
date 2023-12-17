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

#include <nlunit-test.h>
#include <stdint.h>

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

#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enums.h>

using namespace chip;
using namespace chip::app;

namespace {

constexpr EndpointId kExpectedEndpointId = 1;

class FakeDiscoBallDriver: public DiscoBallClusterLogic::DiscoBallDriverInterface
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

    void Reset()
    {
        mStartRequestCount = 0;
        mStopRequestCount = 0;
        mClusterStateChangeCount = 0;
        mLastTimerNumSeconds = 0;
        mLastTimerCallback = nullptr;
        mLastTimerCallbackContext = nullptr;
    }

    // DiscoBallClusterLogic::DiscoBallDriverInterface
    DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const override
    {
        return mCapabilities;
    }

    CHIP_ERROR OnStartRequest(EndpointId endpoint_id, DiscoBallClusterState & cluster_state) override
    {
        ChipLogProgress(Zcl, "DiscoBallDriverInterface::OnStartRequest called");
        VerifyOrReturnError(endpoint_id == kExpectedEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
        ++mStartRequestCount;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR OnStopRequest(EndpointId endpoint_id, DiscoBallClusterState & cluster_state) override
    {
        ChipLogProgress(Zcl, "DiscoBallDriverInterface::OnStopRequest called");
        VerifyOrReturnError(endpoint_id == kExpectedEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
        ++mStopRequestCount;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR OnClusterStateChange(EndpointId endpoint_id, DiscoBallClusterState & cluster_state) override
    {
        ChipLogProgress(Zcl, "DiscoBallDriverInterface::OnClusterStateChange called");
        VerifyOrReturnError(endpoint_id == kExpectedEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
        ++mClusterStateChangeCount;
        return CHIP_NO_ERROR;
    }

    void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoBallTimerCallback timer_cb, void * ctx) override
    {
        ChipLogProgress(Zcl, "DiscoBallDriverInterface::StartPatternTimer called for %d seconds", static_cast<int>(num_seconds));
        VerifyOrDie(endpoint_id == kExpectedEndpointId);
        mLastTimerNumSeconds = num_seconds;
        mLastTimerCallback = timer_cb;
        mLastTimerCallbackContext = ctx;
    }

    void CancelPatternTimer(EndpointId endpoint_id) override
    {
        ChipLogProgress(Zcl, "DiscoBallDriverInterface::CancelPatternTimer called");
        VerifyOrDie(endpoint_id == kExpectedEndpointId);
        mLastTimerNumSeconds = 0;
        mLastTimerCallback = nullptr;
        mLastTimerCallbackContext = nullptr;
    }

  private:
    DiscoBallCapabilities mCapabilities{};
    int mStartRequestCount = 0;
    int mStopRequestCount = 0;
    int mClusterStateChangeCount = 0;
    uint16_t mLastTimerNumSeconds = 0;
    DiscoBallTimerCallback mLastTimerCallback = nullptr;
    void * mLastTimerCallbackContext = nullptr;
};

class FakeDiscoBallStorage : public DiscoBallClusterState::NonVolatileStorageInterface
{
  public:
    void ClearStorage() { mStorageSaved = false; }
    int GetLoadFromStorageCount() { return mLoadFromStorageCount; }
    int GetSaveToStorageCount() { return mSaveToStorageCount; }

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

  private:
    int mLoadFromStorageCount = 0;
    int mSaveToStorageCount = 0;
    bool mStorageSaved = false;
    DiscoBallClusterState mClusterState{};
};


void TestDiscoBallInitialization(nlTestSuite * inSuite, void * inContext)
{
    FakeDiscoBallStorage storage{};
    FakeDiscoBallDriver driver{};

    DiscoBallClusterLogic cluster;

    NL_TEST_ASSERT_EQUALS(inSuite, storage.GetLoadFromStorageCount(), 0);
    NL_TEST_ASSERT_EQUALS(inSuite, storage.GetSaveToStorageCount(), 0);

    NL_TEST_ASSERT_EQUALS(inSuite, cluster.GetEndpointId(), kInvalidEndpointId);

    NL_TEST_ASSERT_SUCCESS(inSuite, cluster.Init(kExpectedEndpointId, storage, driver));

    NL_TEST_ASSERT_EQUALS(inSuite, storage.GetLoadFromStorageCount(), 1);
}

void TestDiscoBallAttributeSetters(nlTestSuite * inSuite, void * inContext)
{
    NL_TEST_ASSERT(inSuite, true);
}

const nlTest sLifecyleTests[] = {
    NL_TEST_DEF("Test Disco Ball initialization error handling", TestDiscoBallInitialization),
    NL_TEST_SENTINEL()
};

const nlTest sLogicTests[] = {
    NL_TEST_DEF("Test Disco Ball attribute setters", TestDiscoBallAttributeSetters),
    NL_TEST_SENTINEL()
};

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
    nlTestSuite theSuite = { "Test Disco Ball Cluster's common portable logic", &sLogicTests[0], TestSetup, TestTearDown };
    nlTestRunner(&theSuite, nullptr);
    return nlTestRunnerStats(&theSuite);
}

CHIP_REGISTER_TEST_SUITE(TestDiscoBallClusterLogicLifecycle)
CHIP_REGISTER_TEST_SUITE(TestDiscoBallClusterLogic)
