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

#include <algorithm>
#include <stdbool.h>
#include <stdint.h>

#include <app-common/zap-generated/cluster-enums.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/BitFlags.h>

namespace chip {
namespace app {

// Supports ActionSwitch MSM and MSL behavior.

class GenericSwitchStateMachine
{
  public:
    struct Event {
        enum class Type : uint8_t {
          kButtonPress = 0,
          kButtonRelease = 1,
          kLongPressTimerHit = 2,
          kIdleTimerHit = 3,
          kLatchSwitchChange = 4,
        };

        Type type;
        union {
            uint8_t buttonPosition;
        };

        explicit Event(Type type_) : type(type_) {}

        static Event MakeButtonPressEvent(uint8_t buttonPos)
        {
            Event event(Type::kButtonPress);
            event.buttonPosition = buttonPos;
            return event;
        }

        static Event MakeButtonReleaseEvent(uint8_t buttonPos)
        {
            Event event(Type::kButtonRelease);
            event.buttonPosition = buttonPos;
            return event;
        }

        static Event MakeLatchSwitchChangeEvent(uint8_t buttonPos)
        {
            Event event(Type::kLatchSwitchChange);
            event.buttonPosition = buttonPos;
            return event;
        }
    };

    enum class State {
      kIdleWaitFirstPress = 0,

      kWaitLongDetermination = 1,
      kLongPressOngoing = 2,
      kLongPressDone = 3,

      kMultiPressReleased = 4,
      kMultiPressPressed = 5,
      kMultiPressDone = 6
    };

    class Driver
    {
      public:
        virtual ~Driver() {}

        // Starts/restarts a timer for timeoutMillis ms, which will call stateMachine->HandleEvent with a kLongPressTimerHit event
        // Returns true on success.
        virtual bool StartLongPressTimer(uint32_t timeoutMillis, GenericSwitchStateMachine *stateMachine) = 0;

        // Starts/restarts a timer for timeoutMillis ms, which will call stateMachine->HandleEvent with a kIdleTimerHit event
        // Returns true on success.
        virtual bool StartWaitIdleTimer(uint32_t timeoutMillis, GenericSwitchStateMachine *stateMachine) = 0;

        virtual void CancelLongPressTimer() = 0;
        virtual void CancelWaitIdleTimer() = 0;

        virtual void EmitInitialPress(uint8_t newPosition) = 0;
        virtual void EmitLongPress(uint8_t newPosition) = 0;
        virtual void EmitLongRelease(uint8_t prevPosition) = 0;
        virtual void EmitMultiPressComplete(uint8_t prevPosition, uint8_t numPresses) = 0;
        virtual void EmitShortRelease(uint8_t prevPosition) = 0;
        virtual void EmitMultiPressOngoing(uint8_t newPosition, uint8_t currentCount) = 0;
  
        // TODO: Revisit this method living in the driver.
        virtual void SetButtonPosition(uint8_t newPosition) = 0;

        virtual BitFlags<Clusters::Switch::Feature> GetSupportedFeatures() const = 0;
        virtual uint8_t GetMultiPressMax() const = 0;
        virtual uint8_t GetNumPositions() const  = 0;

        void SetEndpointId(EndpointId endpointId) { mEndpointId = endpointId; }
      protected:
        EndpointId mEndpointId;
    };

    GenericSwitchStateMachine() {}

    void SetDriver(GenericSwitchStateMachine::Driver *driver) { mDriver = driver; }

    uint32_t GetLongPressThresholdMillis() const { return mLongPressThresholdMillis; }
    uint32_t GetIdleThresholdMillis() const { return mIdleThresholdMillis; }

    void SetLongPressThresholdMillis(uint32_t longPressThresholdMillis)
    {
        mLongPressThresholdMillis = std::max(longPressThresholdMillis, static_cast<uint32_t>(1));
    }
    void SetIdleThresholdMillis(uint32_t idleThresholdMillis)
    {
        mIdleThresholdMillis = std::max(idleThresholdMillis, static_cast<uint32_t>(1));
    }

    void HandleEvent(const Event &event);

  private:
    void TransitionTo(State newState);
    void OnStateEnter(State newState);
    void OnStateExit(State oldState);

    GenericSwitchStateMachine::Driver *mDriver = nullptr;
    State mState = State::kIdleWaitFirstPress;
    uint8_t mMultiPressCount = 0;
    uint8_t mCurrentPressedPosition = 0;
    uint8_t mCurrentLatchedPosition = 0xFFu;
    bool mReachedMaximumPresses = false;
    uint32_t mLongPressThresholdMillis = 800u;
    uint32_t mIdleThresholdMillis = 400u;
};

} // namespace app
} // namespace chip
