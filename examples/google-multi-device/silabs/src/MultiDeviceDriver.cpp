// Copyrigh 2024 Google, All Rights Reserved.

#include <algorithm>

#include <stdint.h>
#include <cmsis_os2.h>

#include "MultiDeviceDriver.h"

#include "gpiointerrupt.h"

#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"

#include "FreeRTOS.h"
#include "timers.h"

#include <lib/support/CodeUtils.h>

// ========= Pin definitions (local) =======
#define LED_OUT_PORT                             gpioPortA
#define LED_OUT_PIN                              0

#define POT_IN_PORT                              gpioPortB
#define POT_IN_PIN                               0

#define BUTTON_IN_PORT                           gpioPortB
#define BUTTON_IN_PIN                            1

#define PROX_IN_PORT                             gpioPortD
#define PROX_IN_PIN                              4

#define DEBUG_OUT_PORT                           gpioPortC
#define DEBUG_OUT_PIN                            8

// ============ Start of driver code =============

namespace google {
namespace matter {

namespace {

TimerHandle_t sDebounceTimer = nullptr;

constexpr unsigned int kDebounceTimeMillis = 10u;

void ManchesterOut(GmdSilabsDriver & driver, const uint8_t *data, size_t size)
{
    // ADAPT kSpeedFactor to whatever speed sets the right rate on the target...
    const int kSpeedFactor = 5;
    while (size > 0) {
        uint16_t encoded = 0;
        for (int i = 0; i < 16; i++) {
            if ((i & 0x1) == 0) {
                if (*data & (1 << (i >> 1))) {
                    encoded |= 1 << i;
                } else {
                    encoded |= 1 << (i + 1);
                }
            }

            bool bitVal = (encoded & (1 << i)) != 0;
            for (int speedIdx = 0; speedIdx < kSpeedFactor; ++speedIdx)
            {
                driver.SetDebugPin(bitVal);
            }
        }

        // Bring back to idle
        for (int speedIdx = 0; speedIdx < (4 * kSpeedFactor); ++speedIdx)
        {
            driver.SetDebugPin(false);
        }

        data++;
        size--;
    }
}

uint32_t MillisToTicks(uint32_t millis)
{
    return std::max(pdMS_TO_TICKS(millis), static_cast<uint32_t>(1u));
}

void OnDebounceTimer( TimerHandle_t pxTimer )
{
    auto & driver = GmdSilabsDriver::GetInstance();
    driver.HandleDebounceTimer();
}


} // namespace

// Singleton instance.

void GmdSilabsDriver::Init()
{
  /* Enable GPIO in CMU */
  CMU_ClockEnable(cmuClock_GPIO, true);

  /* Initialize GPIO interrupt dispatcher */
  GPIOINT_Init();

  GPIO_PinModeSet(BUTTON_IN_PORT, BUTTON_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(PROX_IN_PORT, PROX_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(LED_OUT_PORT, LED_OUT_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(DEBUG_OUT_PORT, DEBUG_OUT_PIN, gpioModePushPull, 0);

  sDebounceTimer = xTimerCreate("debounce", MillisToTicks(kDebounceTimeMillis), pdTRUE, nullptr, OnDebounceTimer);
  if (sDebounceTimer == nullptr)
  {
      SetLightLedEnabled(true);
      EmitDebugCode('N');
  }
  xTimerStart(sDebounceTimer, 100);

#if 0
  /* Register callbacks before setting up and enabling pin interrupt. */
  sIntForButton = GPIOINT_CallbackRegisterExt(BUTTON_IN_PIN, GmdSilabsDriver::OnPinInterrupt, (void*)&GmdSilabsDriver::GetInstance());
  sIntForProx = GPIOINT_CallbackRegisterExt(PROX_IN_PIN, GmdSilabsDriver::OnPinInterrupt, (void*)&GmdSilabsDriver::GetInstance());

  /* Set fall and rising edge interrupts*/
  GPIO_ExtIntConfig(BUTTON_IN_PORT, BUTTON_IN_PIN, sIntForButton, true, true, true);
  GPIO_ExtIntConfig(PROX_IN_PORT, PROX_IN_PIN, sIntForProx, true, true, true);

  GPIO_IntClear((1 << sIntForButton) | (1 << sIntForProx));
  GPIO_IntEnable((1 << sIntForButton) | (1 << sIntForProx));
#endif // 0
}

void GmdSilabsDriver::HandleDebounceTimer()
{
    bool newIsButtonPressed = IsSwitchButtonPressed();
    bool newIsProxDetected = IsProximityDetected();

    EmitDebugCode(newIsButtonPressed ? 20 : 21);
    EmitDebugCode(mGotInitialDebounceState ? 30 : 31);

    if (!mGotInitialDebounceState)
    {
        mIsButtonPressed = newIsButtonPressed;
        mIsProxDetected = newIsProxDetected;
        mGotInitialDebounceState = true;
        EmitDebugCode(100);
        return;
    }

    EmitDebugCode(99);

    if (newIsButtonPressed != mIsButtonPressed)
    {
        mIsButtonPressed = newIsButtonPressed;
        EmitDebugCode(newIsButtonPressed ? 8 : 9);
        CallHardwareEventCallback(newIsButtonPressed ? HardwareEvent::kSwitchButtonPressed : HardwareEvent::kSwitchButtonReleased);
    }

    if (newIsProxDetected != mIsProxDetected)
    {
        mIsProxDetected = newIsProxDetected;
        EmitDebugCode(newIsProxDetected ? 10 : 11);
        CallHardwareEventCallback(newIsProxDetected ? HardwareEvent::kOccupancyDetected : HardwareEvent::kOccupancyUndetected);
    }
}

bool GmdSilabsDriver::IsSwitchButtonPressed() const
{
    return GPIO_PinInGet(BUTTON_IN_PORT, BUTTON_IN_PIN) == 0;
}

bool GmdSilabsDriver::IsProximityDetected() const
{
    return GPIO_PinInGet(PROX_IN_PORT, PROX_IN_PIN) == 0;
}

void GmdSilabsDriver::SetLightLedEnabled(bool enabled)
{
    if (enabled)
    {
        GPIO_PinOutClear(LED_OUT_PORT, LED_OUT_PIN);
    }
    else
    {
        GPIO_PinOutSet(LED_OUT_PORT, LED_OUT_PIN);
    }
}

void GmdSilabsDriver::SetDebugPin(bool high)
{
    if (high)
    {
        GPIO_PinOutSet(DEBUG_OUT_PORT, DEBUG_OUT_PIN);
    }
    else
    {
        GPIO_PinOutClear(DEBUG_OUT_PORT, DEBUG_OUT_PIN);
    }
}

void GmdSilabsDriver::EmitDebugCode(uint8_t code)
{
    ManchesterOut(*this, &code, 1);
}

} // namespace matter
} // namespace google
