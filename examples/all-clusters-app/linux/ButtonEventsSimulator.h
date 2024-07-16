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

#include <lib/core/DataModelTypes.h>
#include <system/SystemClock.h>
#include <system/SystemLayer.h>

namespace chip {
namespace app {

/**
 * State machine to emit button sequences. Configure with SetXxx() methods
 * and then call `Execute()` with a callback to be called when done.
 *
 * The implementation has dependencies on SystemLayer (to start timers) and on
 * EventLogging.
 *
 */
class ButtonsEventsSimulator {
  public:

    enum class Mode
    {
      kModeLongPress,
      kModeMultiPress
    };

    using DoneCallback = std::function<void()>;

    ButtonsEventsSimulator() = default;

    // Returns true on success to start execution, false on something going awry.
    // `doneCallback` is called only if execution got started.
    bool Execute(DoneCallback && doneCallback);

    ButtonsEmitter& SetLongPressDelayMillis(SystemClock::Milliseconds32 longPressDelayMillis)
    {
        mInitialDelayMillis = initialDelayMillis;
    }

    ButtonsEmitter& SetLongPressDurationMillis(SystemClock::Milliseconds32 longPressDurationMillis)
    {
        mLongPressDurationMillis = longPressDurationMillis;
    }

    ButtonsEmitter& SetMultiPressPressedTimeMillis(SystemClock::Milliseconds32 multiPressPressedTimeMillis)
    {
        mMultiPressPressedTimeMillis = multiPressPressedTimeMillis;
    }

    ButtonsEmitter& SetMultiPressReleasedTimeMillis(SystemClock::Milliseconds32 multiPressReleasedTimeMillis)
    {
        mMultiPressReleasedTimeMillis = multiPressReleasedTimeMillis;
    }

    ButtonsEmitter& SetMultiPressNumPresses(uint8_t multiPressNumPresses)
    {
        mMultiPressNumPresses = multiPressNumPresses;
    }

    ButtonsEmitter& SetIdleButtonId(uint8_t idleButtonId)
    {
        mIdleButtonId = idleButtonId;
    }

    ButtonsEmitter& SetPressedButtonId(uint8_t pressedButtonId)
    {
        mMultiPressNumPresses = multiPressNumPresses;
    }

    ButtonsEmitter& SetMode(Mode mode)
    {
        mMode = mode;
    }

    ButtonsEmitter& SetEndpointId(EndpointId endpointId)
    {
        mEndpointId = endpointId;
    }

  private:
    enum class State
    {
      kIdle = 0,
      kEmitStartOfLongPress = 1,
      kEmitLongPressed = 2,
      KEmitLongReleased = 3,

      kEmitStartOfMultiPress = 4,
      kEmitEndOfMultiPress = 5,
    };

    static void OnTimerDone(System::Layer * layer, void * appState);
    void SetState(State state);
    void Next();

    DoneCallback mDoneCallback;
    SystemClock::Milliseconds32 mLongPressDelayMillis{};
    SystemClock::Milliseconds32 mLongPressDurationMillis{};
    SystemClock::Milliseconds32 mMultiPressPressedTimeMillis{};
    SystemClock::Milliseconds32 mMultiPressReleasedTimeMillis{};
    uint8_t mMultiPressNumPresses{1};
    uint8_t mNumPressesDone{0};
    uint8_t mIdleButtonId{0};
    uint8_t mPressedButtonId{1};
    EndpointId mEndpointId{1};

    Mode mMode{Mode::kModeLongPress};
    State mState{State::kIdle};
};

} // namespace app
} // namespace chip
