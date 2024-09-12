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

#include <memory>
#include <utility>

#include "LightingAppCommandDelegate.h"
#include "LightingManager.h"
#include <AppMain.h>

#include <app/util/af-types.h>
#include <app/util/attribute-storage.h>
#include <app/util/endpoint-config-api.h>
#include <app/util/util.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/CommandHandlerInterface.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterfaceRegistry.h>
#include <app/data-model/WrappedStructEncoder.h>
#include <app/ConcreteAttributePath.h>
#include <app/server/Server.h>
#include <lib/support/logging/CHIPLogging.h>

#include <string>

#if defined(CHIP_IMGUI_ENABLED) && CHIP_IMGUI_ENABLED
#include <imgui_ui/ui.h>
#include <imgui_ui/windows/light.h>
#include <imgui_ui/windows/occupancy_sensing.h>
#include <imgui_ui/windows/qrcode.h>

#endif

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;


namespace chip {
namespace app {
namespace Clusters {

namespace detail {

class StructDecodeIterator
{
public:
    // may return a context tag, a CHIP_ERROR (end iteration)
    using EntryElement = std::variant<uint8_t, CHIP_ERROR>;

    StructDecodeIterator(TLV::TLVReader & reader) : mReader(reader) {}

    // Iterate through structure elements. Returns one of:
    //   - uint8_t CONTEXT TAG (keep iterating)
    //   - CHIP_ERROR (including CHIP_NO_ERROR) which should be a final
    //     return value (stop iterating)
    EntryElement Next()
    {
        if (!mEntered)
        {
            VerifyOrReturnError(TLV::kTLVType_Structure == mReader.GetType(), CHIP_ERROR_WRONG_TLV_TYPE);
            ReturnErrorOnFailure(mReader.EnterContainer(mOuter));
            mEntered = true;
        }

        while (true)
        {
            CHIP_ERROR err = mReader.Next();
            if (err != CHIP_NO_ERROR)
            {
                VerifyOrReturnError(err == CHIP_ERROR_END_OF_TLV, err);
                break;
            }

            const TLV::Tag tag = mReader.GetTag();
            if (!TLV::IsContextTag(tag))
            {
                continue;
            }

            // we know context tags are 8-bit
            return static_cast<uint8_t>(TLV::TagNumFromTag(tag));
        }

        return mReader.ExitContainer(mOuter);
    }

private:
    bool mEntered = false;
    TLV::TLVType mOuter;
    TLV::TLVReader & mReader;
};

} // namespace detail

namespace Roboto {

static constexpr ClusterId Id = 0x1234FC09;

namespace Commands {

namespace RobotoResponse {

static constexpr CommandId Id = 0x12340002;

enum class Fields : uint8_t
{
    kReplyBlob     = 0,
};

struct Type
{
public:
    static constexpr CommandId GetCommandId() { return Commands::RobotoResponse::Id; }
    static constexpr ClusterId GetClusterId() { return Clusters::Roboto::Id; }

    ByteSpan replyBlob;

    CHIP_ERROR Encode(TLV::TLVWriter & writer, TLV::Tag tag) const
    {
        DataModel::WrappedStructEncoder encoder{ writer, tag };
        encoder.Encode(to_underlying(Fields::kReplyBlob), replyBlob);
        return encoder.Finalize();
    }

    using ResponseType = DataModel::NullObjectType;

    static constexpr bool MustUseTimedInvoke() { return false; }
};

struct DecodableType
{
public:
    static constexpr CommandId GetCommandId() { return Commands::RobotoResponse::Id; }
    static constexpr ClusterId GetClusterId() { return Clusters::Roboto::Id; }

    ByteSpan replyBlob;
    CHIP_ERROR Decode(TLV::TLVReader & reader)
    {
        detail::StructDecodeIterator __iterator(reader);
        while (true)
        {
            auto __element = __iterator.Next();
            if (std::holds_alternative<CHIP_ERROR>(__element))
            {
                return std::get<CHIP_ERROR>(__element);
            }

            CHIP_ERROR err              = CHIP_NO_ERROR;
            const uint8_t __context_tag = std::get<uint8_t>(__element);

            if (__context_tag == to_underlying(Fields::kReplyBlob))
            {
                err = DataModel::Decode(reader, replyBlob);
            }

            ReturnErrorOnFailure(err);
        }
    }
};

} // namespace RobotoResponse

namespace RobotoRequest {

static constexpr CommandId Id = 0x12340002;

enum class Fields : uint8_t
{
    kRequestBlob = 0,
};

struct Type
{
public:
    static constexpr CommandId GetCommandId() { return Commands::RobotoRequest::Id; }
    static constexpr ClusterId GetClusterId() { return Clusters::Roboto::Id; }

    ByteSpan requestBlob;

    CHIP_ERROR Encode(TLV::TLVWriter & writer, TLV::Tag tag) const
    {
        DataModel::WrappedStructEncoder encoder{ writer, tag };
        encoder.Encode(to_underlying(Fields::kRequestBlob), requestBlob);
        return encoder.Finalize();
    }

    using ResponseType = Clusters::Roboto::Commands::RobotoResponse::DecodableType;

    static constexpr bool MustUseTimedInvoke() { return false; }
};

struct DecodableType
{
public:
    static constexpr CommandId GetCommandId() { return Commands::RobotoRequest::Id; }
    static constexpr ClusterId GetClusterId() { return Clusters::Roboto::Id; }

    ByteSpan requestBlob;

    CHIP_ERROR Decode(TLV::TLVReader & reader)
    {
        detail::StructDecodeIterator __iterator(reader);
        while (true)
        {
            auto __element = __iterator.Next();
            if (std::holds_alternative<CHIP_ERROR>(__element))
            {
                return std::get<CHIP_ERROR>(__element);
            }

            CHIP_ERROR err              = CHIP_NO_ERROR;
            const uint8_t __context_tag = std::get<uint8_t>(__element);

            if (__context_tag == to_underlying(Fields::kRequestBlob))
            {
                err = DataModel::Decode(reader, requestBlob);
            }

            ReturnErrorOnFailure(err);
        }
    }
};

} // namespace RobotoRequest
} // namespace Commands
} // namespace Roboto
} // namespace Clusters
} // namespace app
} // namespace chip


namespace {

constexpr char kChipEventFifoPathPrefix[] = "/tmp/chip_lighting_fifo_";
NamedPipeCommands sChipNamedPipeCommands;
LightingAppCommandDelegate sLightingAppCommandDelegate;


class RobotoClusterInstance : public chip::app::CommandHandlerInterface, public chip::app::AttributeAccessInterface
{
  public:
    RobotoClusterInstance(EndpointId aEndpointId) : chip::app::CommandHandlerInterface(MakeOptional(aEndpointId), Clusters::Roboto::Id), chip::app::AttributeAccessInterface(MakeOptional(aEndpointId), Clusters::Roboto::Id) {}

    CHIP_ERROR EnumerateAcceptedCommands(const ConcreteClusterPath & cluster, CommandIdCallback callback, void * context) override
    {
        callback(Clusters::Roboto::Commands::RobotoRequest::Id, context);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR EnumerateGeneratedCommands(const ConcreteClusterPath & cluster, CommandIdCallback callback, void * context) override
    {
        callback(Clusters::Roboto::Commands::RobotoResponse::Id, context);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override
    {
        switch (aPath.mAttributeId)
        {
        case Globals::Attributes::FeatureMap::Id:
            return aEncoder.Encode(0);

        case Globals::Attributes::ClusterRevision::Id:
            return aEncoder.Encode(1);
        }

        return CHIP_NO_ERROR;
    }

    void InvokeCommand(HandlerContext & ctxt) override
    {
          switch (ctxt.mRequestPath.mCommandId)
          {
          case Clusters::Roboto::Commands::RobotoRequest::Id:
              HandleCommand<Clusters::Roboto::Commands::RobotoRequest::DecodableType>(
                  ctxt, [this](HandlerContext & ctx, const auto & req) { HandleRobotoRequest(ctx, req); });
              return;
          }
    }
  protected:
    void HandleRobotoRequest(HandlerContext & ctx, const Clusters::Roboto::Commands::RobotoRequest::DecodableType & req)
    {
        Clusters::Roboto::Commands::RobotoResponse::Type response;

        // Simple echo: ropy request to response.
        response.replyBlob = req.requestBlob;

        ctx.mCommandHandler.AddResponse(ctx.mRequestPath, response);
    }
};

std::unique_ptr<CommandHandlerInterface> GetRobotoClusterHandler(EndpointId endpointId)
{
    auto instance = std::make_unique<RobotoClusterInstance>(endpointId);

    if (CommandHandlerInterfaceRegistry::Instance().RegisterCommandHandler(instance.get()) != CHIP_NO_ERROR)
    {
        return nullptr;
    }

    if (!AttributeAccessInterfaceRegistry::Instance().Register(instance.get()))
    {
        CommandHandlerInterfaceRegistry::Instance().UnregisterCommandHandler(instance.get());
        return nullptr;
    }

    return instance;
}

// ---------------------------------------------------------------------------
//
// ROBOTO ENDPOINT: contains the following clusters:
//   - Roboto
//   - Descriptor

const int kDescriptorAttributeArraySize = 254;

// Declare Roboto cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(robotoAttrs)
    DECLARE_DYNAMIC_ATTRIBUTE(Globals::Attributes::FeatureMap::Id, BITMAP32, 4, 0),    /* FeatureMap */
    //DECLARE_DYNAMIC_ATTRIBUTE(Globals::Attributes::ClusterRevision::Id, INT16U, 2, 0), /* ClusterRevision */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Descriptor cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(descriptorAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::DeviceTypeList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* device list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ServerList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* server list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ClientList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* client list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::PartsList::Id, ARRAY, kDescriptorAttributeArraySize, 0),  /* parts list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::FeatureMap::Id, BITMAP32, 4, 0),                          /* FeatureMap */
    //DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ClusterRevision::Id, INT16U, 2, 0),                       /* ClusterRevision */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Cluster List for Roboto endpoint
constexpr CommandId robotoIncomingCommands[] = {
    app::Clusters::Roboto::Commands::RobotoRequest::Id,
    kInvalidCommandId,
};

constexpr CommandId robotoOutgoingCommands[] = {
    app::Clusters::Roboto::Commands::RobotoResponse::Id,
    kInvalidCommandId,
};

DECLARE_DYNAMIC_CLUSTER_LIST_BEGIN(robotoEndpointClusters)
DECLARE_DYNAMIC_CLUSTER(Roboto::Id, robotoAttrs, ZAP_CLUSTER_MASK(SERVER), robotoIncomingCommands, robotoOutgoingCommands),
DECLARE_DYNAMIC_CLUSTER(Descriptor::Id, descriptorAttrs, ZAP_CLUSTER_MASK(SERVER), nullptr, nullptr),
DECLARE_DYNAMIC_CLUSTER_LIST_END;

// Declare Bridged Light endpoint
DECLARE_DYNAMIC_ENDPOINT(robotoEndpoint, robotoEndpointClusters);
DataVersion gRobotoDataVersions[ArraySize(robotoEndpointClusters)];

#define DEVICE_TYPE_ROBOTO (0x12340055)
#define DEVICE_VERSION_DEFAULT (1)

const EmberAfDeviceType gRobotoDeviceTypes[] = { { DEVICE_TYPE_ROBOTO, DEVICE_VERSION_DEFAULT } };

} // namespace

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    if (attributePath.mClusterId == OnOff::Id && attributePath.mAttributeId == OnOff::Attributes::OnOff::Id)
    {
        LightingMgr().InitiateAction(*value ? LightingManager::ON_ACTION : LightingManager::OFF_ACTION);
    }
}

/** @brief OnOff Cluster Init
 *
 * This function is called when a specific cluster is initialized. It gives the
 * application an opportunity to take care of cluster initialization procedures.
 * It is called exactly once for each endpoint where cluster is present.
 *
 * @param endpoint   Ver.: always
 *
 * TODO Issue #3841
 * emberAfOnOffClusterInitCallback happens before the stack initialize the cluster
 * attributes to the default value.
 * The logic here expects something similar to the deprecated Plugins callback
 * emberAfPluginOnOffClusterServerPostInitCallback.
 *
 */
void emberAfOnOffClusterInitCallback(EndpointId endpoint)
{
    // TODO: implement any additional Cluster Server init actions
}

void ApplicationInit()
{
    std::string path = kChipEventFifoPathPrefix + std::to_string(getpid());

    if (sChipNamedPipeCommands.Start(path, &sLightingAppCommandDelegate) != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to start CHIP NamedPipeCommands");
        sChipNamedPipeCommands.Stop();
    }

    constexpr EndpointId kRobotoEndpointId = 2;
    constexpr EndpointId kParentEndpointId = 0;

    static std::unique_ptr<CommandHandlerInterface> sRobotoClusterInstance = GetRobotoClusterHandler(/*aEndpointId =*/kRobotoEndpointId);
    VerifyOrDie(sRobotoClusterInstance != nullptr);

    constexpr uint16_t kFirstDynamicEndpointIndex = 0;
    CHIP_ERROR err = emberAfSetDynamicEndpoint(kFirstDynamicEndpointIndex, kRobotoEndpointId, &robotoEndpoint, Span<DataVersion>(gRobotoDataVersions), Span<const EmberAfDeviceType>(gRobotoDeviceTypes), kParentEndpointId);
    VerifyOrDie(err == CHIP_NO_ERROR);
}

void ApplicationShutdown()
{
    if (sChipNamedPipeCommands.Stop() != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to stop CHIP NamedPipeCommands");
    }
}

extern "C" int main(int argc, char * argv[])
{
    if (ChipLinuxAppInit(argc, argv) != 0)
    {
        return -1;
    }

    CHIP_ERROR err = LightingMgr().Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Failed to initialize lighting manager: %" CHIP_ERROR_FORMAT, err.Format());
        chip::DeviceLayer::PlatformMgr().Shutdown();
        return -1;
    }

#if defined(CHIP_IMGUI_ENABLED) && CHIP_IMGUI_ENABLED
    example::Ui::ImguiUi ui;

    ui.AddWindow(std::make_unique<example::Ui::Windows::QRCode>());
    ui.AddWindow(std::make_unique<example::Ui::Windows::OccupancySensing>(chip::EndpointId(1), "Occupancy"));
    ui.AddWindow(std::make_unique<example::Ui::Windows::Light>(chip::EndpointId(1)));

    ChipLinuxAppMainLoop(&ui);
#else
    ChipLinuxAppMainLoop();
#endif

    return 0;
}
