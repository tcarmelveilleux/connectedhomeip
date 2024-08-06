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
    bool IsSwitchButtonPressed() const;
    bool IsProximityDetected() const;

    static void OnPinInterrupt(uint8_t intNum, void *ctx);

    void SetHardwareEventCallback(HardwareEventCallback callback)
    {
        mHardwareCallback = callback;
    }

    static void OnDebounceTimer(void *ctx);

    static GmdSilabsDriver & GetInstance() { return sInstance; }
  private:
    static GmdSilabsDriver sInstance;
    HardwareEventCallback mHardwareCallback = nullptr;
};

} // namespace matter
} // namespace google
