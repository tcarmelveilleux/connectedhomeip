// Copyright 2024 Google. All rights reserved.

#include "GoogleMultiDeviceDishwasherOpstate.h"

#include <memory>
#include <utility>

#include "app-common/zap-generated/cluster-enums.h"
#include "app/clusters/operational-state-server/operational-state-server.h"
#include "app/clusters/operational-state-server/operational-state-cluster-objects.h"
#include <app/data-model/Nullable.h>

#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <system/SystemClock.h>
#include <system/SystemLayer.h>

#include <platform/CHIPDeviceLayer.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

using chip::app::Clusters::OperationalState::GenericOperationalState;
using chip::app::Clusters::OperationalState::GenericOperationalError;
using chip::app::Clusters::OperationalState::OperationalStateEnum;
using chip::app::Clusters::OperationalState::ErrorStateEnum;

namespace google {
namespace matter {

class GoogleDishwasherOperationalStateDelegate : public OperationalState::Delegate
{
public:
    GoogleDishwasherOperationalStateDelegate()
    {
        mOperationalStateList = Span<const GenericOperationalState>(kOpStateList);
    }

    /**
     * Get the countdown time. This attribute is not used in this application.
     * @return The current countdown time.
     */
    app::DataModel::Nullable<uint32_t> GetCountdownTime() override
    {
        if (mCurrentPhaseEstimate.IsNull())
            return DataModel::NullNullable;

        return DataModel::MakeNullable((uint32_t) (mCurrentPhaseEstimate.Value() - mRunningTime));
    }

    /**
     * Fills in the provided GenericOperationalState with the state at index `index` if there is one,
     * or returns CHIP_ERROR_NOT_FOUND if the index is out of range for the list of states.
     * Note: This is used by the SDK to populate the operational state list attribute. If the contents of this list changes,
     * the device SHALL call the Instance's ReportOperationalStateListChange method to report that this attribute has changed.
     * @param index The index of the state, with 0 representing the first state.
     * @param operationalState  The GenericOperationalState is filled.
     */
    CHIP_ERROR GetOperationalStateAtIndex(size_t index, GenericOperationalState & operationalState) override
    {
        if (index >= mOperationalStateList.size())
        {
            return CHIP_ERROR_NOT_FOUND;
        }
        operationalState = mOperationalStateList[index];
        return CHIP_NO_ERROR;
    }

    /**
     * Fills in the provided MutableCharSpan with the phase at index `index` if there is one,
     * or returns CHIP_ERROR_NOT_FOUND if the index is out of range for the list of phases.
     *
     * If CHIP_ERROR_NOT_FOUND is returned for index 0, that indicates that the PhaseList attribute is null
     * (there are no phases defined at all).
     *
     * Note: This is used by the SDK to populate the phase list attribute. If the contents of this list changes, the
     * device SHALL call the Instance's ReportPhaseListChange method to report that this attribute has changed.
     * @param index The index of the phase, with 0 representing the first phase.
     * @param operationalPhase  The MutableCharSpan is filled.
     */
    CHIP_ERROR GetOperationalPhaseAtIndex(size_t index, MutableCharSpan & operationalPhase) override
    {
        if (index >= mOperationalPhaseList.size())
        {
            return CHIP_ERROR_NOT_FOUND;
        }
        return CopyCharSpanToMutableCharSpan(mOperationalPhaseList[index], operationalPhase);
    }

    // command callback
    /**
     * Handle Command Callback in application: Pause
     * @param[out] get operational error after callback.
     */
    void HandlePauseStateCallback(GenericOperationalError & err) override
    {
        // placeholder implementation
        auto error = GetInstance()->SetOperationalState(to_underlying(OperationalState::OperationalStateEnum::kPaused));
        if (error == CHIP_NO_ERROR)
        {
            GetInstance()->UpdateCountdownTimeFromDelegate();
            err.Set(to_underlying(ErrorStateEnum::kNoError));
        }
        else
        {
            err.Set(to_underlying(ErrorStateEnum::kUnableToCompleteOperation));
        }
    }

    /**
     * Handle Command Callback in application: Resume
     * @param[out] get operational error after callback.
     */
    void HandleResumeStateCallback(GenericOperationalError & err) override
    {
        // placeholder implementation
        auto error = GetInstance()->SetOperationalState(to_underlying(OperationalStateEnum::kRunning));
        if (error == CHIP_NO_ERROR)
        {
            GetInstance()->UpdateCountdownTimeFromDelegate();
            err.Set(to_underlying(ErrorStateEnum::kNoError));
        }
        else
        {
            err.Set(to_underlying(ErrorStateEnum::kUnableToCompleteOperation));
        }
    }

    /**
     * Handle Command Callback in application: Start
     * @param[out] get operational error after callback.
     */
    void HandleStartStateCallback(GenericOperationalError & err) override
    {
        mCurrentPhaseEstimate.SetNonNull(static_cast<uint32_t>(kExamplePhaseTime));

        OperationalState::GenericOperationalError current_err(to_underlying(OperationalState::ErrorStateEnum::kNoError));
        GetInstance()->GetCurrentOperationalError(current_err);

        if (current_err.errorStateID != to_underlying(OperationalState::ErrorStateEnum::kNoError))
        {
            err.Set(to_underlying(OperationalState::ErrorStateEnum::kUnableToStartOrResume));
            return;
        }

        // placeholder implementation
        auto error = GetInstance()->SetOperationalState(to_underlying(OperationalStateEnum::kRunning));
        if (error == CHIP_NO_ERROR)
        {
            GetInstance()->UpdateCountdownTimeFromDelegate();
            (void) DeviceLayer::SystemLayer().StartTimer(System::Clock::Seconds16(1), OnOperationalStateTimerTick, this);
            err.Set(to_underlying(ErrorStateEnum::kNoError));
        }
        else
        {
            err.Set(to_underlying(ErrorStateEnum::kUnableToCompleteOperation));
        }
    }

    /**
     * Handle Command Callback in application: Stop
     * @param[out] get operational error after callback.
     */
    void HandleStopStateCallback(GenericOperationalError & err) override
    {
        // placeholder implementation
        auto error = GetInstance()->SetOperationalState(to_underlying(OperationalStateEnum::kStopped));
        if (error == CHIP_NO_ERROR)
        {
            (void) DeviceLayer::SystemLayer().CancelTimer(OnOperationalStateTimerTick, this);

            auto countdownTime = GetCountdownTime();
            if (countdownTime.IsNull() && countdownTime.Value() != 0)
            {
                // Move to Null value overall if we didn't reach end of cycle.
                mCurrentPhaseEstimate.SetNull();
            }

            GetInstance()->UpdateCountdownTimeFromDelegate();

            OperationalState::GenericOperationalError current_err(to_underlying(OperationalState::ErrorStateEnum::kNoError));
            GetInstance()->GetCurrentOperationalError(current_err);

            Optional<DataModel::Nullable<uint32_t>> totalTime((DataModel::Nullable<uint32_t>(mRunningTime + mPausedTime)));
            Optional<DataModel::Nullable<uint32_t>> pausedTime((DataModel::Nullable<uint32_t>(mPausedTime)));

            GetInstance()->OnOperationCompletionDetected(static_cast<uint8_t>(current_err.errorStateID), totalTime, pausedTime);

            mRunningTime = 0;
            mPausedTime  = 0;
            err.Set(to_underlying(ErrorStateEnum::kNoError));
        }
        else
        {
            err.Set(to_underlying(ErrorStateEnum::kUnableToCompleteOperation));
        }
    }

    static void OnOperationalStateTimerTick(System::Layer * systemLayer, void * data)
    {
        GoogleDishwasherOperationalStateDelegate * delegate = reinterpret_cast<GoogleDishwasherOperationalStateDelegate *>(data);

        OperationalState::Instance * instance = delegate->GetClusterInstance();
        OperationalState::OperationalStateEnum state =
            static_cast<OperationalState::OperationalStateEnum>(instance->GetCurrentOperationalState());

        auto countdown_time = delegate->GetCountdownTime();

        if (countdown_time.IsNull() || (!countdown_time.IsNull() && countdown_time.Value() > 0))
        {
            if (state == OperationalState::OperationalStateEnum::kRunning)
            {
                delegate->mRunningTime++;
            }
            else if (state == OperationalState::OperationalStateEnum::kPaused)
            {
                delegate->mPausedTime++;
            }
        }
        else if (!countdown_time.IsNull() && countdown_time.Value() <= 0)
        {
            OperationalState::GenericOperationalError noError(to_underlying(OperationalState::ErrorStateEnum::kNoError));
            delegate->HandleStopStateCallback(noError);
        }

        if (state == OperationalState::OperationalStateEnum::kRunning || state == OperationalState::OperationalStateEnum::kPaused)
        {
            (void) DeviceLayer::SystemLayer().StartTimer(System::Clock::Seconds16(1), OnOperationalStateTimerTick, delegate);
        }
        else
        {
            (void) DeviceLayer::SystemLayer().CancelTimer(OnOperationalStateTimerTick, delegate);
        }
    }

protected:
    OperationalState::Instance * GetClusterInstance()
    {
        return GetInstance();
    }
    static constexpr uint32_t kExamplePhaseTime = 30u;

    uint32_t mRunningTime = 0;
    uint32_t mPausedTime  = 0;
    app::DataModel::Nullable<uint32_t> mCurrentPhaseEstimate;
    Span<const GenericOperationalState> mOperationalStateList;
    Span<const CharSpan> mOperationalPhaseList;

    const GenericOperationalState kOpStateList[4] = {
        GenericOperationalState(to_underlying(OperationalStateEnum::kStopped)),
        GenericOperationalState(to_underlying(OperationalStateEnum::kRunning)),
        GenericOperationalState(to_underlying(OperationalStateEnum::kPaused)),
        GenericOperationalState(to_underlying(OperationalStateEnum::kError)),
    };
};



#if 0
void emberAfOperationalStateClusterInitCallback(chip::EndpointId endpointId)
{
    VerifyOrDie(endpointId == 1); // this cluster is only enabled for endpoint 1.
    VerifyOrDie(gOperationalStateInstance == nullptr && gOperationalStateDelegate == nullptr);

    gOperationalStateDelegate           = new OperationalStateDelegate;
    EndpointId operationalStateEndpoint = 0x01;
    gOperationalStateInstance           = new OperationalState::Instance(gOperationalStateDelegate, operationalStateEndpoint);

    gOperationalStateInstance->SetOperationalState(to_underlying(OperationalState::OperationalStateEnum::kStopped));

    gOperationalStateInstance->Init();
}
#endif // 0

GoogleFakeDishwasherInterface::GoogleFakeDishwasherInterface(EndpointId endpointId) : mEndpointId(endpointId)
{
    mOpStateDelegate = std::make_unique<GoogleDishwasherOperationalStateDelegate>();
}

std::unique_ptr<GoogleFakeDishwasherInterface> MakeGoogleFakeDishwasher(EndpointId endpointId)
{
    return std::make_unique<GoogleFakeDishwasherInterface>(endpointId);
}


} // namespace matter
} // namespace google
