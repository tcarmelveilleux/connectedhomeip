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

#include <app/clusters/on-off-server/on-off-server.h>
#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>

#include <assert.h>

#include <platform/silabs/platformAbstraction/SilabsPlatform.h>

#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

#include <lib/support/CodeUtils.h>

#include <platform/CHIPDeviceLayer.h>

#ifdef SL_CATALOG_SIMPLE_LED_LED1_PRESENT
#define LIGHT_LED 1
#else
#define LIGHT_LED 0
#endif

#define APP_FUNCTION_BUTTON 0
#define APP_LIGHT_SWITCH 1

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Silabs;

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
LEDWidget sLightLED;

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
            return aEncoder.Encode(static_cast<uint32_t>(0));

        case Globals::Attributes::ClusterRevision::Id:
            return aEncoder.Encode(static_cast<uint16_t>(1));
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

}

using namespace chip::TLV;
using namespace ::chip::DeviceLayer;

AppTask AppTask::sAppTask;

CHIP_ERROR AppTask::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    chip::DeviceLayer::Silabs::GetPlatform().SetButtonsCb(AppTask::ButtonEventHandler);

#ifdef DISPLAY_ENABLED
    GetLCD().Init((uint8_t *) "Lighting-App");
#endif

    err = BaseApplication::Init();
    if (err != CHIP_NO_ERROR)
    {
        SILABS_LOG("BaseApplication::Init() failed");
        appError(err);
    }

    err = LightMgr().Init();
    if (err != CHIP_NO_ERROR)
    {
        SILABS_LOG("LightMgr::Init() failed");
        appError(err);
    }

    LightMgr().SetCallbacks(ActionInitiated, ActionCompleted);

    sLightLED.Init(LIGHT_LED);
    sLightLED.Set(LightMgr().IsLightOn());

// Update the LCD with the Stored value. Show QR Code if not provisioned
#ifdef DISPLAY_ENABLED
    GetLCD().WriteDemoUI(LightMgr().IsLightOn());
#ifdef QR_CODE_ENABLED
#ifdef SL_WIFI
    if (!ConnectivityMgr().IsWiFiStationProvisioned())
#else
    if (!ConnectivityMgr().IsThreadProvisioned())
#endif /* !SL_WIFI */
    {
        GetLCD().ShowQRCode(true);
    }
#endif // QR_CODE_ENABLED
#endif

    chip::DeviceLayer::SystemLayer().ScheduleLambda([]()
    {
        EndpointId robotoEndpointId = 2;
        EndpointId parentEndpointId = 0;

        static std::unique_ptr<CommandHandlerInterface> sRobotoClusterInstance = GetRobotoClusterHandler(/*aEndpointId =*/robotoEndpointId);
        VerifyOrDie(sRobotoClusterInstance != nullptr);

        // Disable last fixed endpoint, which is used as a placeholder for all of the
        // supported clusters so that ZAP will generated the requisite code.
        emberAfEndpointEnableDisable(emberAfEndpointFromIndex(static_cast<uint16_t>(emberAfFixedEndpointCount() - 1)), false);

        uint16_t index = 0;

        CHIP_ERROR err = emberAfSetDynamicEndpoint(index, robotoEndpointId, &robotoEndpoint, Span<DataVersion>(gRobotoDataVersions), Span<const EmberAfDeviceType>(gRobotoDeviceTypes), parentEndpointId);
        VerifyOrDie(err == CHIP_NO_ERROR);
    });

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

void AppTask::LightActionEventHandler(AppEvent * aEvent)
{
    bool initiated = false;
    LightingManager::Action_t action;
    int32_t actor;
    CHIP_ERROR err = CHIP_NO_ERROR;

    if (aEvent->Type == AppEvent::kEventType_Light)
    {
        action = static_cast<LightingManager::Action_t>(aEvent->LightEvent.Action);
        actor  = aEvent->LightEvent.Actor;
    }
    else if (aEvent->Type == AppEvent::kEventType_Button)
    {
        action = (LightMgr().IsLightOn()) ? LightingManager::OFF_ACTION : LightingManager::ON_ACTION;
        actor  = AppEvent::kEventType_Button;
    }
    else
    {
        err = APP_ERROR_UNHANDLED_EVENT;
    }

    if (err == CHIP_NO_ERROR)
    {
        initiated = LightMgr().InitiateAction(actor, action);

        if (!initiated)
        {
            SILABS_LOG("Action is already in progress or active.");
        }
    }
}

void AppTask::ButtonEventHandler(uint8_t button, uint8_t btnAction)
{
    AppEvent button_event           = {};
    button_event.Type               = AppEvent::kEventType_Button;
    button_event.ButtonEvent.Action = btnAction;

    if (button == APP_LIGHT_SWITCH && btnAction == static_cast<uint8_t>(SilabsPlatform::ButtonAction::ButtonPressed))
    {
        button_event.Handler = LightActionEventHandler;
        AppTask::GetAppTask().PostEvent(&button_event);
    }
    else if (button == APP_FUNCTION_BUTTON)
    {
        button_event.Handler = BaseApplication::ButtonHandler;
        AppTask::GetAppTask().PostEvent(&button_event);
    }
}

void AppTask::ActionInitiated(LightingManager::Action_t aAction, int32_t aActor)
{
    // Action initiated, update the light led
    bool lightOn = aAction == LightingManager::ON_ACTION;
    SILABS_LOG("Turning light %s", (lightOn) ? "On" : "Off")

    sLightLED.Set(lightOn);

#ifdef DISPLAY_ENABLED
    sAppTask.GetLCD().WriteDemoUI(lightOn);
#endif

    if (aActor == AppEvent::kEventType_Button)
    {
        sAppTask.mSyncClusterToButtonAction = true;
    }
}

void AppTask::ActionCompleted(LightingManager::Action_t aAction)
{
    // action has been completed bon the light
    if (aAction == LightingManager::ON_ACTION)
    {
        SILABS_LOG("Light ON")
    }
    else if (aAction == LightingManager::OFF_ACTION)
    {
        SILABS_LOG("Light OFF")
    }

    if (sAppTask.mSyncClusterToButtonAction)
    {
        chip::DeviceLayer::PlatformMgr().ScheduleWork(UpdateClusterState, reinterpret_cast<intptr_t>(nullptr));
        sAppTask.mSyncClusterToButtonAction = false;
    }
}

void AppTask::PostLightActionRequest(int32_t aActor, LightingManager::Action_t aAction)
{
    AppEvent event;
    event.Type              = AppEvent::kEventType_Light;
    event.LightEvent.Actor  = aActor;
    event.LightEvent.Action = aAction;
    event.Handler           = LightActionEventHandler;
    PostEvent(&event);
}

void AppTask::UpdateClusterState(intptr_t context)
{
    uint8_t newValue = LightMgr().IsLightOn();

    // write the new on/off value
    Protocols::InteractionModel::Status status = OnOffServer::Instance().setOnOffValue(1, newValue, false);

    if (status != Protocols::InteractionModel::Status::Success)
    {
        SILABS_LOG("ERR: updating on/off %x", to_underlying(status));
    }
}
