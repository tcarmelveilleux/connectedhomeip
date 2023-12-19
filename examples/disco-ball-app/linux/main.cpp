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

    // Implementation of chip::app::DiscoBallClusterLogic::DriverInterface
    DiscoBallCapabilities GetCapabilities(EndpointId endpoint_id) const override
    {
        VerifyOrDie(endpoint_id == kDiscoBallEndpoint);

        DiscoBallCapabilities capabilities;

        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kAxis);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kWobble);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kReverse);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kPattern);
        capabilities.supported_features.Set(Clusters::DiscoBall::Feature::kStatistics);

        capabilities.min_speed_value = 0;
        capabilities.max_speed_value = 200;

        capabilities.min_axis_value = 0;
        capabilities.max_axis_value = 90;

        capabilities.min_wobble_speed_value = 0;
        capabilities.max_wobble_speed_value = 100;

        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleLeftRight);
        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleUpDown);
        capabilities.wobble_support.Set(Clusters::DiscoBall::WobbleBitmap::kWobbleRound);

        return capabilities;
    }

    Status OnClusterStateChange(EndpointId endpoint_id, BitFlags<DiscoBallFunction> changes, DiscoBallClusterLogic & cluster)
    {
        ChipLogDetail(Zcl, "DiscoBall endpoint %u state change", static_cast<unsigned>(endpoint_id));

        if (changes.Has(DiscoBallFunction::kRunning))
        {
            ChipLogDetail(Zcl, "  --> Running = %s", (cluster.GetRunAttribute() ? "true" : "false"));
        }

        if (changes.Has(DiscoBallFunction::kRotation))
        {
            ChipLogDetail(Zcl, "  --> Rotate = %u", static_cast<unsigned>(cluster.GetRotateAttribute()));
        }

        if (changes.Has(DiscoBallFunction::kSpeed))
        {
            ChipLogDetail(Zcl, "  --> Speed = %u", static_cast<unsigned>(cluster.GetSpeedAttribute()));
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
