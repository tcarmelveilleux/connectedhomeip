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

#include "ButtonEventsSimulator.h"

#include <utility>
#include <functional>

#include <app/EventLogging.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <system/SystemLayer.h>
#include <system/SystemClock.h>

namespace chip {
namespace app {

void ButtonEventsSimulator::OnTimerDone(System::Layer * layer, void * appState)
{
    ButtonEventsSimulator *that = reinterpret_cast<ButtonEventsSimulator*>(appState);
    that->Next();
}

bool ButtonEventsSimulator::Execute(DoneCallback && doneCallback)
{
    VerifyOrReturnValue(mIdleButtonId != mPressedButtonId, false);

    switch(mMode)
    {
      case Mode::kModeLongPress:
        VerifyOrReturnValue(mLongPressDurationMillis > mLongPressDelayMillis, false);
        break;
      case Mode::kModeMultiPress:
        VerifyOrReturnValue(mMultiPressPressedTimeMillis > 0, false);
        VerifyOrReturnValue(mMultiPressReleasedTimeMillis > 0, false);
        VerifyOrReturnValue(mMultiPressNumPresses > 0, false);
        break;
      default:
        return false;
    }
    mDoneCallback = std::move(doneCallback);
    return false;
}

void ButtonEventsSimulator::SetState(State state)
{
    State oldState = mState;
    if (oldState != state)
    {
        ChipLogProgress(Zcl, "ButtonEventsSimulator state change %u -> %u", static_cast<unsigned>(oldState), static_cast<unsigned>(newState));
    }

    mState = state;
}

void ButtonEventsSimulator::Next()
{

}

} // namespace app
} // namespace chip
