/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include <AppMain.h>

#if PI_DISCO_BALL
#include <pigpiod_if2.h>
#endif

#include <memory>
#include <utility>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/reporting/reporting.h>
#include <app/clusters/disco-ball-server/disco-ball-cluster-logic.h>
#include <app/clusters/disco-ball-server/disco-ball-server.h>
#include <app/server/Server.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <protocols/interaction_model/StatusCode.h>

#include "../disco-ball-common/include/DefaultDiscoBallStorage.h"

using namespace chip;
using namespace chip::app;

using chip::Protocols::InteractionModel::Status;

namespace {

constexpr chip::EndpointId kDiscoBallEndpoint = 1;

class SampleDiscoBallDriver : public DiscoBallClusterLogic::DriverInterface
{
  public:
    SampleDiscoBallDriver() = default;
    ~SampleDiscoBallDriver() {
        Shutdown();
    }

    // Implementation of chip::app::DiscoBallClusterLogic::DriverInterface
    DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const override
    {
        VerifyOrDie(endpoint_id == kDiscoBallEndpoint);

        DiscoBallCapabilities capabilities;

        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kReverse);

        capabilities.min_speed_value = 0;
        capabilities.max_speed_value = 200;

        capabilities.min_axis_value = 0;
        capabilities.max_axis_value = 90;

        capabilities.min_wobble_speed_value = 0;
        capabilities.max_wobble_speed_value = 100;

#if !PI_DISCO_BALL
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kAxis);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kWobble);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kPattern);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kStatistics);

        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleLeftRight);
        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleUpDown);
        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleRound);
#endif

        return capabilities;
    }

    Status OnClusterStateChange(EndpointId endpoint_id, BitFlags<DiscoBallFunction> changes, DiscoBallClusterLogic & cluster)
    {
        ChipLogDetail(Zcl, "DiscoBall endpoint %u state change", static_cast<unsigned>(endpoint_id));
        bool set_run_mode = false;

        if (changes.Has(DiscoBallFunction::kRunning))
        {
            ChipLogDetail(Zcl, "  --> Running = %s", (cluster.GetRunAttribute() ? "true" : "false"));
            set_run_mode = true;
        }

        if (changes.Has(DiscoBallFunction::kRotation))
        {
            ChipLogDetail(Zcl, "  --> Rotate = %u", static_cast<unsigned>(cluster.GetRotateAttribute()));
            set_run_mode = true;
        }

        if (changes.Has(DiscoBallFunction::kSpeed))
        {
            ChipLogDetail(Zcl, "  --> Speed = %u", static_cast<unsigned>(cluster.GetSpeedAttribute()));
            set_run_mode = true;
        }

        if (changes.Has(DiscoBallFunction::kAxis))
        {
            ChipLogDetail(Zcl, "  --> Axis = %u", static_cast<unsigned>(cluster.GetAxisAttribute()));
        }

        if (changes.Has(DiscoBallFunction::kWobbleSpeed))
        {
            ChipLogDetail(Zcl, "  --> WobbleSpeed = %u", static_cast<unsigned>(cluster.GetWobbleSpeedAttribute()));
        }

        if (changes.Has(DiscoBallFunction::kWobbleSetting))
        {
            ChipLogDetail(Zcl, "  --> WobbleSetting = %u", static_cast<unsigned>(cluster.GetWobbleSettingAttribute().Raw()));
        }

        if (changes.Has(DiscoBallFunction::kName))
        {
            CharSpan name = cluster.GetNameAttribute();
            ChipLogDetail(Zcl, "  --> Name = '%.*s'", static_cast<int>(name.size()), name.data());
        }
        if (set_run_mode) {
            SetRunMode(cluster);
        }

        return Status::Success;
    }

    void StartPatternTimer(EndpointId endpoint_id, uint16_t num_seconds, DiscoBallTimerCallback timer_cb, void * ctx) override
    {
        ChipLogDetail(Zcl, "DiscoBall endpoint %u start pattern timer for %u seconds", static_cast<unsigned>(endpoint_id), static_cast<unsigned>(num_seconds));
    }

    void CancelPatternTimer(EndpointId endpoint_id) override
    {
        ChipLogDetail(Zcl, "DiscoBall endpoint %u cancel pattern timer", static_cast<unsigned>(endpoint_id));
    }

    void MarkAttributeDirty(const ConcreteAttributePath& path) override
    {
        MatterReportingAttributeChangeCallback(path);
    }
    void Init() {
#if PI_DISCO_BALL
        mPigpio = pigpio_start(nullptr, nullptr);
        if (mPigpio < 0) {
            ChipLogError(Zcl, "Unable to start pigpio");
            return;
        }
        if (set_mode(mPigpio, mGpioForward, PI_PUD_DOWN) != 0 || set_mode(mPigpio, mGpioReverse, PI_PUD_DOWN) != 0) {
            ChipLogError(Zcl, "Unable to set output mode");
            ShutdownGpio();
            return;
        }
        if (set_pull_up_down(mPigpio, mGpioForward, PI_PUD_DOWN) != 0 || set_pull_up_down(mPigpio, mGpioReverse, PI_PUD_DOWN) != 0) {
            ChipLogError(Zcl, "Unable to set pulldown mode");
            ShutdownGpio();
            return;
        }
        ChipLogProgress(Zcl, "Set up Raspi pin PWM on %d---------------------------------------", mPigpio);
#endif
    }
    void ShutdownGpio() {
        if (mPigpio >= 0) {
            pigpio_stop(mPigpio);
            mPigpio = -1;
        }
    }
    void Shutdown() {
        ShutdownGpio();
    }
private:
    int mPigpio = -1;
    int mGpioForward = 20;
    int mGpioReverse = 26;
    CHIP_ERROR SetRunMode(DiscoBallClusterLogic & cluster) {
#if PI_DISCO_BALL
	int gpio_err_forward = set_PWM_dutycycle(mPigpio, mGpioForward, 0);
        int gpio_err_reverse = set_PWM_dutycycle(mPigpio, mGpioReverse, 0);
        if (gpio_err_forward != 0 || gpio_err_reverse != 0) {
            ChipLogError(Zcl, "Error setting PWM duty cycle on GPIO pins. Unable to stop disco ball. err forward = %s err reverse = %s", pigpio_error(gpio_err_forward), pigpio_error(gpio_err_reverse));
            return CHIP_ERROR_INCORRECT_STATE;
        }
        if (cluster.GetRunAttribute()) {
            // Duty cycle of less than 100 doens't work for the current motor, so use that as a minimum
            // Duty cycle range is therefore 100-200, motor range is 0-200, so just map the range as appropriate
            // Note that this has no bearing on reality becuase that motor is spinning like the dickens
            int speed = (cluster.GetSpeedAttribute() >> 1) + 100;
            switch(cluster.GetRotateAttribute()) {
            case Clusters::DiscoBall::RotateEnum::kClockwise:
                gpio_err_forward = set_PWM_dutycycle(mPigpio, mGpioForward, speed);
                if (gpio_err_forward != 0) {
                    ChipLogError(Zcl, "Unable to start GPIO PWM: %s", pigpio_error(gpio_err_forward));
                }
		break;
            case Clusters::DiscoBall::RotateEnum::kCounterClockwise:
                gpio_err_reverse = set_PWM_dutycycle(mPigpio, mGpioReverse, speed);
                if (gpio_err_reverse != 0) {
                    ChipLogError(Zcl, "Unable to start GPIO PWM: %s", pigpio_error(gpio_err_reverse));
                }
                break;
            default:
                return CHIP_ERROR_INCORRECT_STATE;
            }
        }
#endif
        return CHIP_NO_ERROR;
    }
};

SampleDiscoBallDriver gDiscoBallDriver;
DefaultDiscoBallStorage gDiscoBallStorage;
std::unique_ptr<DiscoBallServer> gDiscoBallServer;

} // namespace



void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
}

void ApplicationInit()
{
    gDiscoBallServer = std::make_unique<DiscoBallServer>(Clusters::DiscoBall::Id);
    VerifyOrDie(gDiscoBallServer != nullptr);
    VerifyOrDie(gDiscoBallServer->RegisterEndpoint(kDiscoBallEndpoint, gDiscoBallStorage, gDiscoBallDriver) == CHIP_NO_ERROR);
}

void ApplicationShutdown()
{
    gDiscoBallServer->UnregisterEndpoint(kDiscoBallEndpoint);
    gDiscoBallServer.reset();
}

int main(int argc, char * argv[])
{
    if (ChipLinuxAppInit(argc, argv) != 0)
    {
        return -1;
    }

    ChipLinuxAppMainLoop();

    return 0;
}
