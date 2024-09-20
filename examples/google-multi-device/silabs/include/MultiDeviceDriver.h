// Copyrigh 2024 Google, All Rights Reserved.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace google {
namespace matter {

enum class HardwareEvent
{
  kOccupancyDetected = 0,
  kOccupancyUndetected = 1,

  kRedButtonPressed = 2,
  kRedButtonReleased = 3,
  kYellowButtonPressed = 4,
  kYellowButtonReleased = 5,
  kGreenButtonPressed = 6,
  kGreenButtonReleased = 7,

  kLatchSwitch1Selected = 8,
  kLatchSwitch1Deselected = 9,
  kLatchSwitch2Selected = 10,
  kLatchSwitch2Deselected = 11,
  kLatchSwitch3Selected = 12,
  kLatchSwitch3Deselected = 13,

  // For some no-ops in tables
  kNoHardwareEvent = 14,
};

typedef void (*HardwareEventCallback)(HardwareEvent event);

class GmdSilabsDriver
{
  public:
    enum class ButtonId : uint8_t {
      kRed = 0,
      kYellow = 1,
      kGreen = 2,
      kLatch1 = 3,
      kLatch2 = 4,
      kLatch3 = 5,
      kNumButtons
    };

    enum class LedId : uint8_t {
      kRed = 0,
      kYellow = 1,
      kGreen = 2,
      kNumLeds
    };

    GmdSilabsDriver() {}

    void Init();

    void SetLightLedEnabled(LedId led_id, bool enabled);
    void SetDebugPin(bool high);
    void EmitDebugCode(uint8_t code);

    bool IsSwitchButtonPressed(ButtonId button_id) const;
    bool IsProximityDetected() const;
    bool IsAlternativeDiscriminator() const;

    void HandleDebounceTimer();

    void SetHardwareEventCallback(HardwareEventCallback callback)
    {
        mHardwareCallback = callback;
    }

    void CallHardwareEventCallback(HardwareEvent event)
    {
        if (mHardwareCallback != nullptr && (event != HardwareEvent::kNoHardwareEvent))
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
    bool mIsButtonPressed[static_cast<size_t>(ButtonId::kNumButtons)] = {0};
    bool mIsProxDetected = false;
    bool mGotInitialDebounceState = false;
};

} // namespace matter
} // namespace google
