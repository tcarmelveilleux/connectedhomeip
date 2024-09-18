/*
 *
 *    Copyright (c) 2024 Project CHIP Authors
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

#include <functional>

#include <stdbool.h>
#include <stdint.h>

#include "GenericSwitchStateMachine.h"

#include <system/SystemClock.h>
#include <system/SystemLayer.h>

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-enums.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/EventLogging.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/logging/CHIPLogging.h>
#include <lib/support/BitFlags.h>
#include <lib/support/IntrusiveList.h>
#include <platform/CHIPDeviceLayer.h>

namespace chip {
namespace app {

class DefaultGenericSwitchStateMachineDriver : public GenericSwitchStateMachine::Driver
{
  public:
    DefaultGenericSwitchStateMachineDriver() 
    {
        mFeatures
            .Set(Clusters::Switch::Feature::kActionSwitch)
            .Set(Clusters::Switch::Feature::kMomentarySwitch)
            .Set(Clusters::Switch::Feature::kMomentarySwitchLongPress)
            .Set(Clusters::Switch::Feature::kMomentarySwitchMultiPress);
    }

    // Starts/restarts a timer for timeoutMillis ms, with given `ctx` context pointer. Returns true on success.
    bool StartLongPressTimer(uint32_t timeoutMillis, GenericSwitchStateMachine *stateMachine) override
    {
        mLongPressTimerWrapper = CallbackWrapper{[stateMachine]() {
            stateMachine->HandleEvent(GenericSwitchStateMachine::Event(GenericSwitchStateMachine::Event::Type::kLongPressTimerHit));
        }};
        CHIP_ERROR err = chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Timeout(timeoutMillis), &CallbackWrapperTrampoline, (void *)&mLongPressTimerWrapper);

        return err == CHIP_NO_ERROR;
    }

    void CancelLongPressTimer() override
    {
        chip::DeviceLayer::SystemLayer().CancelTimer(&CallbackWrapperTrampoline, (void *)&mLongPressTimerWrapper);
    }

    bool StartWaitIdleTimer(uint32_t timeoutMillis, GenericSwitchStateMachine *stateMachine) override
    {
        mIdleTimerWrapper = CallbackWrapper{[stateMachine]() {
            stateMachine->HandleEvent(GenericSwitchStateMachine::Event(GenericSwitchStateMachine::Event::Type::kIdleTimerHit));
        }};
        CHIP_ERROR err = chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Timeout(timeoutMillis), &CallbackWrapperTrampoline, (void *)&mIdleTimerWrapper);

        return err == CHIP_NO_ERROR;
    }

    void CancelWaitIdleTimer() override
    {
        chip::DeviceLayer::SystemLayer().CancelTimer(&CallbackWrapperTrampoline, (void *)&mIdleTimerWrapper);
    }

    // System adaptation
    void EmitInitialPress(uint8_t newPosition) override
    {
        Clusters::Switch::Events::InitialPress::Type event{ newPosition };
        EventNumber eventNumber = 0;

        CHIP_ERROR err = LogEvent(event, mEndpointId, eventNumber);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to log InitialPress event: %" CHIP_ERROR_FORMAT, err.Format());
        }
        else
        {
            ChipLogProgress(NotSpecified, "Logged InitialPress(%u) on Endpoint %u", static_cast<unsigned>(newPosition),
                            static_cast<unsigned>(mEndpointId));
        }
    }

    void EmitLongPress(uint8_t newPosition) override
    {
        Clusters::Switch::Events::LongPress::Type event{ newPosition };
        EventNumber eventNumber = 0;

        CHIP_ERROR err = LogEvent(event, mEndpointId, eventNumber);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to log LongPress event: %" CHIP_ERROR_FORMAT, err.Format());
        }
        else
        {
            ChipLogProgress(NotSpecified, "Logged LongPress(%u) on Endpoint %u", static_cast<unsigned>(newPosition),
                            static_cast<unsigned>(mEndpointId));
        }
    }

    void EmitLongRelease(uint8_t prevPosition) override
    {
        Clusters::Switch::Events::LongRelease::Type event{};
        event.previousPosition  = prevPosition;
        EventNumber eventNumber = 0;

        CHIP_ERROR err = LogEvent(event, mEndpointId, eventNumber);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to log LongRelease event: %" CHIP_ERROR_FORMAT, err.Format());
        }
        else
        {
            ChipLogProgress(NotSpecified, "Logged LongRelease on Endpoint %u", static_cast<unsigned>(mEndpointId));
        }
    }

    void EmitMultiPressComplete(uint8_t prevPosition, uint8_t numPresses) override
    {
        Clusters::Switch::Events::MultiPressComplete::Type event{};
        event.previousPosition            = prevPosition;
        event.totalNumberOfPressesCounted = numPresses;
        EventNumber eventNumber           = 0;

        CHIP_ERROR err = LogEvent(event, mEndpointId, eventNumber);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to log MultiPressComplete event: %" CHIP_ERROR_FORMAT, err.Format());
        }
        else
        {
            ChipLogProgress(NotSpecified, "Logged MultiPressComplete(count=%u) on Endpoint %u", static_cast<unsigned>(numPresses),
                            static_cast<unsigned>(mEndpointId));
        }
    }

    void EmitShortRelease(uint8_t prevPosition) override
    {
        Clusters::Switch::Events::ShortRelease::Type event{};
        event.previousPosition            = prevPosition;
        EventNumber eventNumber           = 0;

        CHIP_ERROR err = LogEvent(event, mEndpointId, eventNumber);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to log ShortRelease event: %" CHIP_ERROR_FORMAT, err.Format());
        }
        else
        {
            ChipLogProgress(NotSpecified, "Logged ShortRelease(prev=%u) on Endpoint %u", static_cast<unsigned>(prevPosition),
                            static_cast<unsigned>(mEndpointId));
        }
    }

    void EmitMultiPressOngoing(uint8_t newPosition, uint8_t currentCount) override
    {
        Clusters::Switch::Events::MultiPressOngoing::Type event{};
        event.newPosition = newPosition;
        event.currentNumberOfPressesCounted = currentCount;
        EventNumber eventNumber           = 0;

        CHIP_ERROR err = LogEvent(event, mEndpointId, eventNumber);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to log EmitMultiPressOngoing event: %" CHIP_ERROR_FORMAT, err.Format());
        }
        else
        {
            ChipLogProgress(NotSpecified, "Logged EmitMultiPressOngoing(count=%u) on Endpoint %u", static_cast<unsigned>(currentCount),
                            static_cast<unsigned>(mEndpointId));
        }
    }

    void SetButtonPosition(uint8_t newPosition) override
    {
        Clusters::Switch::Attributes::CurrentPosition::Set(mEndpointId, newPosition);
    }

    BitFlags<Clusters::Switch::Feature> GetSupportedFeatures() const override { return mFeatures; }
    uint8_t GetMultiPressMax() const override { return mMultiPressMax; }
    uint8_t GetNumPositions() const override { return mNumPositions; }

    void SetSupportedFeatures(BitFlags<Clusters::Switch::Feature> features)
    {
        mFeatures = features;
    }

    void SetMultiPressMax(uint8_t multiPressMax) 
    { 
        mMultiPressMax = multiPressMax;
    }

    void SetNumPositions(uint8_t numPositions) 
    { 
        if ((numPositions > 1) && (numPositions < 255))
        {
            mNumPositions = numPositions;
        }
    }
    
  private:
    using StateMachineCallback = std::function<void()>;

    static void CallbackWrapperTrampoline(System::Layer * layer, void * ctx)
    {
        if (ctx != nullptr)
        {
            auto wrapper = reinterpret_cast<CallbackWrapper *>(ctx);
            wrapper->ForwardCall();
        }
    }

    class CallbackWrapper
    {
      public:
        CallbackWrapper() {}
        explicit CallbackWrapper(StateMachineCallback callback) : mCallback(callback) {}

        void ForwardCall()
        {
            mCallback();
        }
      private:
        StateMachineCallback mCallback;
    };

    CallbackWrapper mLongPressTimerWrapper;
    CallbackWrapper mIdleTimerWrapper;

    uint8_t mMultiPressMax = 2u;
    uint8_t mNumPositions = 2u;

    BitFlags<Clusters::Switch::Feature> mFeatures;

};

} // namespace app
} // namespace chip
