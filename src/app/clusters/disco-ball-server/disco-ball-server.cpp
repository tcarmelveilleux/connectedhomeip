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

#include <app-common/zap-generated/cluster-objects.h>
#include <app/CommandHandler.h>
#include <app/CommandHandlerInterface.h>
#include <app/ConcreteCommandPath.h>
#include <app/InteractionModelEngine.h>
#include <app/clusters/disco-ball-server/disco-ball-server.h>
#include <app/server/Server.h>
#include <app/util/af.h>
#include <app/util/attribute-storage.h>

namespace {
// TODO: This is rather yucky, but it's Sunday night and I'm being lazy
chip::app::DiscoBallCommandHandler gCommandhandler;
chip::app::DiscoBallAttributeAccess gAttributeAccess;
} // namespace

namespace chip {
namespace app {

CHIP_ERROR ClusterState::Init(EndpointId endpoint_id, NonVolatileStorageInterface & storage)
{
    mEndpointId = endpoint_id;
    mStorage = storage;

    this->last_run_statistic = 0;
    this->patterns_statistic = 0;

    this->run_attribute = false;
    this->rotate_attribute = DiscoBall::RotateEnum::kClockwise;
    this->speed_attribute = 0;
    this->axis_attribute = 0;
    this->wobble_speed_attribute = 0;
    this->num_patterns = 0;
    // Don't touch the actual patterns, the pattern loading below will do it.

    this->name_attribute = CharSpan{};

    this->wobble_setting_attribute = BitFlags<DiscoBall::WobbleBitmap>{};

    CHIP_ERROR err = mStorage.LoadFromStorage(*this);
    if ((err != CHIP_NO_ERROR) && (err != CHIP_ERROR_NOT_FOUND))
    {
        Deinit();
        return err;
    }

    return CHIP_NO_ERROR;
}

/* =========================== Start of DiscoBallClusterLogic =====================*/
CHIP_ERROR DiscoBallClusterLogic::Init(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DiscoBallDriverInterface & driver)
{
    mEndpointId = endpoint_id;

    mDriver = &driver;
    mCapabilities = mDriver->GetCapabilities(mEndpointId);

    CHIP_ERROR err = mClusterState.Init(endpoint_id, storage);
    if (err != CHIP_NO_ERROR)
    {
        Deinit();
        return err;
    }

    // Override defaults with capabilities;
    mClusterState.speed_attribute = mCapabilities.min_speed_value;
    mClusterState.axis_attribute = mCapabilities.min_axis_value;
    mClusterState.wobble_speed_attribute = mCapabilities.min_wobble_speed_value;
}

bool DiscoBallClusterLogic::GetRunAttribute() const
{
    return mClusterState.run_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetRunAttribute(bool run_state)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

DiscoBall::RotateEnum DiscoBallClusterLogic::GetRotateAttribute() const
{
    return mClusterState.rotate_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetRotateAttribute(bool DiscoBall::RotateEnum rotate_state)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

uint8_t DiscoBallClusterLogic::GetSpeedAttribute() const
{
    return mClusterState.speed_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetSpeedAttribute(bool DiscoBall::RotateEnum rotate_state)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

uint8_t DiscoBallClusterLogic::GetAxisAttribute() const
{
    return mClusterState.axis_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetAxisAttribute(uint8_t axis)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

uint8_t DiscoBallClusterLogic::GetWobbleSpeedAttribute() const
{
    return mClusterState.wobble_speed_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetWobbleSpeedAttribute(uint8_t wobble_speed)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

size_t DiscoBallClusterLogic::GetNumPatterns(FabricIndex fabric_idx) const
{
    // With no accessing fabric, return total number of entries.
    if (fabric_idx == kUndefinedFabricIndex)
    {
        return mClusterState.num_patterns;
    }

    // With accessing fabric, return count only of those matching the accessing fabric.
    size_t patter_count = 0;
    for (size_t pattern_idx = 0; pattern_idx < mClusterState.num_patterns; ++pattern_idx)
    {
        const auto & pattern = mClusterState.pattern_attribute[pattern_idx];
        if (pattern.GetValue().fabricIndex == fabric_idx)
        {
            ++pattern_count;
        }
    }

    return pattern_count;
}

DiscoBallPatternStructBacking DiscoBallClusterLogic::GetPatternAttributeEntry(FabricIndex fabric_idx, size_t pattern_idx) const
{
    return chip::NullOptional;
}

CHIP_ERROR DiscoBallClusterLogic::ClearPattern(FabricIndex fabric_idx, size_t pattern_idx)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

InteractionModel::Status DiscoBallClusterLogic::SetPattern(FabricIndex fabric_idx, const Clusters::DiscoBall::Structs::PatternStruct::Type & pattern)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

CharSpan DiscoBallClusterLogic::GetNameAttribute() const
{
    return mClusterState.name_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetNameAttribute(CharSpan name)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

BitFlags<DiscoBall::WobbleBitmap> DiscoBallClusterLogic::GetWobbleSupportAttribute() const
{
    if (!mDriver)
    {
        return BitFlags<DiscoBall::WobbleBitmap>{0};
    }

    return mDriver->GetWobbleSupport(mEndpointId);
}

BitFlags<DiscoBall::WobbleBitmap> DiscoBallClusterLogic::GetWobbleSettingAttribute() const
{
    return mClusterState.wobble_setting_attribute;
}

InteractionModel::Status DiscoBallClusterLogic::SetWobbleSettingAttribute(BitFlags<DiscoBall::WobbleBitmap> wobble_setting)
{
    return InteractionModel::Status::UnsupportedAttribute;
}

BitFlags<DiscoBall::Feature> DiscoBallClusterLogic::GetSupportedFeatures() const
{
    if (!mDriver)
    {
        return BitFlags<DiscoBall::Feature>{0};
    }

    return mDriver->GetSupportedFeatures(mEndpointId);
}

InteractionModel::Status HandleStartRequest(const Clusters::DiscoBall::Commands::StartRequest::DecodableType & args)
{
    return InteractionModel::Status::UnsupportedCommand;
}

InteractionModel::Status HandleStopRequest()
{
    return InteractionModel::Status::UnsupportedCommand;
}

InteractionModel::Status HandleReverseRequest()
{
    return InteractionModel::Status::UnsupportedCommand;
}

InteractionModel::Status HandleWobbleRequest()
{
    return InteractionModel::Status::UnsupportedCommand;
}

InteractionModel::Status HandlePatternRequest(FabricIndex fabric_index, const Clusters::DiscoBall::Commands::PatternRequest::DecodableType & args)
{
    return InteractionModel::Status::UnsupportedCommand;
}

InteractionModel::Status HandleStatsRequest(Clusters::DiscoBall::Commands::StatsResponse::Type & out_stats_response)
{
    return InteractionModel::Status::UnsupportedCommand;
}

/* =========================== Start of DiscoBallServer =====================*/
void DiscoBallServer::InvokeCommand(HandlerContext & handlerContext)
{
    DiscoBallClusterLogic * cluster = FindEndpoint(aPath.mEndpointId);
    VerifyOrReturnError(cluster != nullptr, CHIP_IM_GLOBAL_STATUS(InteractionModel::Status::UnsupportedEndpoint));

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
    ChipLogProgress(Zcl, "Handle disco ball command --------------------");
    // TODO: implement this for real.
    // TODO: check constraints, add in attribute change reporting as appropriate once this is all tied together
    switch (handlerContext.mRequestPath.mCommandId)
    {
    case Clusters::DiscoBall::Commands::StartRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::StartRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "StartRequest received");
            // TODO: CHeck for timed request!
            InteractionModel::Status status = cluster->HandleStartRequest(payload);
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::StopRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::StopRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "StopRequest received");
            InteractionModel::Status status = cluster->HandleStopRequest();
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::ReverseRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::ReverseRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "ReverseRequest received");
            InteractionModel::Status status = cluster->HandleReverseRequest();
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::WobbleRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::WobbleRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "WobbleRequest received");
            InteractionModel::Status status = cluster->HandleWobbleRequest();
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::PatternRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::PatternRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "PatternRequest received");
            FabricIndex fabric_index = handlerContext.GetSubjectDescriptor().fabricIndex;

            // Need a fabric to request pattern access, since fabric-scoped.
            if (fabric_index == kUndefinedFabricIndex)
            {
                ChipLogError(Zcl, "PatternRequest requested with no accessing fabric!");
                handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, InteractionModel::Status::InvalidAccess);
                return;
            }

            InteractionModel::Status status = cluster->HandlePatternRequest(fabric_index, payload);
            handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
        });
        break;
    case Clusters::DiscoBall::Commands::StatsRequest::Id:
        HandleCommand<Clusters::DiscoBall::Commands::StatsRequest::DecodableType>(handlerContext, [&](auto & _u, auto & payload) {
            ChipLogProgress(Zcl, "StatsRequest received");
            Clusters::DiscoBall::Commands::StatsResponse::Type resp;

            InteractionModel::Status status = cluster->HandleStatsRequest(resp);
            if (status != InteractionModel::Status::Success)
            {
                handlerContext.mCommandHandler.AddStatus(handlerContext.mRequestPath, status);
                return;
            }

            handlerContext.mCommandHandler.AddResponseData(handlerContext.mRequestPath, resp);
        });
        break;
    }
}

/*
| 0x0000  s| Run           | bool                        | all^*^         |         | 0       | R V T^*^ | M
| 0x0001  s| Rotate        | <<ref_RotateEnum>>          | all            |         | 0       | R V      | M
| 0x0002  s| Speed         | uint8                       | 0 to 200^*^    |         | 0       | R V      | M
| 0x0003  s| Axis          | uint8                       | 0 to 90        |         | 0       | RW VO    | AX \| WBL
| 0x0004  s| WobbleSpeed   | uint8                       | 0 to 200       |         | 0       | RW VO    | WBL
| 0x0005  s| Pattern       | list[<<ref_PatternStruct>>] | max 16^*^      | N       | 0       | RW VM    | PAT
| 0x0006  s| Name          | string                      | max 16         | N^*^    | 0       | RW VM    | P, O
| 0x0007  s| WobbleSupport | <<ref_WobbleBitmap>>        | desc           |         |         | R V      | [WBL]
| 0x0008  s| WobbleSetting | <<ref_WobbleBitmap>>        | desc           |         |         | RW VM    | [WBL]
*/
CHIP_ERROR DiscoBallServer::Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder)
{
    DiscoBallClusterLogic * cluster = FindEndpoint(aPath.mEndpointId);
    VerifyOrReturnError(cluster != nullptr, CHIP_IM_GLOBAL_STATUS(InteractionModel::Status::UnsupportedEndpoint));
    DiscoBallClusterState & cluster_state = cluster->GetClusterState();
    VerifyOrReturnError(cluster_state->IsInitialized(), CHIP_IM_GLOBAL_STATUS(InteractionModel::Status::Failure));

    ChipLogProgress(Zcl, "Handle disco ball attribute read");
    switch (aPath.mAttributeId)
    {
    case Clusters::DiscoBall::Attributes::Run::Id:
        ChipLogProgress(Zcl, "Read Run attribute");
        return aEncoder.Encode(cluster_state.GetRun());
    case Clusters::DiscoBall::Attributes::Rotate::Id:
        ChipLogProgress(Zcl, "Read Rotate attribute");
        return aEncoder.Encode(cluster_state.GetRotate());
    case Clusters::DiscoBall::Attributes::Speed::Id:
        ChipLogProgress(Zcl, "Read speed attribute");
        return aEncoder.Encode(cluster_state.GetSpeed());
    case Clusters::DiscoBall::Attributes::Axis::Id:
        ChipLogProgress(Zcl, "Read axis attribute");
        return aEncoder.Encode(cluster_state.GetAxis());
    case Clusters::DiscoBall::Attributes::WobbleSpeed::Id:
        ChipLogProgress(Zcl, "Read wobble speed attribute");
        return aEncoder.Encode(cluster_state.GetWobbleSpeed());
    case Clusters::DiscoBall::Attributes::Pattern::Id:
        ChipLogProgress(Zcl, "Read pattern attribute");
        // TODO: Encode the list
        return aEncoder.EncodeEmptyList();
    case Clusters::DiscoBall::Attributes::Name::Id:
        ChipLogProgress(Zcl, "Read name attribute");
        return aEncoder.Encode(cluster_state.GetName());
    case Clusters::DiscoBall::Attributes::WobbleSupport::Id:
        ChipLogProgress(Zcl, "Read wobble support attribute");
        return aEncoder.Encode(cluster_state.GetWobbleSupport().Raw());
    case Clusters::DiscoBall::Attributes::WobbleSetting::Id:
        ChipLogProgress(Zcl, "Read wobble setting attribute");
        return aEncoder.Encode(cluster_state.GetWobbleSetting().Raw());
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiscoBallServer::Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder)
{
    // TODO: Add support here.
    // TODO: Don't forget to check constraints and report attribute changes
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiscoBallServer::RegisterEndpoint(EndpointId endpoint_id, DiscoBallClusterState::NonVolatileStorageInterface & storage, DiscoBallDriverInterface & driver)
{
    VerifyOrReturnError(endpoint_id != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(FindEndpoint(endpoint_id) == nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    // Find a free registration slot.
    DiscoBallClusterLogic * endpoint_handler = nullptr;
    for (DiscoBallClusterLogic * candidate : mEndpoints)
    {
        if (endpoint_handler->GetEndpointId() == kInvalidEndpointId)
        {
            endpoint_handler = candidate;
            break;
        }
    }

    if (endpoint_handler == nullptr)
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    CHIP_ERROR err = endpoint_handler->Init(endpoint_id, storage, driver);
    if (err != CHIP_NO_ERROR)
    {
        endpoint_handler->Deinit();
        return err;
    }

    return CHIP_NO_ERROR;
}

void DiscoBallServer::UnregisterEndpoint(EndpointId endpoint_id)
{

}

DiscoBallClusterLogic * DiscoBallServer::FindEndpoint(EndpointId endpoint_id)
{
    for (DiscoBallClusterLogic * endpoint_handler : mEndpoints)
    {
        if (endpoint_handler-> endpoint_handler->GetEndpointId() == endpoint_id)
        {
            return endpoint_handler;
        }
    }

    return nullptr;
}


} // namespace app
} // namespace chip
void MatterDiscoBallPluginServerInitCallback()
{
    // TODO: Make application do the registration/init
    ChipLogProgress(Zcl, "Registering Disco Ball overrides");
    registerAttributeAccessOverride(&gAttributeAccess);
    chip::app::InteractionModelEngine::GetInstance()->RegisterCommandHandler(&gCommandhandler);
}
