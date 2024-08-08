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

#include <stdint.h>

#include "GenericSwitchStateMachine.h"

#include <lib/support/logging/CHIPLogging.h>

namespace chip {
namespace app {

void GenericSwitchStateMachine::HandleEvent(const Event &event)
{
    VerifyOrDie(mDriver != nullptr);

    ChipLogError(NotSpecified, "Got Event %d", (int)event.type);

    // TODO: Support multiple pressed positions.
    if (event.type == Event::Type::kButtonPress)
    {
        mDriver->SetButtonPosition(event.buttonPosition);
    }
    else if (event.type == Event::Type::kButtonRelease)
    {
        mDriver->SetButtonPosition(0);
    }

    switch (mState)
    {
        case State::kIdleWaitFirstPress: {
            if (event.type == Event::Type::kButtonPress)
            {
                mCurrentPressedPosition = event.buttonPosition;
                TransitionTo(State::kWaitLongDetermination);
            }
            break;
        }
        case State::kWaitLongDetermination: {
            if (event.type == Event::Type::kButtonRelease)
            {
                TransitionTo(State::kMultiPressReleased);
            }
            else if (event.type == Event::Type::kLongPressTimerHit)
            {
                TransitionTo(State::kLongPressOngoing);
            }
            break;
        }
        case State::kLongPressOngoing: {
            if (event.type == Event::Type::kButtonRelease)
            {
                TransitionTo(State::kLongPressDone);
            }
            break;
        }
        case State::kLongPressDone: {
            // No event-based transitions.
            break;
        }
        case State::kMultiPressReleased: {
            if (event.type == Event::Type::kButtonPress)
            {
                TransitionTo(State::kMultiPressPressed);
            }
            else if (event.type == Event::Type::kIdleTimerHit)
            {
                TransitionTo(State::kMultiPressDone);
            }
            break;
        }
        case State::kMultiPressPressed: {
            if (event.type == Event::Type::kButtonRelease)
            {
                TransitionTo(State::kMultiPressReleased);
            }
            break;
        }
        case State::kMultiPressDone: {
            // No event-based transitions.
            break;
        }
        default:
            VerifyOrDie(false && "Unexpected state");
    }
}

void GenericSwitchStateMachine::TransitionTo(State newState)
{
    State oldState = mState;

    if (newState == oldState)
    {
        return;
    }

    OnStateExit(oldState);
    mState = newState;
    OnStateEnter(newState);

    ChipLogError(NotSpecified, "Generic Switch %d -> %d", (int)oldState, (int)newState);
}

void GenericSwitchStateMachine::OnStateEnter(State newState)
{
    VerifyOrDie(mDriver != nullptr);
    switch (mState)
    {
        case State::kIdleWaitFirstPress: {
            mCurrentPressedPosition = 0;
            break;
        }
        case State::kWaitLongDetermination: {
            mMultiPressCount = 1;
            mDriver->EmitInitialPress(mCurrentPressedPosition);
            mDriver->StartLongPressTimer(mLongPressThresholdMillis, this);
            break;
        }
        case State::kLongPressOngoing: {
            mDriver->EmitLongPress(mCurrentPressedPosition);
            break;
        }
        case State::kLongPressDone: {
            mDriver->EmitLongRelease(mCurrentPressedPosition);
            TransitionTo(State::kIdleWaitFirstPress);
            break;
        }
        case State::kMultiPressReleased: {
            mDriver->CancelLongPressTimer();
            mDriver->StartWaitIdleTimer(mIdleThresholdMillis, this);
            break;
        }
        case State::kMultiPressPressed: {
            mDriver->CancelWaitIdleTimer();

            // Ignore counting once we reach the max, just wait for idle.
            if (mMultiPressCount != mMaxMultiPress)
            {
                ++mMultiPressCount;
            }
            break;
        }
        case State::kMultiPressDone: {
            mDriver->EmitMultiPressComplete(mCurrentPressedPosition, mMultiPressCount);
            TransitionTo(State::kIdleWaitFirstPress);
            break;
        }
        default:
            break;
    }
}

void GenericSwitchStateMachine::OnStateExit(State oldState)
{
    // Nothing happens :)
    (void)oldState;
}


} // namespace app
} // namespace chip
