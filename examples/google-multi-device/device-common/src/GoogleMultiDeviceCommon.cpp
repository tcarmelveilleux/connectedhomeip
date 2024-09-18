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

#include <app-common/zap-generated/cluster-enums.h>
#include <include/platform/DeviceInstanceInfoProvider.h>
#include <lib/support/CodeUtils.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/BitFlags.h>

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
    if (mOccupancyInstanceEp5 != nullptr)
    {
        mOccupancyInstanceEp5.reset();
    }
    if (mOccupancyDelegateEp5 != nullptr)
    {
        mOccupancyDelegateEp5.reset();
    }
}

void GoogleMultiDeviceIntegration::InitializeProduct()
{
    chip::DeviceLayer::SetDeviceInstanceInfoProvider(&sDeviceInfoProvider);
    chip::Credentials::SetDeviceAttestationCredentialsProvider(&sAttestationProvider);

    // EP2: Generic switch setup (Action Switch features)
    {
        mGenericSwitchDriverEp2.SetEndpointId(2);
        BitFlags<Clusters::Switch::Feature> ep2Features;
        ep2Features
            .Set(Clusters::Switch::Feature::kActionSwitch)
            .Set(Clusters::Switch::Feature::kMomentarySwitch)
            .Set(Clusters::Switch::Feature::kMomentarySwitchLongPress)
            .Set(Clusters::Switch::Feature::kMomentarySwitchMultiPress);
        mGenericSwitchDriverEp2.SetSupportedFeatures(ep2Features);
        mGenericSwitchDriverEp2.SetMultiPressMax(5);
        mGenericSwitchDriverEp2.SetNumPositions(2);

        mGenericSwitchStateMachineEp2.SetDriver(&mGenericSwitchDriverEp2);
        mGenericSwitchDriverEp2.SetButtonPosition(0);
    }

    // EP3: Generic switch setup (Legacy Switch features)
    {
        mGenericSwitchDriverEp3.SetEndpointId(3);
        BitFlags<Clusters::Switch::Feature> ep3Features;
        ep3Features
            .Set(Clusters::Switch::Feature::kMomentarySwitch)
            .Set(Clusters::Switch::Feature::kMomentarySwitchLongPress)
            .Set(Clusters::Switch::Feature::kMomentarySwitchRelease)
            .Set(Clusters::Switch::Feature::kMomentarySwitchMultiPress);
        mGenericSwitchDriverEp3.SetSupportedFeatures(ep3Features);
        mGenericSwitchDriverEp3.SetMultiPressMax(5);
        mGenericSwitchDriverEp3.SetNumPositions(2);

        mGenericSwitchStateMachineEp3.SetDriver(&mGenericSwitchDriverEp3);
        mGenericSwitchDriverEp3.SetButtonPosition(0);
    }

    // EP4: Generic switch setup (Latching Switch features)
    {
        mGenericSwitchDriverEp4.SetEndpointId(4);

        BitFlags<Clusters::Switch::Feature> ep4Features;
        ep4Features.Set(Clusters::Switch::Feature::kLatchingSwitch);
        mGenericSwitchDriverEp4.SetSupportedFeatures(ep4Features);
        mGenericSwitchDriverEp4.SetNumPositions(3);
        mGenericSwitchDriverEp4.SetMultiPressMax(0);

        mGenericSwitchStateMachineEp4.SetDriver(&mGenericSwitchDriverEp4);

        // Initialize initial state.
        mGenericSwitchDriverEp4.SetButtonPosition(GetEp4LatchInitialPosition());
    }

    // EP5: Occupancy sensor setup
    const EndpointId kOccupancyEndpointId = 5;
    mOccupancyDelegateEp5 = std::make_unique<OccupancyDelegate>(kOccupancyEndpointId);
    mOccupancyInstanceEp5 = std::make_unique<OccupancySensing::Instance>(mOccupancyDelegateEp5.get(), kOccupancyEndpointId);
    VerifyOrDie(mOccupancyInstanceEp5->Init() == CHIP_NO_ERROR);

    // EP6: Dishwasher setup
    const EndpointId kDishwasherEndpointId = 6;
    mFakeDishwasherEp6 = MakeGoogleFakeDishwasher(kDishwasherEndpointId);
    mOpStateInstanceEp6 = std::make_unique<OperationalState::Instance>(mFakeDishwasherEp6->GetDelegate(), kDishwasherEndpointId);

    mOpStateInstanceEp6->SetOperationalState(to_underlying(OperationalState::OperationalStateEnum::kStopped));
    mOpStateInstanceEp6->Init();
}

void GoogleMultiDeviceIntegration::HandleButtonPress(ButtonId buttonId)
{
    uint8_t latchPos = 0;
    bool isLatch = false;

    switch (buttonId)
    {
    case ButtonId::kRed:
        // Position 1 on EP2
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mGenericSwitchStateMachineEp2.HandleEvent(GenericSwitchStateMachine::Event::MakeButtonPressEvent(1));
        });
        break;
    case ButtonId::kYellow:
        // Position 1 on EP3
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mGenericSwitchStateMachineEp3.HandleEvent(GenericSwitchStateMachine::Event::MakeButtonPressEvent(1));
        });
        break;
    case ButtonId::kGreen:
        // TODO: Handle for opstate
        break;
    case ButtonId::kLatch1:
        latchPos = 0;
        isLatch = true;
        break;
    case ButtonId::kLatch2:
        latchPos = 1;
        isLatch = true;
        break;
    case ButtonId::kLatch3:
        latchPos = 2;
        isLatch = true;
        break;
    default:
        break;
    }

    if (isLatch)
    {
        // Latch positions on EP4
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this, latchPos](){
            this->mGenericSwitchStateMachineEp4.HandleEvent(GenericSwitchStateMachine::Event::MakeLatchSwitchChangeEvent(latchPos));
        });
    }
}

void GoogleMultiDeviceIntegration::HandleButtonRelease(ButtonId buttonId)
{
    switch (buttonId)
    {
    case ButtonId::kRed:
        // Position 1 on EP2
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mGenericSwitchStateMachineEp2.HandleEvent(GenericSwitchStateMachine::Event::MakeButtonReleaseEvent(1));
        });
        break;
    case ButtonId::kYellow:
        // Position 1 on EP3
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mGenericSwitchStateMachineEp3.HandleEvent(GenericSwitchStateMachine::Event::MakeButtonReleaseEvent(1));
        });
        break;
    case ButtonId::kGreen:
        // TODO: Handle for opstate
        break;
    default:
        break;
    }
}

void GoogleMultiDeviceIntegration::HandleOccupancyDetected(uint8_t sensorId)
{
    if (sensorId == 0)
    {
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mOccupancyInstanceEp5->SetOccupancyDetectedFromSensor(true);
        });
    }
}

void GoogleMultiDeviceIntegration::HandleOccupancyUndetected(uint8_t sensorId)
{
    if (sensorId == 0)
    {
        chip::DeviceLayer::SystemLayer().ScheduleLambda([this](){
            this->mOccupancyInstanceEp5->SetOccupancyDetectedFromSensor(false);
        });
    }
}

} // namespace matter
} // namespace google
