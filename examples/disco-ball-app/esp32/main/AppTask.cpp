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

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/disco-ball-server/disco-ball-cluster-logic.h>
#include <app/clusters/disco-ball-server/disco-ball-server.h>

#include <app/reporting/reporting.h>
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

class SampleDiscoBallDriver : public chip::app::DiscoBallClusterLogic::DriverInterface
{
  public:
    SampleDiscoBallDriver() = default;

    void Init()
    {
        // TODO.
    }

    // Implementation of chip::app::DiscoBallClusterLogic::DriverInterface
    DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const override
    {
        VerifyOrDie(endpoint_id == kDiscoBallEndpoint);

        DiscoBallCapabilities capabilities;

        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kWobble);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kReverse);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kStatistics);

        capabilities.min_speed_value = 0;
        capabilities.max_speed_value = 200;

        capabilities.min_wobble_speed_value = 0;
        capabilities.max_wobble_speed_value = 100;

        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleLeftRight);
        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleUpDown);

        return capabilities;
    }

    Status OnClusterStateChange(EndpointId endpoint_id, BitFlags<DiscoBallFunction> changes, DiscoBallClusterLogic & cluster)
    {
        // TODO: Implement
        ESP_LOGI(TAG, "Cluster state change received.");
        return Status::Success;
    }

    void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoBallTimerCallback timer_cb, void * ctx) override
    {
        // TODO: Implement
    }
    void CancelPatternTimer(EndpointId endpoint_id) override
    {
        // TODO: Implement
    }
    void MarkAttributeDirty(const ConcreteAttributePath& path) override
    {
        MatterReportingAttributeChangeCallback(path);
    }
};

QueueHandle_t sAppEventQueue;
TaskHandle_t sAppTaskHandle;

SampleDiscoBallDriver gDiscoBallDriver;
DefaultDiscoBallStorage gDiscoBallStorage;
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

    chip::DeviceLayer::PlatformMgr().LockChipStack();

    gDiscoBallServer = new DiscoBallServer(Clusters::DiscoBall::Id);
    VerifyOrDie(gDiscoBallServer != nullptr);
    gDiscoBallDriver.Init();
    ReturnErrorOnFailure(gDiscoBallServer->RegisterEndpoint(kDiscoBallEndpoint, gDiscoBallStorage, gDiscoBallDriver));

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

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

chip::app::DiscoBallClusterLogic::DriverInterface * GetDiscoBallDriver()
{
    return &gDiscoBallDriver;
}
