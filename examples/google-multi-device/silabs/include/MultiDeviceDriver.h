// Copyrigh 2024 Google, All Rights Reserved.

#pragma once

#include <stdint.h>

namespace google {
namespace matter {

enum class HardwareEvent
{
  kOccupancyDetected = 0,
  kOccupancyUndetected = 1,
  kSwitchButtonPressed = 2,
  kSwitchButtonReleased = 3,
};

typedef void (*HardwareEventCallback)(HardwareEvent event);

class GmdSilabsDriver
{
  public:
    GmdSilabsDriver() {}

    void Init();

    void SetLightLedEnabled(bool enabled);
    void SetDebugPin(bool high);
    void EmitDebugCode(uint8_t code);

    bool IsSwitchButtonPressed() const;
    bool IsProximityDetected() const;

    void HandleDebounceTimer();

    void SetHardwareEventCallback(HardwareEventCallback callback)
    {
        mHardwareCallback = callback;
    }

    void CallHardwareEventCallback(HardwareEvent event)
    {
        if (mHardwareCallback != nullptr)
        {
            mHardwareCallback(event);
        }
    }

    static GmdSilabsDriver & GetInstance() {
      static GmdSilabsDriver sInstance;
      return sInstance;
    }
  private:
    HardwareEventCallback mHardwareCallback = nullptr;
    bool mIsButtonPressed = false;
    bool mIsProxDetected = false;
    bool mGotInitialDebounceState = false;
};

} // namespace matter
} // namespace google
