// Copyright 2024 Google LLC. All rights reserved.

#pragma once

#include <system/SystemLayer.h>
#include <system/SystemClock.h>

namespace google {
namespace matter {

// Supports ActionSwitch MSM and MSL behavior.

class GenericSwitchStateMachine
{
  public:
    enum class Event {
      kButtonPress = 0,
      kButtonRelease = 1,
      kLongPressTimerComplete = 2,
      kIdleTimerComplete = 3,
    };

    enum class State {
      kIdleWaitFirstPress = 0,

      kWaitLongDetermination = 1,
      kWaitLongRelease = 2,

      kWaitRelease = 3,

    };

    class GenericSwitchStateMachineDriver
    {
      public:
        virtual void StartTimer(chip::System::Clock::Timeout timeoutMs, void (*)(void *), void *ctx) = 0;
        virtual void CancelTimer(void *ctx) = 0;

        virtual void EmitInitPress(uint8_t newPosition) = 0;
        virtual void EmitLongPress(uint8_t newPosition) = 0;
        virtual void EmitLongRelease(uint8_t prevPosition) = 0;
        virtual void EmitMultiPressComplete(uint8_t prevPosition, uint8_t numPresses) = 0;
    };

  private:

}

} // namespace matter
} // namespace google
