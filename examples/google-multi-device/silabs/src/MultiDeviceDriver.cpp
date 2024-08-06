// Copyrigh 2024 Google, All Rights Reserved.

#include <algorithm>

#include <stdint.h>
#include <cmsis_os2.h>

#include "MultiDeviceDriver.h"

#include "gpiointerrupt.h"

#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"

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

// ============ Start of driver code =============

namespace google {
namespace matter {

namespace {

uint32_t MillisToTicks(uint32_t millis)
{
    uint32_t millisPerTick = (1000u / osKernelGetTickFreq());
    return std::max(millis / millisPerTick, static_cast<uint32_t>(1u));
}

osTimerId_t sSwitchDebounceTimer = nullptr;
bool sButtonSwitchStateToCheck = false;
unsigned int sIntForButton = 0;
unsigned int sIntForProx = 0;

constexpr unsigned int kDebounceTimeMillis = 10u;

} // namespace

// Singleton instance.
GmdSilabsDriver GmdSilabsDriver::sInstance;


#if 0

/// \return timer ID for reference by other functions or NULL in case of error.
osTimerId_t osTimerNew (osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr);

/// Get name of a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \return name as NULL terminated string.
const char *osTimerGetName (osTimerId_t timer_id);

/// Start or restart a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \param[in]     ticks         \ref CMSIS_RTOS_TimeOutValue "time ticks" value of the timer.
/// \return status code that indicates the execution status of the function.
osStatus_t osTimerStart (osTimerId_t timer_id, uint32_t ticks);

/// Stop a timer.
/// \param[in]     timer_id      timer ID obtained by \ref osTimerNew.
/// \return status code that indicates the execution status of the function.
osStatus_t osTimerStop (osTimerId_t timer_id);

#endif


void GmdSilabsDriver::Init()
{
  /* Enable GPIO in CMU */
  CMU_ClockEnable(cmuClock_GPIO, true);

  sSwitchDebounceTimer = osTimerNew(GmdSilabsDriver::OnDebounceTimer, osTimerOnce, reinterpret_cast<void*>(BUTTON_IN_PIN), nullptr);
  VerifyOrDie(sSwitchDebounceTimer != nullptr);

  /* Initialize GPIO interrupt dispatcher */
  GPIOINT_Init();

  GPIO_PinModeSet(BUTTON_IN_PORT, BUTTON_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(PROX_IN_PORT, PROX_IN_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(LED_OUT_PORT, LED_OUT_PIN, gpioModePushPull, 1);

  /* Register callbacks before setting up and enabling pin interrupt. */
  sIntForButton = GPIOINT_CallbackRegisterExt(BUTTON_IN_PIN, GmdSilabsDriver::OnPinInterrupt, (void*)&GmdSilabsDriver::GetInstance());
  sIntForProx = GPIOINT_CallbackRegisterExt(PROX_IN_PIN, GmdSilabsDriver::OnPinInterrupt, (void*)&GmdSilabsDriver::GetInstance());

  /* Set fall and rising edge interrupts*/
  GPIO_ExtIntConfig(BUTTON_IN_PORT, BUTTON_IN_PIN, sIntForButton, true, true, true);
  GPIO_ExtIntConfig(PROX_IN_PORT, PROX_IN_PIN, sIntForProx, true, true, true);

  GPIO_IntEnable(GPIO_EnabledIntGet() | (1 << sIntForButton) | (1 << sIntForProx));

  // NVIC_EnableIRQ(GPIO_ODD_IRQn);
  // NVIC_EnableIRQ(GPIO_EVEN_IRQn);
}

void GmdSilabsDriver::OnDebounceTimer(void *ctx)
{
    uint32_t switch_pin = reinterpret_cast<uint32_t>(ctx);
    if (switch_pin == BUTTON_IN_PIN)
    {
        auto self = GmdSilabsDriver::GetInstance();
        self.SetLightLedEnabled(true);
        if (self.IsSwitchButtonPressed() == sButtonSwitchStateToCheck)
        {
            self.mHardwareCallback(self.IsSwitchButtonPressed() ? HardwareEvent::kSwitchButtonPressed : HardwareEvent::kSwitchButtonReleased);
        }

        GPIO_IntEnable(GPIO_EnabledIntGet() | (1 << sIntForButton));
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

// Static
void GmdSilabsDriver::OnPinInterrupt(uint8_t intNum, void *ctx)
{
    GmdSilabsDriver * self = (GmdSilabsDriver*)ctx;
    if (self->mHardwareCallback == nullptr)
    {
        return;
    }

    if (intNum == BUTTON_IN_PIN)
    {
        sButtonSwitchStateToCheck = self->IsSwitchButtonPressed();

        // Debounce the button before emitting events.
// TODO START TIMER
        osTimerStart(sSwitchDebounceTimer, MillisToTicks(kDebounceTimeMillis));
        GPIO_IntEnable(GPIO_EnabledIntGet() & ~(1 << sIntForButton));
    }
    else if (intNum == PROX_IN_PIN)
    {
        self->mHardwareCallback(self->IsProximityDetected() ? HardwareEvent::kOccupancyDetected : HardwareEvent::kOccupancyUndetected);
    }
}


} // namespace matter
} // namespace google
