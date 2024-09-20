/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppTask.h"
#include "AppConfig.h"
#include "AppEvent.h"

#include "LEDWidget.h"

#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <credentials/DeviceAttestationCredsProvider.h>

#include <assert.h>

#include <platform/silabs/platformAbstraction/SilabsPlatform.h>

#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

#include <lib/support/CodeUtils.h>

#include <platform/CHIPDeviceLayer.h>

#include "GoogleMultiDeviceCommon.h"

#include "MultiDeviceDriver.h"

#ifdef SL_CATALOG_SIMPLE_LED_LED1_PRESENT
#define LIGHT_LED 1
#else
#define LIGHT_LED 0
#endif

#define APP_FUNCTION_BUTTON 0
#define APP_LIGHT_SWITCH 1

using namespace chip;
using namespace chip::app;
using namespace google::matter;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Silabs;

// HACK: on Silabs to remove the Example DAC provider. Dunno why it's even needed.
namespace chip {
namespace Credentials {
namespace Examples {

DeviceAttestationCredentialsProvider * GetExampleDACProvider()
{
    return nullptr;
}

} // namespace Examples
} // namespace Credentials
} // namespace chip

namespace {
LEDWidget sLightLED;

// WARNING: CALLED FROM ISR CONTEXT
void MultiDeviceDriverEvent(HardwareEvent event)
{
    AppEvent app_event = {};
    app_event.Handler = AppTask::MultiDeviceDriverAppEventHandler;

    bool send_event = true;
    switch (event)
    {
        case HardwareEvent::kRedButtonPressed:
            app_event.Type = AppEvent::kEventType_kRedButtonPressed;
            break;
        case HardwareEvent::kRedButtonReleased:
            app_event.Type = AppEvent::kEventType_kRedButtonReleased;
            break;
        case HardwareEvent::kYellowButtonPressed:
            app_event.Type = AppEvent::kEventType_kYellowButtonPressed;
            break;
        case HardwareEvent::kYellowButtonReleased:
            app_event.Type = AppEvent::kEventType_kYellowButtonReleased;
            break;
        case HardwareEvent::kGreenButtonPressed:
            app_event.Type = AppEvent::kEventType_kGreenButtonPressed;
            break;
        case HardwareEvent::kGreenButtonReleased:
            app_event.Type = AppEvent::kEventType_kGreenButtonReleased;
            break;


        case HardwareEvent::kLatchSwitch1Selected:
            app_event.Type = AppEvent::kEventType_kLatchSwitch1Selected;
            break;

        case HardwareEvent::kLatchSwitch2Selected:
            app_event.Type = AppEvent::kEventType_kLatchSwitch2Selected;
            break;

        case HardwareEvent::kLatchSwitch3Selected:
            app_event.Type = AppEvent::kEventType_kLatchSwitch3Selected;
            break;

        case HardwareEvent::kOccupancyDetected:
            app_event.Type = AppEvent::kEventType_OccupancyDetected;
            break;
        case HardwareEvent::kOccupancyUndetected:
            app_event.Type = AppEvent::kEventType_OccupancyUndetected;
            break;

        default:
            send_event = false;
            break;
    }

    if (send_event)
    {
        AppTask::GetAppTask().PostEvent(&app_event);
    }
}

}

using namespace chip::TLV;
using namespace ::chip::DeviceLayer;

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    chip::DeviceLayer::Silabs::GetPlatform().SetButtonsCb(AppTask::ButtonEventHandler);

    GmdSilabsDriver::GetInstance().Init();
    GmdSilabsDriver::GetInstance().SetHardwareEventCallback(MultiDeviceDriverEvent);

    err = BaseApplication::Init();
    if (err != CHIP_NO_ERROR)
    {
        SILABS_LOG("BaseApplication::Init() failed");
        appError(err);
    }

    sLightLED.Init(LIGHT_LED);
    sLightLED.Set(false);

    return err;
}

CHIP_ERROR AppTask::StartAppTask()
{
    return BaseApplication::StartAppTask(AppTaskMain);
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
    osMessageQueueId_t sAppEventQueue = *(static_cast<osMessageQueueId_t *>(pvParameter));

    CHIP_ERROR err = sAppTask.Init();
    if (err != CHIP_NO_ERROR)
    {
        SILABS_LOG("AppTask.Init() failed");
        appError(err);
    }

#if !(defined(CHIP_CONFIG_ENABLE_ICD_SERVER) && CHIP_CONFIG_ENABLE_ICD_SERVER)
    sAppTask.StartStatusLEDTimer();
#endif

    chip::DeviceLayer::SystemLayer().ScheduleLambda([](){
        GoogleMultiDeviceIntegration::GetInstance().InitializeProduct();
    });

    SILABS_LOG("App Task started");

    while (true)
    {
        osStatus_t eventReceived = osMessageQueueGet(sAppEventQueue, &event, NULL, osWaitForever);
        while (eventReceived == osOK)
        {
            sAppTask.DispatchEvent(&event);
            eventReceived = osMessageQueueGet(sAppEventQueue, &event, NULL, 0);
        }
    }
}

void AppTask::MultiDeviceDriverAppEventHandler(AppEvent * aEvent)
{
    auto & productIntegration = GoogleMultiDeviceIntegration::GetInstance();

    switch (aEvent->Type)
    {
        case AppEvent::kEventType_kRedButtonPressed:
            productIntegration.HandleButtonPress(GoogleMultiDeviceIntegration::ButtonId::kRed);
            break;
        case AppEvent::kEventType_kRedButtonReleased:
            productIntegration.HandleButtonRelease(GoogleMultiDeviceIntegration::ButtonId::kRed);
            break;
        case AppEvent::kEventType_kYellowButtonPressed:
            productIntegration.HandleButtonPress(GoogleMultiDeviceIntegration::ButtonId::kYellow);
            break;
        case AppEvent::kEventType_kYellowButtonReleased:
            productIntegration.HandleButtonRelease(GoogleMultiDeviceIntegration::ButtonId::kYellow);
            break;
        case AppEvent::kEventType_kGreenButtonPressed:
            productIntegration.HandleButtonPress(GoogleMultiDeviceIntegration::ButtonId::kGreen);
            break;
        case AppEvent::kEventType_kGreenButtonReleased:
            productIntegration.HandleButtonRelease(GoogleMultiDeviceIntegration::ButtonId::kGreen);
            break;
        case AppEvent::kEventType_kLatchSwitch1Selected:
            productIntegration.HandleButtonPress(GoogleMultiDeviceIntegration::ButtonId::kLatch1);
            break;
        case AppEvent::kEventType_kLatchSwitch2Selected:
            productIntegration.HandleButtonPress(GoogleMultiDeviceIntegration::ButtonId::kLatch2);
            break;
        case AppEvent::kEventType_kLatchSwitch3Selected:
            productIntegration.HandleButtonPress(GoogleMultiDeviceIntegration::ButtonId::kLatch3);
            break;

        case AppEvent::kEventType_OccupancyDetected:
            productIntegration.HandleOccupancyDetected(0);
            break;
        case AppEvent::kEventType_OccupancyUndetected:
            productIntegration.HandleOccupancyUndetected(0);
            break;
        default:
            break;
    }
}

void AppTask::ButtonEventHandler(uint8_t button, uint8_t btnAction)
{
    AppEvent button_event           = {};
    button_event.Type               = AppEvent::kEventType_Button;
    button_event.ButtonEvent.Action = btnAction;

    if (button == APP_FUNCTION_BUTTON)
    {
        button_event.Handler = BaseApplication::ButtonHandler;
        AppTask::GetAppTask().PostEvent(&button_event);
    }
}

void ::google::matter::GoogleMultiDeviceIntegration::SetDebugLed(bool enabled)
{
    auto & driver = GmdSilabsDriver::GetInstance();
    driver.SetLightLedEnabled(GmdSilabsDriver::LedId::kRed, enabled);
}

void ::google::matter::GoogleMultiDeviceIntegration::EmitDebugCode(uint8_t code)
{
    auto & driver = GmdSilabsDriver::GetInstance();
    driver.EmitDebugCode(code);
}

uint8_t ::google::matter::GoogleMultiDeviceIntegration::GetEp4LatchInitialPosition()
{
    auto & driver = GmdSilabsDriver::GetInstance();
    if (driver.IsSwitchButtonPressed(GmdSilabsDriver::ButtonId::kLatch1))
    {
        return 0;
    }
    else if (driver.IsSwitchButtonPressed(GmdSilabsDriver::ButtonId::kLatch2))
    {
        return 1;
    }
    else if (driver.IsSwitchButtonPressed(GmdSilabsDriver::ButtonId::kLatch3))
    {
        return 2;
    }

    return 0;
}

bool ::google::matter::GoogleMultiDeviceIntegration::IsAlternativeDiscriminator()
{
    auto & driver = GmdSilabsDriver::GetInstance();
    return driver.IsAlternativeDiscriminator();
}
