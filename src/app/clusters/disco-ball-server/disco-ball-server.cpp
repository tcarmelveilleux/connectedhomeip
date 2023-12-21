/**
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include "disco-ball-server.h"
#include "disco-ball-cluster-logic.h"

#include <app-common/zap-generated/cluster-objects.h>
#include <app/CommandHandler.h>
#include <app/CommandHandlerInterface.h>
#include <app/ConcreteCommandPath.h>
#include <app/InteractionModelEngine.h>
#include <app/util/attribute-storage.h>
#include <access/SubjectDescriptor.h>
#include <lib/support/logging/CHIPLogging.h>

#include <protocols/interaction_model/StatusCode.h>

namespace chip {
namespace app {

using chip::Protocols::InteractionModel::ClusterStatusCode;
using chip::Protocols::InteractionModel::Status;
using chip::Access::SubjectDescriptor;

// This is the disco ball command handler for all endpoints (endpoint ID is NullOptional)
DiscoBallServer::DiscoBallServer(ClusterId cluster_id) : CommandHandlerInterface(chip::NullOptional, cluster_id), AttributeAccessInterface(chip::NullOptional, cluster_id)
{
  registerAttributeAccessOverride(this);
  chip::app::InteractionModelEngine::GetInstance()->RegisterCommandHandler(this);
}

DiscoBallServer::~DiscoBallServer()
{
  unregisterAttributeAccessOverride(this);
  chip::app::InteractionModelEngine::GetInstance()->UnregisterCommandHandler(this);
}

/* =========================== Start of DiscoBallServer =====================*/
void DiscoBallServer::InvokeCommand(HandlerContext & handlerContext)
{
    EndpointId endpoint_id = handlerContext.mRequestPath.mEndpointId;
    DiscoBallClusterLogic * cluster = FindEndpoint(endpoint_id);
    if (cluster == nullptr)
    {
        handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, Status::UnsupportedEndpoint);
        return;
    }

    SubjectDescriptor subject_descriptor = handlerContext.mCommandHandler.GetSubjectDescriptor();

    /*
    | ID     | Name           | Direction        | Response^**^      | Access | Conformance
    | 0x00  s| StartRequest   | client => server | Y                 | O T^*^ | M
    | 0x01  s| StopRequest    | client => server | Y                 | O      | M
    | 0x02  s| ReverseRequest | client => server | Y                 | O      | REV
    | 0x03  s| WobbleRequest  | client => server | Y                 | O      | WBL
    | 0x04  s| PatternRequest | client => server | Y                 | M      | PAT
    | 0x05  s| StatsRequest   | client => server | StatsResponse^**^ | O      | STA
    | 0x06  s| StatsResponse  | client <= server | N                 | O      | STA
    */
    // TODO: implement this for real.
    // TODO: check constraints, add in attribute change reporting as appropriate once this is all tied together
    switch (handlerContext.mRequestPath.mCommandId)
    {
    case Clusters::DiscoBall::Commands::StartRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::StartRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "DiscoBall StartRequest received");
            ClusterStatusCode status = cluster->HandleStartRequest(payload);
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::StopRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::StopRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "DiscoBall StopRequest received");
            ClusterStatusCode status = cluster->HandleStopRequest();
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::ReverseRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::ReverseRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "DiscoBall ReverseRequest received");
            ClusterStatusCode status = cluster->HandleReverseRequest();
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::WobbleRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::WobbleRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "DiscoBall WobbleRequest received");
            ClusterStatusCode status = cluster->HandleWobbleRequest();
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::PatternRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::PatternRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "DiscoBall PatternRequest received");
            FabricIndex fabric_index = subject_descriptor.fabricIndex;

            // Need a fabric to request pattern access, since fabric-scoped.
            if (fabric_index == kUndefinedFabricIndex)
            {
                ChipLogError(Zcl, "PatternRequest requested with no accessing fabric!");
                handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, Status::UnsupportedAccess);
                return;
            }

            ClusterStatusCode status = cluster->HandlePatternRequest(fabric_index, payload);
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::StatsRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::StatsRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "DiscoBall StatsRequest received");
            Clusters::DiscoBall::Commands::StatsResponse::Type resp;

            ClusterStatusCode status = cluster->HandleStatsRequest(resp);
            if (status != Status::Success)
            {
                handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
                return;
            }

            handlerContext.mCommandHandler.AddResponseData(handlerContext.mRequestPath, resp);
        });
        break;
    default:
        handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, Status::UnsupportedCommand);
        break;
    }
}

/*
| 0x0000  s| Run           | bool                        | all^*^         |         | 0       | R V T^*^ | M
| 0x0001  s| Rotate        | <<ref_RotateEnum>>          | all            |         | 0       | R V      | M
| 0x0002  s| Speed         | uint8                       | 0 to 200^*^    |         | 0       | R V      | M
| 0x0003  s| Axis          | uint8                       | 0 to 90            |         | 0       | RW VO    | AX \| WBL
| 0x0004  s| WobbleSpeed   | uint8                       | 0 to 200       |         | 0       | RW VO    | WBL
| 0x0005  s| Pattern       | list[<<ref_PatternStruct>>] | max 16^*^      | N       | 0       | RW VM    | PAT
| 0x0006  s| Name          | string                      | max 16         | N^*^    | 0       | RW VM    | P, O
| 0x0007  s| WobbleSupport | <<ref_WobbleBitmap>>        | desc           |         |         | R V      | [WBL]
| 0x0008  s| WobbleSetting | <<ref_WobbleBitmap>>        | desc           |         |         | RW VM    | [WBL]
*/
CHIP_ERROR DiscoBallServer::Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder)
{
    DiscoBallClusterLogic * cluster = FindEndpoint(aPath.mEndpointId);
    VerifyOrReturnError(cluster != nullptr, CHIP_IM_GLOBAL_STATUS(UnsupportedEndpoint));

    switch (aPath.mAttributeId)
    {
    case Clusters::DiscoBall::Attributes::Run::Id:
        ChipLogProgress(Zcl, "Read Run attribute");
        return aEncoder.Encode(cluster->GetRunAttribute());
    case Clusters::DiscoBall::Attributes::Rotate::Id:
        ChipLogProgress(Zcl, "Read Rotate attribute");
        return aEncoder.Encode(cluster->GetRotateAttribute());
    case Clusters::DiscoBall::Attributes::Speed::Id:
        ChipLogProgress(Zcl, "Read speed attribute");
        return aEncoder.Encode(cluster->GetSpeedAttribute());
    case Clusters::DiscoBall::Attributes::Axis::Id:
        ChipLogProgress(Zcl, "Read axis attribute");
        return aEncoder.Encode(cluster->GetAxisAttribute());
    case Clusters::DiscoBall::Attributes::WobbleSpeed::Id:
        ChipLogProgress(Zcl, "Read wobble speed attribute");
        return aEncoder.Encode(cluster->GetWobbleSpeedAttribute());
    case Clusters::DiscoBall::Attributes::Pattern::Id:
        ChipLogProgress(Zcl, "Read pattern attribute");
        // TODO: Encode the list
        return aEncoder.EncodeEmptyList();
    case Clusters::DiscoBall::Attributes::Name::Id:
        ChipLogProgress(Zcl, "Read name attribute");
        return aEncoder.Encode(cluster->GetNameAttribute());
    case Clusters::DiscoBall::Attributes::WobbleSupport::Id:
        ChipLogProgress(Zcl, "Read wobble support attribute");
        return aEncoder.Encode(cluster->GetWobbleSupportAttribute().Raw());
    case Clusters::DiscoBall::Attributes::WobbleSetting::Id:
        ChipLogProgress(Zcl, "Read wobble setting attribute");
        return aEncoder.Encode(cluster->GetWobbleSettingAttribute().Raw());
    case Clusters::DiscoBall::Attributes::FeatureMap::Id:
        return aEncoder.Encode(cluster->GetSupportedFeatures());
    default:
        break;
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiscoBallServer::Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder)
{
    // TODO: Add support here.
    // TODO: Don't forget to check constraints and report attribute changes
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiscoBallServer::RegisterEndpoint(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DiscoBallClusterLogic::DriverInterface & driver)
{
    VerifyOrReturnError(endpoint_id != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(FindEndpoint(endpoint_id) == nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    // Find a free registration slot.
    DiscoBallClusterLogic * endpoint_handler = nullptr;
    for (DiscoBallClusterLogic & candidate : mEndpoints)
    {
        if (candidate.GetEndpointId() == kInvalidEndpointId)
        {
            endpoint_handler = &candidate;
            break;
        }
    }

    ChipLogError(Zcl, "Registered DiscoBall endpoint %u on slot: %p", static_cast<unsigned>(endpoint_id), endpoint_handler);

    if (endpoint_handler == nullptr)
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    CHIP_ERROR err = endpoint_handler->Init(endpoint_id, storage, driver);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "Failed to init DiscoBall endpoint handler on endpoint %u: %" CHIP_ERROR_FORMAT, static_cast<unsigned>(endpoint_id), err.Format());
        endpoint_handler->Deinit();
        return err;
    }

    return CHIP_NO_ERROR;
}

void DiscoBallServer::UnregisterEndpoint(EndpointId endpoint_id)
{
    DiscoBallClusterLogic * endpoint_handler = FindEndpoint(endpoint_id);
    if (endpoint_handler == nullptr)
    {
        return;
    }

    endpoint_handler->Deinit();
}

DiscoBallClusterLogic * DiscoBallServer::FindEndpoint(EndpointId endpoint_id)
{
    for (DiscoBallClusterLogic & endpoint_handler : mEndpoints)
    {
        if (endpoint_handler.GetEndpointId() == endpoint_id)
        {
            return &endpoint_handler;
        }
    }

    return nullptr;
}

} // namespace app
} // namespace chip

void MatterDiscoBallPluginServerInitCallback() {}
