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

#include <app-common/zap-generated/cluster-enums.h>
#include <lib/support/logging/CHIPLogging.h>

namespace chip {
namespace app {

void GenericSwitchStateMachine::HandleEvent(const Event &event)
{
    VerifyOrDie(mDriver != nullptr);

#if 0
    ChipLogError(NotSpecified, "Got Event %d", (int)event.type);
#endif

    bool supports_latch = mDriver->GetSupportedFeatures().Has(Clusters::Switch::Feature::kLatchingSwitch);

    if (event.type == Event::Type::kButtonPress)
    {
        mDriver->SetButtonPosition(event.buttonPosition);
    }
    // TODO: Support multiple pressed positions (not always return to 0).
    else if (event.type == Event::Type::kButtonRelease)
    {
        mDriver->SetButtonPosition(0);
    }
    else if (event.type == Event::Type::kLatchSwitchChange && supports_latch)
    {
        if (event.buttonPosition != mCurrentPosition)
        {
            mDriver->SetButtonPosition(event.buttonPosition);
            mDriver->EmitSwitchLatched(event.buttonPosition);
            mCurrentPosition = event.buttonPosition;
        }

        // No state machine transitions for latchign switches.
        return;
    }

    switch (mState)
    {
        case State::kIdleWaitFirstPress: {
            if (event.type == Event::Type::kButtonPress)
            {
                mCurrentPosition = event.buttonPosition;
                TransitionTo(State::kWaitLongDetermination);
            }
            break;
        }
        case State::kWaitLongDetermination: {
            if (event.type == Event::Type::kButtonRelease)
            {
                TransitionTo(State::kMultiPressReleased);
            }
            // NOTE: kLongPressTimerHit won't ever happen if LongPress not supported.
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

    bool supports_msr = mDriver->GetSupportedFeatures().Has(Clusters::Switch::Feature::kMomentarySwitchRelease);
    bool supports_msl = mDriver->GetSupportedFeatures().Has(Clusters::Switch::Feature::kMomentarySwitchLongPress);
    bool supports_msm = mDriver->GetSupportedFeatures().Has(Clusters::Switch::Feature::kMomentarySwitchMultiPress);
    bool supports_as = mDriver->GetSupportedFeatures().Has(Clusters::Switch::Feature::kActionSwitch);

    switch (mState)
    {
        case State::kIdleWaitFirstPress: {
            mCurrentPosition = 0;
            mReachedMaximumPresses = false;
            break;
        }
        case State::kWaitLongDetermination: {
            mMultiPressCount = 1;
            mDriver->EmitInitialPress(mCurrentPosition);
            if (supports_msl)
            {
                mDriver->StartLongPressTimer(mLongPressThresholdMillis, this);
            }
            break;
        }
        case State::kLongPressOngoing: {
            mDriver->EmitLongPress(mCurrentPosition);
            break;
        }
        case State::kLongPressDone: {
            mDriver->EmitLongRelease(mCurrentPosition);
            TransitionTo(State::kIdleWaitFirstPress);
            break;
        }
        case State::kMultiPressReleased: {
            if (supports_msr && !supports_as)
            {
                mDriver->EmitShortRelease(mCurrentPosition);
            }

            if (supports_msm)
            {
                // For multipress, start waiting for idle time until a next press.
                mDriver->CancelLongPressTimer();
                mDriver->StartWaitIdleTimer(mIdleThresholdMillis, this);
            }
            else
            {
                // For non-multipress, immediately go to multipress done.
                TransitionTo(State::kMultiPressDone);
            }
            break;
        }
        case State::kMultiPressPressed: {
            mDriver->CancelWaitIdleTimer();

            if (supports_msm && !supports_as)
            {
                mDriver->EmitInitialPress(mCurrentPosition);
            }

            // Ignore counting once we reach the max, just wait for idle.
            if (mMultiPressCount != mDriver->GetMultiPressMax())
            {
                ++mMultiPressCount;

                if (supports_msm && !supports_as)
                {
                    mDriver->EmitMultiPressOngoing(mCurrentPosition, mMultiPressCount);
                }
            }
            else
            {
                mReachedMaximumPresses = true;
            }
            break;
        }
        case State::kMultiPressDone: {
            if (supports_msm)
            {
                uint8_t numPresses = mReachedMaximumPresses ? 0 : mMultiPressCount;
                mDriver->EmitMultiPressComplete(mCurrentPosition, numPresses);
            }

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
