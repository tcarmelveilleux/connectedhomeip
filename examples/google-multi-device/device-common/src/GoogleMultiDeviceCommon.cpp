// Copyright 2024 Google. All rights reserved.

#include <memory>
#include <utility>

#include <stdint.h>

#include "GoogleMultiDeviceCommon.h"

#include "GoogleMultiDeviceInfoProvider.h"
#include "GoogleMultiDeviceAttestationProvider.h"
#include "GenericSwitchStateMachine.h"
#include "DefaultGenericSwitchStateMachineDriver.h"
#include "GoogleMultiDeviceDishwasherOpstate.h"

#include <include/platform/DeviceInstanceInfoProvider.h>
#include <lib/support/CodeUtils.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

#include <system/SystemClock.h>
#include <system/SystemLayer.h>

#include <platform/CHIPDeviceLayer.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

namespace google {
namespace matter {

namespace {

GoogleMultiDeviceInfoProvider sDeviceInfoProvider;
GoogleMultiDeviceAttestationProvider sAttestationProvider;

} // namespace

class OccupancyDelegate : public OccupancySensing::Instance::Delegate
{
  public:
    //OccupancyDelegate() = delete;
    OccupancyDelegate(EndpointId endpointId) : mEndpoint(endpointId) {}

    // Not copyable.
    OccupancyDelegate(const OccupancyDelegate &)             = delete;
    OccupancyDelegate & operator=(const OccupancyDelegate &) = delete;

    /**
     * Called whenever HoldTime is updated via Matter.
     *
     * NOTE: The hold time timer (if needed) is managed by the cluster Instance
     *       so the value provided here is merely for informational purpose for
     *       the product to know about it.
     */
    void OnHoldTimeWrite(uint16_t holdTimeSeconds) override {}

    /**
     * Called whenever Occupancy attribute changes (i.e. including hold time handling).
     */
    void OnOccupancyChange(chip::BitMask<OccupancySensing::OccupancyBitmap> occupancy) override
    {
        GoogleMultiDeviceIntegration::GetInstance().SetDebugLed(occupancy.HasAny());
    }

    /**
     * Query the supported features for the given delegate.
     */
    BitFlags<OccupancySensing::Feature> GetSupportedFeatures() override
    {
        return BitFlags<OccupancySensing::Feature>{}
            .Set(OccupancySensing::Feature::kPassiveInfrared)
            .Set(OccupancySensing::Feature::kRadar)
            .Set(OccupancySensing::Feature::kRFSensing);
    }

    /**
     * Getter to obtain the current hold time limits for the product.
     *
     * @return the actual hold time limits values.
     */
    OccupancySensing::Structs::HoldTimeLimitsStruct::Type GetHoldTimeLimits() const override
    {
        OccupancySensing::Structs::HoldTimeLimitsStruct::Type holdTimeLimits{};
        holdTimeLimits.holdTimeMin     = 1;
        holdTimeLimits.holdTimeMax     = 20;
        holdTimeLimits.holdTimeDefault = 5;

        return holdTimeLimits;
    }

    /**
     * @brief Start a hold timer for `numSeconds` seconds, that will must callback in Matter context when expired.
     *
     * @returns true on success, false on failure.
     */
    bool StartHoldTimer(uint16_t numSeconds, std::function<void()> callback) override
    {
        mHoldTimerCallback = callback;
        return CHIP_NO_ERROR == chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Timeout(numSeconds * 1000u), &HoldTimerHandler, this);
    }
  private:
    static void HoldTimerHandler(System::Layer * layer, void * ctx)
    {
        if (ctx != nullptr)
        {
            auto that = reinterpret_cast<OccupancyDelegate *>(ctx);
            that->mHoldTimerCallback();
        }
    }

    EndpointId mEndpoint;
};

GoogleMultiDeviceIntegration::~GoogleMultiDeviceIntegration()
{
    if (mOccupancyInstanceEp3 != nullptr)
    {
        mOccupancyInstanceEp3.reset();
    }
    if (mOccupancyDelegateEp3 != nullptr)
    {
        mOccupancyDelegateEp3.reset();
    }
}

void GoogleMultiDeviceIntegration::InitializeProduct()
{
    chip::DeviceLayer::SetDeviceInstanceInfoProvider(&sDeviceInfoProvider);
    chip::Credentials::SetDeviceAttestationCredentialsProvider(&sAttestationProvider);

    // EP2: Generic switch setup
    mGenericSwitchDriverEp2.SetEndpointId(2);
    mGenericSwitchStateMachineEp2.SetDriver(&mGenericSwitchDriverEp2);

    // EP3: Occupancy sensor setup
    const EndpointId kOccupancyEndpointId = 3;
    mOccupancyDelegateEp3 = std::make_unique<OccupancyDelegate>(kOccupancyEndpointId);
    mOccupancyInstanceEp3 = std::make_unique<OccupancySensing::Instance>(mOccupancyDelegateEp3.get(), kOccupancyEndpointId);
    VerifyOrDie(mOccupancyInstanceEp3->Init() == CHIP_NO_ERROR);

    // EP4: Dishwasher setup
    const EndpointId kDishwasherEndpointId = 4;
    mFakeDishwasherEp4 = MakeGoogleFakeDishwasher(kDishwasherEndpointId);
    mOpStateInstanceEp4 = std::make_unique<OperationalState::Instance>(mFakeDishwasherEp4->GetDelegate(), kDishwasherEndpointId);

    mOpStateInstanceEp4->SetOperationalState(to_underlying(OperationalState::OperationalStateEnum::kStopped));
    mOpStateInstanceEp4->Init();
}

void GoogleMultiDeviceIntegration::HandleButtonPress(uint8_t buttonId)
{
    if (buttonId == 0)
    {
        // Position 1 on EP2
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mGenericSwitchStateMachineEp2.HandleEvent(GenericSwitchStateMachine::Event::MakeButtonPressEvent(1));
        });
    }
}

void GoogleMultiDeviceIntegration::HandleButtonRelease(uint8_t buttonId)
{
    if (buttonId == 0)
    {
        // Position 1 on EP2
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mGenericSwitchStateMachineEp2.HandleEvent(GenericSwitchStateMachine::Event::MakeButtonReleaseEvent(1));
        });
    }
}

void GoogleMultiDeviceIntegration::HandleOccupancyDetected(uint8_t sensorId)
{
    if (sensorId == 0)
    {
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mOccupancyInstanceEp3->SetOccupancyDetectedFromSensor(true);
        });
    }
}

void GoogleMultiDeviceIntegration::HandleOccupancyUndetected(uint8_t sensorId)
{
    if (sensorId == 0)
    {
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mOccupancyInstanceEp3->SetOccupancyDetectedFromSensor(false);
        });
    }
}

} // namespace matter
} // namespace google
