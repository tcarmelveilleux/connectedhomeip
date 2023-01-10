/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
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

#include <array>
#include "AppTask.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include <support/logging/CHIPLogging.h>


#include <app-common/zap-generated/attribute-id.h>
#include <app-common/zap-generated/attribute-type.h>
#include <app-common/zap-generated/attributes/Accessors.h>

#define APP_TASK_NAME "APP"
#define APP_EVENT_QUEUE_SIZE 10
#define APP_TASK_STACK_SIZE (3072)
#define BUTTON_PRESSED 1
#define APP_LIGHT_SWITCH 1

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

static const char * TAG = "app-task";

LEDWidget AppLED;
Button AppButton;

namespace {

constexpr EndpointId kLightEndpointId = 1;
QueueHandle_t sAppEventQueue;
TaskHandle_t sAppTaskHandle;

constexpr uint8_t kNoteOff = 0x80;
constexpr uint8_t kNoteOn = 0x90;
constexpr uint8_t kNoteIdForOne = 62;

static const std::array<gpio_num_t, 4> kMidiGpios = { GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_14, GPIO_NUM_27 };

template <size_t N>
class XylophoneMidiHandler
{
  public:
    XylophoneMidiHandler(const std::array<gpio_num_t, N> & gpioPositions, uint8_t noteIdForOne) : mGpioPositions(gpioPositions), mNoteIdForOne(noteIdForOne) {}

    void Init()
    {
        for (auto gpioNum : mGpioPositions)
        {
            // Start low, before GPIO config
            gpio_set_level(gpioNum, 0);

            // Configure all pins as outputs
            gpio_config_t io_conf = {};
            // interrupt of rising edge
            io_conf.intr_type = GPIO_INTR_DISABLE;
            // bit mask of the pins, use GPIO4/5 here
            io_conf.pin_bit_mask = (1ULL << static_cast<uint64_t>(gpioNum));
            // set as input mode
            io_conf.mode = GPIO_MODE_OUTPUT;
            // enable pull-up mode
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);
        }
    }

    void Handle(ByteSpan midiBytes)
    {
        VerifyOrReturn(midiBytes.size() >= 4);
        // Only Note-ON supported
        uint8_t command = midiBytes.data()[0] & 0xF0;
        uint8_t channel = midiBytes.data()[0] & 0x0F;
        uint8_t note = midiBytes.data()[1];
        uint8_t velocity = midiBytes.data()[2];
        uint8_t on_delay_ms = ((midiBytes.data()[3] >> 4) & 0x0F) * 10;
        uint8_t off_delay_ms = ((midiBytes.data()[3] >> 0) & 0x0F) * 10;

        VerifyOrReturn((command == kNoteOn) || (command == kNoteOff));
        VerifyOrReturn((note >= mNoteIdForOne) && (channel == 0));

        // Note ID 0 doesn't sound on Xylophone (it is the rest)
        uint8_t noteId = (note - mNoteIdForOne) + 1;
        bool useNote = (noteId < kNumPositions);
        bool isNoteOn = (command == kNoteOn) && (velocity > 0);

        ChipLogProgress(AppServer, "%s 0x%02x, %s (%d)", (isNoteOn ? "NoteOn" : "NoteOff"), static_cast<unsigned>(note), (useNote ? "usable" : "unusable"), static_cast<int>(noteId));
        if (isNoteOn && useNote)
        {
            SetNote(noteId);
            vTaskDelay(on_delay_ms / portTICK_RATE_MS);
            ClearNote();
            vTaskDelay(off_delay_ms / portTICK_RATE_MS);
        }
    }

  private:
    void SetNote(uint8_t noteId)
    {
        uint8_t mask = 1;
        for (auto gpioNum : mGpioPositions)
        {
            gpio_set_level(gpioNum, (noteId & mask) ? 1 : 0);
            mask <<= 1;
        }
    }

    // Needed to reset the trigger circuit
    void ClearNote()
    {
        for (auto gpioNum : mGpioPositions)
        {
            gpio_set_level(gpioNum, 0);
        }
    }

    static constexpr size_t kNumPositions = (1 << N);
    const std::array<gpio_num_t, N> & mGpioPositions;
    uint8_t mNoteIdForOne;
};

XylophoneMidiHandler gXylophoneMidiHandler(kMidiGpios, kNoteIdForOne);

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

    AppLED.Init();
    AppButton.Init();
    gXylophoneMidiHandler.Init();

    AppButton.SetButtonPressCallback(ButtonPressCallback);

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
    if (aEvent->mHandler)
    {
        aEvent->mHandler(aEvent);
    }
    else
    {
        ESP_LOGI(TAG, "Event received with no handler. Dropping event.");
    }
}

void AppTask::LightingActionEventHandler(AppEvent * aEvent)
{
    AppLED.Toggle();
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    sAppTask.UpdateClusterState();
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

void AppTask::MidiEventHandler(AppEvent * event)
{
    gXylophoneMidiHandler.Handle(ByteSpan{event->MidiEvent.midiBytes});
}

void AppTask::ButtonPressCallback()
{
    AppEvent button_event;
    button_event.Type     = AppEvent::kEventType_Button;
    button_event.mHandler = AppTask::LightingActionEventHandler;
    sAppTask.PostEvent(&button_event);
}

void AppTask::PostMidiEvent(chip::ByteSpan midiBytes)
{
    AppEvent midiEvent;

    VerifyOrReturn(midiBytes.size() == sizeof(midiEvent.MidiEvent.midiBytes));

    midiEvent.Type = AppEvent::AppEventTypes::kEventType_CustomMidi;
    midiEvent.mHandler = AppTask::MidiEventHandler;
    memcpy(&midiEvent.MidiEvent.midiBytes[0], midiBytes.data(), sizeof(midiEvent.MidiEvent.midiBytes));

    sAppTask.PostEvent(&midiEvent);
}

void AppTask::UpdateClusterState()
{
    ESP_LOGI(TAG, "Writing to OnOff cluster");
    // write the new on/off value
    EmberAfStatus status = Clusters::OnOff::Attributes::OnOff::Set(kLightEndpointId, AppLED.IsTurnedOn());

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        ESP_LOGE(TAG, "Updating on/off cluster failed: %x", status);
    }

    ESP_LOGI(TAG, "Writing to Current Level cluster");
    status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kLightEndpointId, AppLED.GetLevel());

    if (status != EMBER_ZCL_STATUS_SUCCESS)
    {
        ESP_LOGE(TAG, "Updating level cluster failed: %x", status);
    }
}
