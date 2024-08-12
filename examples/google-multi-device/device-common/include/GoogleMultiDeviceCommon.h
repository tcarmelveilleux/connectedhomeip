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
    GoogleMultiDeviceIntegration() = default;
    ~GoogleMultiDeviceIntegration();

    // Not copyable.
    GoogleMultiDeviceIntegration(const GoogleMultiDeviceIntegration &)             = delete;
    GoogleMultiDeviceIntegration & operator=(const GoogleMultiDeviceIntegration &) = delete;

    void InitializeProduct();

    void HandleButtonPress(uint8_t buttonId);
    void HandleButtonRelease(uint8_t buttonId);
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

    std::unique_ptr<chip::app::Clusters::OccupancySensing::Instance> mOccupancyInstanceEp3 = nullptr;
    std::unique_ptr<chip::app::Clusters::OccupancySensing::Instance::Delegate> mOccupancyDelegateEp3 = nullptr;

    std::unique_ptr<GoogleFakeDishwasherInterface> mFakeDishwasherEp4 = nullptr;
    std::unique_ptr<chip::app::Clusters::OperationalState::Instance> mOpStateInstanceEp4 = nullptr;
};

} // namespace matter
} // namespace google
