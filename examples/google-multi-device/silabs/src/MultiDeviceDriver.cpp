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
#define RED_LED_OUT_PORT        gpioPortC
#define RED_LED_OUT_PIN         0

#define YELLOW_LED_OUT_PORT     gpioPortC
#define YELLOW_LED_OUT_PIN      3

#define GREEN_LED_OUT_PORT      gpioPortC
#define GREEN_LED_OUT_PIN       8

#define POT_IN_PORT             gpioPortB
#define POT_IN_PIN              0

#define RED_BUTTON_IN_PORT      gpioPortC
#define RED_BUTTON_IN_PIN       2

#define YELLOW_BUTTON_IN_PORT   gpioPortC
#define YELLOW_BUTTON_IN_PIN    1

#define GREEN_BUTTON_IN_PORT    gpioPortB
#define GREEN_BUTTON_IN_PIN     1

#define LATCH1_IN_PORT          gpioPortA
#define LATCH1_IN_PIN           0

#define LATCH2_IN_PORT          gpioPortB
#define LATCH2_IN_PIN           4

#define LATCH3_IN_PORT          gpioPortB
#define LATCH3_IN_PIN           5

#define PROX_IN_PORT            gpioPortD
#define PROX_IN_PIN             4

#define DEBUG_OUT_PORT          gpioPortD
#define DEBUG_OUT_PIN           5

// ============ Start of driver code =============

namespace google {
namespace matter {

namespace {

TimerHandle_t sDebounceTimer = nullptr;

constexpr unsigned int kDebounceTimeMillis = 10u;

struct PortAndPin {
    PortAndPin(unsigned the_port, unsigned the_pin): port(the_port), pin(the_pin) {}

    unsigned port;
    unsigned pin;
};

const PortAndPin gLedMappings[static_cast<size_t>(GmdSilabsDriver::LedId::kNumLeds)] =
{
   PortAndPin{RED_LED_OUT_PORT, RED_LED_OUT_PIN},
   PortAndPin{YELLOW_LED_OUT_PORT, YELLOW_LED_OUT_PIN},
   PortAndPin{GREEN_LED_OUT_PORT, GREEN_LED_OUT_PIN},
};

const PortAndPin gSwitchMappings[static_cast<size_t>(GmdSilabsDriver::ButtonId::kNumButtons)] =
{
   PortAndPin{RED_BUTTON_IN_PORT, RED_BUTTON_IN_PIN},
   PortAndPin{YELLOW_BUTTON_IN_PORT, YELLOW_BUTTON_IN_PIN},
   PortAndPin{GREEN_BUTTON_IN_PORT, GREEN_BUTTON_IN_PIN},
   PortAndPin{LATCH1_IN_PORT, LATCH1_IN_PIN},
   PortAndPin{LATCH2_IN_PORT, LATCH2_IN_PIN},
   PortAndPin{LATCH3_IN_PORT, LATCH3_IN_PIN},
};

const HardwareEvent gSwitchPressEvents[static_cast<size_t>(GmdSilabsDriver::ButtonId::kNumButtons)] =
{
    HardwareEvent::kRedButtonPressed,
    HardwareEvent::kYellowButtonPressed,
    HardwareEvent::kGreenButtonPressed,
    HardwareEvent::kLatchSwitch1Selected,
    HardwareEvent::kLatchSwitch2Selected,
    HardwareEvent::kLatchSwitch3Selected,
};

const HardwareEvent gSwitchReleaseEvents[static_cast<size_t>(GmdSilabsDriver::ButtonId::kNumButtons)] =
{
    HardwareEvent::kRedButtonReleased,
    HardwareEvent::kYellowButtonReleased,
    HardwareEvent::kGreenButtonReleased,
    HardwareEvent::kLatchSwitch1Deselected,
    HardwareEvent::kLatchSwitch2Deselected,
    HardwareEvent::kLatchSwitch3Deselected,
};

#if 0
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
#endif // 0

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

  GPIO_PinModeSet(RED_BUTTON_IN_PORT, RED_BUTTON_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(YELLOW_BUTTON_IN_PORT, YELLOW_BUTTON_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(GREEN_BUTTON_IN_PORT, GREEN_BUTTON_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(LATCH1_IN_PORT, LATCH1_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(LATCH2_IN_PORT, LATCH2_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(LATCH3_IN_PORT, LATCH3_IN_PIN, gpioModeInputPullFilter, 1);

  GPIO_PinModeSet(PROX_IN_PORT, PROX_IN_PIN, gpioModeInputPullFilter, 1);

  GPIO_PinModeSet(RED_LED_OUT_PORT, RED_LED_OUT_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(YELLOW_LED_OUT_PORT, YELLOW_LED_OUT_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(GREEN_LED_OUT_PORT, GREEN_LED_OUT_PIN, gpioModePushPull, 1);

  GPIO_PinModeSet(DEBUG_OUT_PORT, DEBUG_OUT_PIN, gpioModePushPull, 0);

  sDebounceTimer = xTimerCreate("debounce", MillisToTicks(kDebounceTimeMillis), pdTRUE, nullptr, OnDebounceTimer);
  if (sDebounceTimer == nullptr)
  {
      SetLightLedEnabled(LedId::kRed, true);
      EmitDebugCode('N');
  }
  xTimerStart(sDebounceTimer, 100);

#if 0
  /* Register callbacks before setting up and enabling pin interrupt. */
  sIntForButton = GPIOINT_CallbackRegisterExt(RED_BUTTON_IN_PIN, GmdSilabsDriver::OnPinInterrupt, (void*)&GmdSilabsDriver::GetInstance());
  sIntForProx = GPIOINT_CallbackRegisterExt(PROX_IN_PIN, GmdSilabsDriver::OnPinInterrupt, (void*)&GmdSilabsDriver::GetInstance());

  /* Set fall and rising edge interrupts*/
  GPIO_ExtIntConfig(RED_BUTTON_IN_PORT, RED_BUTTON_IN_PIN, sIntForButton, true, true, true);
  GPIO_ExtIntConfig(PROX_IN_PORT, PROX_IN_PIN, sIntForProx, true, true, true);

  GPIO_IntClear((1 << sIntForButton) | (1 << sIntForProx));
  GPIO_IntEnable((1 << sIntForButton) | (1 << sIntForProx));
#endif // 0
}

void GmdSilabsDriver::HandleDebounceTimer()
{
    bool newIsButtonPressed[static_cast<size_t>(ButtonId::kNumButtons)] = {0};
    
    for (size_t btn_idx = 0; btn_idx < static_cast<size_t>(ButtonId::kNumButtons); ++btn_idx)
    {
        newIsButtonPressed[btn_idx] = IsSwitchButtonPressed(static_cast<ButtonId>(btn_idx));
        if (!mGotInitialDebounceState)
        {
            mIsButtonPressed[btn_idx] = newIsButtonPressed[btn_idx];
        }
    }
    bool newIsProxDetected = IsProximityDetected();

    if (!mGotInitialDebounceState)
    {
        mIsProxDetected = newIsProxDetected;
        mGotInitialDebounceState = true;
        return;
    }

    for (size_t btn_idx = 0; btn_idx < static_cast<size_t>(ButtonId::kNumButtons); ++btn_idx)
    {
        if (newIsButtonPressed[btn_idx] != mIsButtonPressed[btn_idx])
        {
            mIsButtonPressed[btn_idx] = newIsButtonPressed[btn_idx];
            CallHardwareEventCallback(newIsButtonPressed[btn_idx] ? gSwitchPressEvents[btn_idx] : gSwitchReleaseEvents[btn_idx]);
        }
    }
}

bool GmdSilabsDriver::IsSwitchButtonPressed(ButtonId button_id) const
{
    if (button_id >= ButtonId::kNumButtons)
    {
        return false;
    }

    PortAndPin port_and_pin = gSwitchMappings[static_cast<size_t>(button_id)];
    return GPIO_PinInGet(port_and_pin.port, port_and_pin.pin) == 0;
}
    
bool GmdSilabsDriver::IsProximityDetected() const
{
    return GPIO_PinInGet(PROX_IN_PORT, PROX_IN_PIN) == 0;
}

void GmdSilabsDriver::SetLightLedEnabled(LedId led_id, bool enabled)
{
    if (led_id >= LedId::kNumLeds)
    {
        return;
    }

    PortAndPin port_and_pin = gLedMappings[static_cast<size_t>(led_id)];

    if (enabled)
    {
        GPIO_PinOutClear(port_and_pin.port, port_and_pin.pin);
    }
    else
    {
        GPIO_PinOutSet(port_and_pin.port, port_and_pin.pin);
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
    (void)code; //ManchesterOut(*this, &code, 1);
}

} // namespace matter
} // namespace google
