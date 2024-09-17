// Copyright 2024 Google. All rights reserved.

#pragma once

#include <memory>

#include <stdint.h>

#include "GenericSwitchStateMachine.h"
#include "DefaultGenericSwitchStateMachineDriver.h"
#include "GoogleMultiDeviceDishwasherOpstate.h"
#include "app/clusters/occupancy-sensor-server/occupancy-sensor-server.h"
#include "app/clusters/operational-state-server/operational-state-server.h"

namespace google {
namespace matter {

class GoogleMultiDeviceIntegration
{
  public:
    enum class ButtonId : uint8_t {
        kRed = 0,
        kYellow = 1,
        kGreen = 2,
        kLatch1 = 3,
        kLatch2 = 4,
        kLatch3 = 5
    };

    GoogleMultiDeviceIntegration() = default;
    ~GoogleMultiDeviceIntegration();

    // Not copyable.
    GoogleMultiDeviceIntegration(const GoogleMultiDeviceIntegration &)             = delete;
    GoogleMultiDeviceIntegration & operator=(const GoogleMultiDeviceIntegration &) = delete;

    void InitializeProduct();

    void HandleButtonPress(ButtonId buttonId);
    void HandleButtonRelease(ButtonId buttonId);
    void HandleOccupancyDetected(uint8_t sensorId);
    void HandleOccupancyUndetected(uint8_t sensorId);

    // IMPLEMENT IN THE ACTUAL PRODUCT MODULE
    void SetDebugLed(bool enabled);
    void EmitDebugCode(uint8_t code);

    static GoogleMultiDeviceIntegration & GetInstance()
    {
        static GoogleMultiDeviceIntegration instance;
        return instance;
    }
  private:
    chip::app::DefaultGenericSwitchStateMachineDriver mGenericSwitchDriverEp2;
    chip::app::GenericSwitchStateMachine mGenericSwitchStateMachineEp2;

    std::unique_ptr<chip::app::Clusters::OccupancySensing::Instance> mOccupancyInstanceEp5 = nullptr;
    std::unique_ptr<chip::app::Clusters::OccupancySensing::Instance::Delegate> mOccupancyDelegateEp5 = nullptr;

    std::unique_ptr<GoogleFakeDishwasherInterface> mFakeDishwasherEp6 = nullptr;
    std::unique_ptr<chip::app::Clusters::OperationalState::Instance> mOpStateInstanceEp6 = nullptr;
};

} // namespace matter
} // namespace google
