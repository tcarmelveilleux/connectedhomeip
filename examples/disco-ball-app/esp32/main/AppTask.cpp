/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/disco-ball-server/disco-ball-cluster-logic.h>
#include <app/clusters/disco-ball-server/disco-ball-server.h>

#include <app/reporting/reporting.h>
#include <app/server/Server.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>
#include <protocols/interaction_model/StatusCode.h>

#include "DefaultDiscoBallStorage.h"

#define APP_TASK_NAME "APP"
#define APP_EVENT_QUEUE_SIZE 10
#define APP_TASK_STACK_SIZE (3072)

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::DeviceLayer;

using chip::Protocols::InteractionModel::Status;

static const char TAG[] = "app-task";

namespace {

class DiscoBallHardware
{
  public:
    virtual void SetMotorSpeedDirection(bool is_clockwise, uint8_t normalized_speed) = 0;
    virtual void StopMotor() = 0;
    virtual void SetLightEnable(bool enabled) = 0;
    virtual void SetWobbleSetting(BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting) = 0;
    virtual void SetWobbleSpeed(uint8_t normalized_speed) = 0;
    virtual void StopWobble() = 0;
    virtual void SetAxis(uint8_t axis) = 0;
};

class ESP32S3DevKitCDiscoBallHardware : public DiscoBallHardware
{
  public:
    // Wobble MP3 player.
    static constexpr gpio_num_t kTxd1Pin = GPIO_NUM_17;
    static constexpr gpio_num_t kRxd1Pin = GPIO_NUM_18;
    static constexpr gpio_num_t kMp3PlayerIo1Pin = GPIO_NUM_15;
    static constexpr gpio_num_t kMp3PlayerIo2Pin = GPIO_NUM_16;
    static constexpr uart_port_t kUartNumber = UART_NUM_1;
    static constexpr uint32_t kUartBaudRate = 9600;

    // LED and motor via DRV8833 H-Bridge.
    static constexpr gpio_num_t kNotSleepPin = GPIO_NUM_4;
    static constexpr gpio_num_t kLedControlPin = GPIO_NUM_5;
    static constexpr gpio_num_t kMotorPwm1Pin = GPIO_NUM_6;
    static constexpr gpio_num_t kMotorPwm2Pin = GPIO_NUM_7;
    static constexpr gpio_num_t kFactoryResetButton = GPIO_NUM_8;

    void Init();

    void SetMotorSpeedDirection(bool is_clockwise, uint8_t normalized_speed) override;
    void StopMotor() override;
    void SetLightEnable(bool enabled) override;
    void SetWobbleSetting(BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting) override;
    void SetWobbleSpeed(uint8_t normalized_speed) override;
    void StopWobble() override;
    void SetAxis(uint8_t axis) override {}

    bool IsFactoryResetButtonPressed() const
    {
        return gpio_get_level(kFactoryResetButton) == 0;
    }

  private:
    bool mIsWobbleRunning = false;
};

void ESP32S3DevKitCDiscoBallHardware::Init()
{
    gpio_config_t io_conf = {};

    // LED and motor via DRV8833 H-Bridge.
    gpio_set_level(kNotSleepPin, 1);
    gpio_set_level(kLedControlPin, 0);
    gpio_set_level(kMotorPwm1Pin, 0);
    gpio_set_level(kMotorPwm2Pin, 0);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << kNotSleepPin) | (1ULL << kLedControlPin) | (1ULL << kMotorPwm1Pin) | (1ULL << kMotorPwm2Pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Set Wobble audio control pins.
    // TODO: Config UART1.
    gpio_set_level(kMp3PlayerIo1Pin, 1);
    gpio_set_level(kMp3PlayerIo2Pin, 1);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = (1ULL << kMp3PlayerIo1Pin) | (1ULL << kMp3PlayerIo2Pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Init factory-reset button.
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << kFactoryResetButton);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // TODO: Configure motor controller PWM.
}

void ESP32S3DevKitCDiscoBallHardware::SetMotorSpeedDirection(bool is_clockwise, uint8_t normalized_speed)
{
    // TODO: Support speed.
    if (is_clockwise)
    {
        gpio_set_level(kMotorPwm1Pin, 1);
        gpio_set_level(kMotorPwm2Pin, 0);
    }
    else
    {
        gpio_set_level(kMotorPwm1Pin, 0);
        gpio_set_level(kMotorPwm2Pin, 1);
    }
}

void ESP32S3DevKitCDiscoBallHardware::StopMotor()
{
    gpio_set_level(kMotorPwm1Pin, 0);
    gpio_set_level(kMotorPwm2Pin, 0);
}

void ESP32S3DevKitCDiscoBallHardware::SetLightEnable(bool enabled)
{
    gpio_set_level(kLedControlPin, enabled);
}

void ESP32S3DevKitCDiscoBallHardware::SetWobbleSetting(BitFlags<Clusters::DiscoBall::WobbleBitmap> wobble_setting)
{
    // TODO: Implement wobble setting.
}

void ESP32S3DevKitCDiscoBallHardware::SetWobbleSpeed(uint8_t normalized_speed)
{
    // TODO: Implement wobble speed.
    if (normalized_speed == 0)
    {
        return;
    }

    gpio_set_level(kMp3PlayerIo1Pin, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(kMp3PlayerIo1Pin, 1);
}

void ESP32S3DevKitCDiscoBallHardware::StopWobble()
{
    // TODO: Implement wobble stop.
}

class SampleDiscoBallDriver : public chip::app::DiscoBallClusterLogic::DriverInterface
{
  public:
    SampleDiscoBallDriver() = default;

    void Init(DiscoBallHardware & hardware)
    {
        mHardware = &hardware;
    }

    // Executed in AppTask context.
    static void ProcessFromAppTask(AppEvent *event)
    {
        // For all events we disregard EndpointId since this version
        // only handles a single global application endpoint and does
        // not contextualize by endpoint.

        SampleDiscoBallDriver * that = static_cast<SampleDiscoBallDriver *>(event->context);
        switch (event->event_type)
        {
            case AppEvent::AppEventTypes::kSetMotorDirectionSpeed: {
                bool is_clockwise = event->set_motor_direction_speed_event.is_clockwise;
                uint8_t speed_rpm = event->set_motor_direction_speed_event.speed_rpm;

                that->mHardware->SetMotorSpeedDirection(is_clockwise, speed_rpm);
                that->mHardware->SetWobbleSpeed(100);
                break;
            }
            case AppEvent::AppEventTypes::kStopMotor: {
                that->mHardware->StopMotor();
                break;
            }
            case AppEvent::AppEventTypes::kSetLightState: {
                bool enabled = event->set_light_state_event.enabled;
                that->mHardware->SetLightEnable(enabled);
                break;
            }
            case AppEvent::AppEventTypes::kUpdateWobble: {
                auto wobble_setting = event->update_wobble_event.wobble_setting;
                auto speed = event->update_wobble_event.speed;
                that->mHardware->SetWobbleSetting(wobble_setting);
                that->mHardware->SetWobbleSpeed(speed);
                break;
            }
            default:
                break;
        }
    }

    // Implementation of chip::app::DiscoBallClusterLogic::DriverInterface
    DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const override
    {
        VerifyOrDie(endpoint_id == kDiscoBallEndpoint);

        DiscoBallCapabilities capabilities;

        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kReverse);

        // TODO: Enable the following when supported.
#if 0
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kWobble);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kStatistics);
#endif

        capabilities.min_speed_value = 0;
        capabilities.max_speed_value = 200;

        capabilities.min_wobble_speed_value = 0;
        capabilities.max_wobble_speed_value = 100;

        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleLeftRight);
        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleUpDown);

        return capabilities;
    }

    // Called from Matter thread context: must post to AppTask.
    Status OnClusterStateChange(EndpointId endpoint_id, BitFlags<DiscoBallFunction> changes, DiscoBallClusterLogic & cluster)
    {
        if (changes.Has(DiscoBallFunction::kRunning))
        {
            ChipLogProgress(Zcl, "Ep %u Running = %s", static_cast<unsigned>(endpoint_id), (cluster.GetRunAttribute() ? "true" : "false"));
            mIsRunning = cluster.GetRunAttribute();
            mIsLightEnabled = mIsRunning;

            PostSetLightState(endpoint_id);
            if (mIsRunning)
            {
                PostSetMotorDirectionSpeed(endpoint_id);
            }
            else
            {
                PostStopMotor(endpoint_id);
            }
        }

        if (changes.Has(DiscoBallFunction::kRotation))
        {
            ChipLogProgress(Zcl, "Ep %u Rotate = %u", static_cast<unsigned>(endpoint_id), static_cast<unsigned>(cluster.GetRotateAttribute()));
            mIsClockwise = (cluster.GetRotateAttribute() == Clusters::DiscoBall::RotateEnum::kClockwise);
            if (mIsRunning)
            {
                PostSetMotorDirectionSpeed(endpoint_id);
            }
        }

        if (changes.Has(DiscoBallFunction::kSpeed))
        {
            ChipLogProgress(Zcl, "Ep %u Speed = %u", static_cast<unsigned>(endpoint_id), static_cast<unsigned>(cluster.GetSpeedAttribute()));
            mSpeed = cluster.GetSpeedAttribute();
            if (mIsRunning)
            {
                PostSetMotorDirectionSpeed(endpoint_id);
            }
        }

        if (changes.Has(DiscoBallFunction::kAxis))
        {
            ChipLogProgress(Zcl, "Ep %u Axis = %u", static_cast<unsigned>(endpoint_id), static_cast<unsigned>(cluster.GetAxisAttribute()));
            mAxis = cluster.GetAxisAttribute();
        }

        if (changes.Has(DiscoBallFunction::kWobbleSpeed))
        {
            ChipLogProgress(Zcl, "Ep %u WobbleSpeed = %u", static_cast<unsigned>(endpoint_id), static_cast<unsigned>(cluster.GetWobbleSpeedAttribute()));
            mWobbleSpeed = cluster.GetWobbleSpeedAttribute();
            PostUpdateWobble(endpoint_id);
        }

        if (changes.Has(DiscoBallFunction::kWobbleSetting))
        {
            ChipLogProgress(Zcl, "Ep %u WobbleSetting = %u", static_cast<unsigned>(endpoint_id), static_cast<unsigned>(cluster.GetWobbleSettingAttribute().Raw()));
            mWobbleSetting = cluster.GetWobbleSettingAttribute();
            PostUpdateWobble(endpoint_id);
        }

        if (changes.Has(DiscoBallFunction::kName))
        {
            CharSpan name = cluster.GetNameAttribute();
            ChipLogProgress(Zcl, "Ep %u Name = '%.*s'", static_cast<unsigned>(endpoint_id), static_cast<int>(name.size()), name.data());
        }

        return Status::Success;
    }

    // Called from Matter thread context: must post to AppTask.
    void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoBallTimerCallback timer_cb, void * ctx) override
    {
        // TODO: Implement
    }

    // Called from Matter thread context: must post to AppTask.
    void CancelPatternTimer(EndpointId endpoint_id) override
    {
        // TODO: Implement
    }

    void MarkAttributeDirty(const ConcreteAttributePath& path) override
    {
        MatterReportingAttributeChangeCallback(path);
    }

  private:
    void PostSetMotorDirectionSpeed(EndpointId endpoint_id)
    {
        AppEvent event;
        event.event_type = AppEvent::AppEventTypes::kSetMotorDirectionSpeed;
        event.set_motor_direction_speed_event.endpoint_id = endpoint_id;
        event.set_motor_direction_speed_event.is_clockwise = mIsClockwise;
        event.set_motor_direction_speed_event.speed_rpm = mSpeed;
        event.handler = ProcessFromAppTask;
        event.context = this;
        GetAppTask().PostEvent(&event);
    }

    void PostStopMotor(EndpointId endpoint_id)
    {
        AppEvent event;
        event.event_type = AppEvent::AppEventTypes::kStopMotor;
        event.stop_motor_event.endpoint_id = endpoint_id;
        event.handler = ProcessFromAppTask;
        event.context = this;
        GetAppTask().PostEvent(&event);
    }

    void PostSetLightState(EndpointId endpoint_id)
    {
        AppEvent event;
        event.event_type = AppEvent::AppEventTypes::kSetLightState;
        event.set_light_state_event.endpoint_id = endpoint_id;
        event.set_light_state_event.enabled = mIsLightEnabled;
        event.handler = ProcessFromAppTask;
        event.context = this;
        GetAppTask().PostEvent(&event);
    }

    void PostUpdateWobble(EndpointId endpoint_id)
    {
        AppEvent event;
        event.event_type = AppEvent::AppEventTypes::kUpdateWobble;
        event.update_wobble_event.endpoint_id = endpoint_id;
        event.update_wobble_event.wobble_setting = mWobbleSetting;
        event.update_wobble_event.speed = mWobbleSpeed;
        event.handler = ProcessFromAppTask;
        event.context = this;
        GetAppTask().PostEvent(&event);
    }

    DiscoBallHardware * mHardware = nullptr;
    bool mIsRunning = false;
    bool mIsClockwise = true;
    bool mIsLightEnabled = false;
    uint8_t mSpeed = 0;
    uint8_t mAxis = 0;
    uint8_t mWobbleSpeed = 0;
    chip::BitFlags<chip::app::Clusters::DiscoBall::WobbleBitmap> mWobbleSetting{0};
};

QueueHandle_t sAppEventQueue;
TaskHandle_t sAppTaskHandle;

SampleDiscoBallDriver gDiscoBallDriver;
DefaultDiscoBallStorage gDiscoBallStorage;
ESP32S3DevKitCDiscoBallHardware gDiscoBallHardware;
DiscoBallServer * gDiscoBallServer = nullptr;

} // namespace

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::StartAppTask()
{
    sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
    if (sAppEventQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate app event queue");
        return APP_ERROR_EVENT_QUEUE_FAILED;
    }

    // Start App task.
    BaseType_t xReturned;
    xReturned = xTaskCreate(AppTaskMain, APP_TASK_NAME, APP_TASK_STACK_SIZE, NULL, 1, &sAppTaskHandle);
    return (xReturned == pdPASS) ? CHIP_NO_ERROR : APP_ERROR_CREATE_TASK_FAILED;
}

CHIP_ERROR AppTask::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    gDiscoBallHardware.Init();

    chip::DeviceLayer::PlatformMgr().LockChipStack();

    gDiscoBallServer = new DiscoBallServer(Clusters::DiscoBall::Id);
    VerifyOrDie(gDiscoBallServer != nullptr);
    gDiscoBallDriver.Init(gDiscoBallHardware);
    ReturnErrorOnFailure(gDiscoBallServer->RegisterEndpoint(kDiscoBallEndpoint, gDiscoBallStorage, gDiscoBallDriver));

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    // Check for factory reset, only at init, via button pressed.
    if (gDiscoBallHardware.IsFactoryResetButtonPressed())
    {
        // Debounce by 50ms
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if (gDiscoBallHardware.IsFactoryResetButtonPressed())
        {
            ESP_LOGE(TAG, "Factory reset button pressed!");
            while (gDiscoBallHardware.IsFactoryResetButtonPressed())
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            ESP_LOGE(TAG, "Factory reset button released, triggering factory reset.");
            chip::DeviceLayer::PlatformMgr().LockChipStack();
            Server::GetInstance().ScheduleFactoryReset();
            chip::DeviceLayer::PlatformMgr().UnlockChipStack();
        }
    }

    return err;
}

void AppTask::AppTaskMain(void * pvParameter)
{
    AppEvent event;
    CHIP_ERROR err = sAppTask.Init();
    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGI(TAG, "AppTask.Init() failed due to %" CHIP_ERROR_FORMAT, err.Format());
        return;
    }

    ESP_LOGI(TAG, "App Task started");

    while (true)
    {
        BaseType_t eventReceived = xQueueReceive(sAppEventQueue, &event, pdMS_TO_TICKS(10));
        while (eventReceived == pdTRUE)
        {
            sAppTask.DispatchEvent(&event);
            eventReceived = xQueueReceive(sAppEventQueue, &event, 0); // return immediately if the queue is empty
        }
    }
}

void AppTask::PostEvent(const AppEvent * aEvent)
{
    if (sAppEventQueue != NULL)
    {
        BaseType_t status;
        if (xPortInIsrContext())
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            status                         = xQueueSendFromISR(sAppEventQueue, aEvent, &higherPrioTaskWoken);
        }
        else
        {
            status = xQueueSend(sAppEventQueue, aEvent, 1);
        }
        if (!status)
            ESP_LOGE(TAG, "Failed to post event to app task event queue");
    }
    else
    {
        ESP_LOGE(TAG, "Event Queue is NULL should never happen");
    }
}

void AppTask::DispatchEvent(AppEvent * aEvent)
{
    if (aEvent->handler)
    {
        aEvent->handler(aEvent);
    }
    else
    {
        ESP_LOGI(TAG, "Event received with no handler. Dropping event.");
    }
}

chip::app::DiscoBallClusterLogic::DriverInterface * GetDiscoBallDriver()
{
    return &gDiscoBallDriver;
}
