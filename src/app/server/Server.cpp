/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <app/server/Server.h>

#include <access/examples/ExampleAccessControlDelegate.h>

#include <app/EventManagement.h>
#include <app/InteractionModelEngine.h>
#include <app/server/Dnssd.h>
#include <app/server/EchoHandler.h>
#include <app/util/DataModelHandler.h>

#if CONFIG_NETWORK_LAYER_BLE
#include <ble/BLEEndPoint.h>
#endif
#include <inet/IPAddress.h>
#include <inet/InetError.h>
#include <lib/core/CHIPPersistentStorageDelegate.h>
#include <lib/dnssd/Advertiser.h>
#include <lib/dnssd/ServiceNaming.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/PersistedCounter.h>
#include <lib/support/TestGroupData.h>
#include <lib/support/logging/CHIPLogging.h>
#include <messaging/ExchangeMgr.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/DeviceControlServer.h>
#include <platform/DeviceInfoProvider.h>
#include <platform/KeyValueStoreManager.h>
#include <protocols/secure_channel/CASEServer.h>
#include <protocols/secure_channel/MessageCounterManager.h>
#include <setup_payload/SetupPayload.h>
#include <sys/param.h>
#include <system/SystemPacketBuffer.h>
#include <system/TLVPacketBufferBackingStore.h>
#include <transport/SessionManager.h>
using namespace chip::DeviceLayer;

using chip::kMinValidFabricIndex;
using chip::RendezvousInformationFlag;
using chip::DeviceLayer::PersistedStorage::KeyValueStoreMgr;
using chip::Inet::IPAddressType;
#if CONFIG_NETWORK_LAYER_BLE
using chip::Transport::BleListenParameters;
#endif
using chip::Transport::PeerAddress;
using chip::Transport::UdpListenParameters;

namespace {

void StopEventLoop(intptr_t arg)
{
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().StopEventLoopTask();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Stopping event loop: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

class DeviceTypeResolver : public chip::Access::AccessControl::DeviceTypeResolver
{
public:
    bool IsDeviceTypeOnEndpoint(chip::DeviceTypeId deviceType, chip::EndpointId endpoint) override
    {
        return chip::app::IsDeviceTypeOnEndpoint(deviceType, endpoint);
    }
} sDeviceTypeResolver;

} // namespace

namespace chip {

Server Server::sServer;

#if CHIP_CONFIG_ENABLE_SERVER_IM_EVENT
#define CHIP_NUM_EVENT_LOGGING_BUFFERS 3
static uint8_t sInfoEventBuffer[CHIP_DEVICE_CONFIG_EVENT_LOGGING_INFO_BUFFER_SIZE];
static uint8_t sDebugEventBuffer[CHIP_DEVICE_CONFIG_EVENT_LOGGING_DEBUG_BUFFER_SIZE];
static uint8_t sCritEventBuffer[CHIP_DEVICE_CONFIG_EVENT_LOGGING_CRIT_BUFFER_SIZE];
static ::chip::PersistedCounter<chip::EventNumber> sGlobalEventIdCounter;
static ::chip::app::CircularEventBuffer sLoggingBuffer[CHIP_NUM_EVENT_LOGGING_BUFFERS];
#endif // CHIP_CONFIG_ENABLE_SERVER_IM_EVENT

CHIP_ERROR Server::Init(const ServerInitParams & initParams)
{
    ChipLogProgress(AppServer, "Server initializing...");

    CASESessionManagerConfig caseSessionManagerConfig;
    DeviceLayer::DeviceInfoProvider * deviceInfoprovider = nullptr;

    mOperationalServicePort        = initParams.operationalServicePort;
    mUserDirectedCommissioningPort = initParams.userDirectedCommissioningPort;
    mInterfaceId                   = initParams.interfaceId;

    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(initParams.persistentStorageDelegate != nullptr, err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(initParams.accessDelegate != nullptr, err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(initParams.aclStorage != nullptr, err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(initParams.groupDataProvider != nullptr, err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(initParams.operationalKeystore != nullptr, err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(initParams.opCertStore != nullptr, err = CHIP_ERROR_INVALID_ARGUMENT);

    // TODO(16969): Remove chip::Platform::MemoryInit() call from Server class, it belongs to outer code
    chip::Platform::MemoryInit();

    SuccessOrExit(err = mCommissioningWindowManager.Init(this));
    mCommissioningWindowManager.SetAppDelegate(initParams.appDelegate);

    // Initialize PersistentStorageDelegate-based storage
    mDeviceStorage            = initParams.persistentStorageDelegate;
    mSessionResumptionStorage = initParams.sessionResumptionStorage;
    mOperationalKeystore      = initParams.operationalKeystore;
    mOpCertStore              = initParams.opCertStore;

    mCertificateValidityPolicy = initParams.certificateValidityPolicy;

    // Set up attribute persistence before we try to bring up the data model
    // handler.
    SuccessOrExit(mAttributePersister.Init(mDeviceStorage));
    SetAttributePersistenceProvider(&mAttributePersister);

    {
        FabricTable::InitParams fabricTableInitParams;
        fabricTableInitParams.storage             = mDeviceStorage;
        fabricTableInitParams.operationalKeystore = mOperationalKeystore;
        fabricTableInitParams.opCertStore         = mOpCertStore;

        err = mFabrics.Init(fabricTableInitParams);
        SuccessOrExit(err);
    }

    SuccessOrExit(err = mAccessControl.Init(initParams.accessDelegate, sDeviceTypeResolver));
    Access::SetAccessControl(mAccessControl);

    mAclStorage = initParams.aclStorage;
    SuccessOrExit(err = mAclStorage->Init(*mDeviceStorage, mFabrics.begin(), mFabrics.end()));

    app::DnssdServer::Instance().SetFabricTable(&mFabrics);
    app::DnssdServer::Instance().SetCommissioningModeProvider(&mCommissioningWindowManager);

    mGroupsProvider = initParams.groupDataProvider;
    SetGroupDataProvider(mGroupsProvider);

    mTestEventTriggerDelegate = initParams.testEventTriggerDelegate;

    deviceInfoprovider = DeviceLayer::GetDeviceInfoProvider();
    if (deviceInfoprovider)
    {
        deviceInfoprovider->SetStorageDelegate(mDeviceStorage);
    }

    // This initializes clusters, so should come after lower level initialization.
    InitDataModelHandler(&mExchangeMgr);

    // Init transport before operations with secure session mgr.
    err = mTransports.Init(UdpListenParameters(DeviceLayer::UDPEndPointManager())
                               .SetAddressType(IPAddressType::kIPv6)
                               .SetListenPort(mOperationalServicePort)
                               .SetNativeParams(initParams.endpointNativeParams)

#if INET_CONFIG_ENABLE_IPV4
                               ,
                           UdpListenParameters(DeviceLayer::UDPEndPointManager())
                               .SetAddressType(IPAddressType::kIPv4)
                               .SetListenPort(mOperationalServicePort)
#endif
#if CONFIG_NETWORK_LAYER_BLE
                               ,
                           BleListenParameters(DeviceLayer::ConnectivityMgr().GetBleLayer())
#endif
    );

    err = mListener.Init(this);
    SuccessOrExit(err);
    mGroupsProvider->SetListener(&mListener);

#if CONFIG_NETWORK_LAYER_BLE
    mBleLayer = DeviceLayer::ConnectivityMgr().GetBleLayer();
#endif
    SuccessOrExit(err);

    err = mSessions.Init(&DeviceLayer::SystemLayer(), &mTransports, &mMessageCounterManager, mDeviceStorage, &GetFabricTable());
    SuccessOrExit(err);

    err = mFabricDelegate.Init(this);
    SuccessOrExit(err);
    mFabrics.AddFabricDelegate(&mFabricDelegate);

    err = mExchangeMgr.Init(&mSessions);
    SuccessOrExit(err);
    err = mMessageCounterManager.Init(&mExchangeMgr);
    SuccessOrExit(err);

    err = mUnsolicitedStatusHandler.Init(&mExchangeMgr);
    SuccessOrExit(err);

    err = chip::app::InteractionModelEngine::GetInstance()->Init(&mExchangeMgr, &GetFabricTable());
    SuccessOrExit(err);

    chip::Dnssd::Resolver::Instance().Init(DeviceLayer::UDPEndPointManager());

#if CHIP_CONFIG_ENABLE_SERVER_IM_EVENT
    // Initialize event logging subsystem
    err = sGlobalEventIdCounter.Init(mDeviceStorage, &DefaultStorageKeyAllocator::IMEventNumber,
                                     CHIP_DEVICE_CONFIG_EVENT_ID_COUNTER_EPOCH);
    SuccessOrExit(err);

    {
        ::chip::app::LogStorageResources logStorageResources[] = {
            { &sDebugEventBuffer[0], sizeof(sDebugEventBuffer), ::chip::app::PriorityLevel::Debug },
            { &sInfoEventBuffer[0], sizeof(sInfoEventBuffer), ::chip::app::PriorityLevel::Info },
            { &sCritEventBuffer[0], sizeof(sCritEventBuffer), ::chip::app::PriorityLevel::Critical }
        };

        chip::app::EventManagement::GetInstance().Init(&mExchangeMgr, CHIP_NUM_EVENT_LOGGING_BUFFERS, &sLoggingBuffer[0],
                                                       &logStorageResources[0], &sGlobalEventIdCounter);
    }
#endif // CHIP_CONFIG_ENABLE_SERVER_IM_EVENT

#if defined(CHIP_APP_USE_ECHO)
    err = InitEchoHandler(&mExchangeMgr);
    SuccessOrExit(err);
#endif

    app::DnssdServer::Instance().SetSecuredPort(mOperationalServicePort);
    app::DnssdServer::Instance().SetUnsecuredPort(mUserDirectedCommissioningPort);
    app::DnssdServer::Instance().SetInterfaceId(mInterfaceId);

    if (GetFabricTable().FabricCount() != 0)
    {
        // The device is already commissioned, proactively disable BLE advertisement.
        ChipLogProgress(AppServer, "Fabric already commissioned. Disabling BLE advertisement");
#if CONFIG_NETWORK_LAYER_BLE
        chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);
#endif
    }
    else
    {
#if CHIP_DEVICE_CONFIG_ENABLE_PAIRING_AUTOSTART
        SuccessOrExit(err = mCommissioningWindowManager.OpenBasicCommissioningWindow());
#endif
    }

    // TODO @bzbarsky-apple @cecille Move to examples
    // ESP32 and Mbed OS examples have a custom logic for enabling DNS-SD
#if !CHIP_DEVICE_LAYER_TARGET_ESP32 && !CHIP_DEVICE_LAYER_TARGET_MBED &&                                                           \
    (!CHIP_DEVICE_LAYER_TARGET_AMEBA || !CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE)
    // StartServer only enables commissioning mode if device has not been commissioned
    app::DnssdServer::Instance().StartServer();
#endif

    caseSessionManagerConfig = {
        .sessionInitParams =  {
            .sessionManager    = &mSessions,
            .sessionResumptionStorage = mSessionResumptionStorage,
            .certificateValidityPolicy = mCertificateValidityPolicy,
            .exchangeMgr       = &mExchangeMgr,
            .fabricTable       = &mFabrics,
            .clientPool        = &mCASEClientPool,
            .groupDataProvider = mGroupsProvider,
            .mrpLocalConfig    = GetLocalMRPConfig(),
        },
        .devicePool        = &mDevicePool,
    };

    err = mCASESessionManager.Init(&DeviceLayer::SystemLayer(), caseSessionManagerConfig);
    SuccessOrExit(err);

    err = mCASEServer.ListenForSessionEstablishment(&mExchangeMgr, &mSessions, &mFabrics, mSessionResumptionStorage,
                                                    mCertificateValidityPolicy, mGroupsProvider);
    SuccessOrExit(err);

    // This code is necessary to restart listening to existing groups after a reboot
    // Each manufacturer needs to validate that they can rejoin groups by placing this code at the appropriate location for them
    //
    // Thread LWIP devices using dedicated Inet endpoint implementations are excluded because they call this function from:
    // src/platform/OpenThread/GenericThreadStackManagerImpl_OpenThread_LwIP.cpp
#if !CHIP_SYSTEM_CONFIG_USE_OPEN_THREAD_ENDPOINT
    RejoinExistingMulticastGroups();
#endif // !CHIP_SYSTEM_CONFIG_USE_OPEN_THREAD_ENDPOINT

    // Handle deferred clean-up of a previously armed fail-safe that occurred during FabricTable commit.
    // This is done at the very end since at the earlier time above when FabricTable::Init() is called,
    // the delegates could not have been registered, and the other systems were not initialized. By now,
    // everything is initialized, so we can do a deferred clean-up.
    {
        FabricIndex fabricIndexDeletedOnInit = GetFabricTable().GetDeletedFabricFromCommitMarker();
        if (fabricIndexDeletedOnInit != kUndefinedFabricIndex)
        {
            ChipLogError(AppServer, "FabricIndex 0x%x deleted due to restart while fail-safed. Processing a clean-up!",
                         static_cast<unsigned>(fabricIndexDeletedOnInit));

            // Always pretend it was an add, since being in the middle of an update currently breaks
            // the validity of the fabric table. This is expected to be extremely infrequent, so
            // this "harsher" than usual clean-up is more likely to get us in a valid state for whatever
            // remains.
            const bool addNocCalled    = true;
            const bool updateNocCalled = false;
            GetFailSafeContext().ScheduleFailSafeCleanup(fabricIndexDeletedOnInit, addNocCalled, updateNocCalled);

            // Schedule clearing of the commit marker to only occur after we have processed all fail-safe clean-up.
            // Because Matter runs a single event loop for all scheduled work, it will occur after the above has
            // taken place. If a reset occurs before we have cleaned everything up, the next boot will still
            // see the commit marker.
            PlatformMgr().ScheduleWork(
                [](intptr_t arg) {
                    Server * server = reinterpret_cast<Server *>(arg);
                    VerifyOrReturn(server != nullptr);

                    server->GetFabricTable().ClearCommitMarker();
                    ChipLogProgress(AppServer, "Cleared FabricTable pending commit marker");
                },
                reinterpret_cast<intptr_t>(this));
        }
    }

    PlatformMgr().HandleServerStarted();

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "ERROR setting up transport: %" CHIP_ERROR_FORMAT, err.Format());
    }
    else
    {
        // NOTE: this log is scraped by the test harness.
        ChipLogProgress(AppServer, "Server Listening...");
    }
    return err;
}

void Server::RejoinExistingMulticastGroups()
{
    ChipLogProgress(AppServer, "Joining Multicast groups");
    CHIP_ERROR err = CHIP_NO_ERROR;
    for (const FabricInfo & fabric : mFabrics)
    {
        Credentials::GroupDataProvider::GroupInfo groupInfo;

        auto * iterator = mGroupsProvider->IterateGroupInfo(fabric.GetFabricIndex());
        if (iterator)
        {
            // GroupDataProvider was able to allocate rescources for an iterator
            while (iterator->Next(groupInfo))
            {
                err = mTransports.MulticastGroupJoinLeave(
                    Transport::PeerAddress::Multicast(fabric.GetFabricId(), groupInfo.group_id), true);
                if (err != CHIP_NO_ERROR)
                {
                    ChipLogError(AppServer, "Error when trying to join Group %u of fabric index %u : %" CHIP_ERROR_FORMAT,
                                 groupInfo.group_id, fabric.GetFabricIndex(), err.Format());

                    // We assume the failure is caused by a network issue or a lack of rescources; neither of which will be solved
                    // before the next join. Exit the loop to save rescources.
                    iterator->Release();
                    return;
                }
            }

            iterator->Release();
        }
    }
}

void Server::DispatchShutDownAndStopEventLoop()
{
    PlatformMgr().ScheduleWork([](intptr_t) { PlatformMgr().HandleServerShuttingDown(); });
    PlatformMgr().ScheduleWork(StopEventLoop);
}

void Server::ScheduleFactoryReset()
{
    PlatformMgr().ScheduleWork([](intptr_t) {
        // Delete all fabrics and emit Leave event.
        GetInstance().GetFabricTable().DeleteAllFabrics();
        PlatformMgr().HandleServerShuttingDown();
        ConfigurationMgr().InitiateFactoryReset();
    });
}

void Server::Shutdown()
{
    mCASEServer.Shutdown();
    mCASESessionManager.Shutdown();
    app::DnssdServer::Instance().SetCommissioningModeProvider(nullptr);
    chip::Dnssd::ServiceAdvertiser::Instance().Shutdown();

    chip::Dnssd::Resolver::Instance().Shutdown();
    chip::app::InteractionModelEngine::GetInstance()->Shutdown();
    mMessageCounterManager.Shutdown();
    mExchangeMgr.Shutdown();
    mSessions.Shutdown();
    mTransports.Close();
    mAccessControl.Finish();
    Credentials::SetGroupDataProvider(nullptr);
    mAttributePersister.Shutdown();
    mCommissioningWindowManager.Shutdown();
    // TODO(16969): Remove chip::Platform::MemoryInit() call from Server class, it belongs to outer code
    chip::Platform::MemoryShutdown();
}

#if CHIP_DEVICE_CONFIG_ENABLE_COMMISSIONER_DISCOVERY_CLIENT
// NOTE: UDC client is located in Server.cpp because it really only makes sense
// to send UDC from a Matter device. The UDC message payload needs to include the device's
// randomly generated service name.
CHIP_ERROR Server::SendUserDirectedCommissioningRequest(chip::Transport::PeerAddress commissioner)
{
    ChipLogDetail(AppServer, "SendUserDirectedCommissioningRequest2");

    CHIP_ERROR err;
    char nameBuffer[chip::Dnssd::Commission::kInstanceNameMaxLength + 1];
    err = app::DnssdServer::Instance().GetCommissionableInstanceName(nameBuffer, sizeof(nameBuffer));
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Failed to get mdns instance name error: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }
    ChipLogDetail(AppServer, "instanceName=%s", nameBuffer);

    chip::System::PacketBufferHandle payloadBuf = chip::MessagePacketBuffer::NewWithData(nameBuffer, strlen(nameBuffer));
    if (payloadBuf.IsNull())
    {
        ChipLogError(AppServer, "Unable to allocate packet buffer\n");
        return CHIP_ERROR_NO_MEMORY;
    }

    err = gUDCClient.SendUDCMessage(&mTransports, std::move(payloadBuf), commissioner);
    if (err == CHIP_NO_ERROR)
    {
        ChipLogDetail(AppServer, "Send UDC request success");
    }
    else
    {
        ChipLogError(AppServer, "Send UDC request failed, err: %" CHIP_ERROR_FORMAT, err.Format());
    }
    return err;
}
#endif // CHIP_DEVICE_CONFIG_ENABLE_COMMISSIONER_DISCOVERY_CLIENT

/* ================ START OF STORAGE API AUDIT (#) ==================== */
#ifdef NL_TEST_ASSERT
#undef NL_TEST_ASSERT
#endif

#define NL_TEST_ASSERT(inSuite, inCondition)                              \
    do {                                                                  \
        (inSuite)->performedAssertions += 1;                              \
                                                                          \
        if (!(inCondition))                                               \
        {                                                                 \
            ChipLogError(Automation, "%s:%u: assertion failed: \"%s\"\n", \
                         __FILE__, __LINE__, #inCondition);               \
            (inSuite)->failedAssertions += 1;                             \
            (inSuite)->flagError = true;                                  \
        }                                                                 \
    } while (0)

// The following test is a copy of `src/lib/support/tests/TestTestPersistentStorageDelegate.cpp` 's
// `TestBasicApi()` test. It has to be copied since we currently are not setup to
// run on-device unit tests at large on all embedded platforms part of the SDK.
CHIP_ERROR AuditPersistentStorageDelegateImplementation(PersistentStorageDelegate & storage)
{
    struct fakeTestSuite
    {
        int performedAssertions = 0;
        int failedAssertions = 0;
        bool flagError = false;
    } theSuite;
    auto * inSuite = &theSuite;

    // Start fresh.
    (void)storage.SyncDeleteKeyValue("roboto");
    (void)storage.SyncDeleteKeyValue("key2");
    (void)storage.SyncDeleteKeyValue("key3");
    (void)storage.SyncDeleteKeyValue("key4");
    (void)storage.SyncDeleteKeyValue("keyDOES_NOT_EXIST");

    // ========== Start of actual audit =========

    uint8_t buf[16];
    uint16_t size = sizeof(buf);

    // Key not there
    CHIP_ERROR err;
    memset(&buf[0], 0, sizeof(buf));
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("roboto", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    NL_TEST_ASSERT(inSuite, size == sizeof(buf));

#if 0
    NL_TEST_ASSERT(inSuite, storage.GetNumKeys() == 0);
#endif

    err = storage.SyncDeleteKeyValue("roboto");
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    // Add basic key, read it back, erase it
    const char * kStringValue1 = "abcd";
    err                        = storage.SyncSetKeyValue("roboto", kStringValue1, static_cast<uint16_t>(strlen(kStringValue1)));
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    memset(&buf[0], 0, sizeof(buf));
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("roboto", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue1));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], kStringValue1, strlen(kStringValue1)));

    err = storage.SyncDeleteKeyValue("roboto");
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    memset(&buf[0], 0, sizeof(buf));
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("roboto", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    NL_TEST_ASSERT(inSuite, size == sizeof(buf));

    // Validate adding 2 different keys
    const char * kStringValue2 = "0123abcd";
    const char * kStringValue3 = "cdef89";
    err                        = storage.SyncSetKeyValue("key2", kStringValue2, static_cast<uint16_t>(strlen(kStringValue2)));
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    err = storage.SyncSetKeyValue("key3", kStringValue3, static_cast<uint16_t>(strlen(kStringValue3)));
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

#if 0
    NL_TEST_ASSERT(inSuite, storage.GetNumKeys() == 2);
    auto keys = storage.GetKeys();
    std::array<std::string, 2> kExpectedKeys{ "key2", "key3" };
    NL_TEST_ASSERT(inSuite, SetMatches(keys, kExpectedKeys) == true);
#endif

    // Read them back

    memset(&buf[0], 0, sizeof(buf));
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("key2", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue2));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], kStringValue2, strlen(kStringValue2)));

    memset(&buf[0], 0, sizeof(buf));
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("key3", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue3));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], kStringValue3, strlen(kStringValue3)));

    memset(&buf[0], 0, sizeof(buf));
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("key2", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue2));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], kStringValue2, strlen(kStringValue2)));

    // Pre-clear buffer to make sure next operations don't change contents
    uint8_t all_zeroes[sizeof(buf)];
    memset(&buf[0], 0, sizeof(buf));
    memset(&all_zeroes[0], 0, sizeof(all_zeroes));

    // Read in too small a buffer: no data read, but correct size given
    memset(&buf[0], 0, sizeof(buf));
    size = static_cast<uint16_t>(strlen(kStringValue2) - 1);
    err  = storage.SyncGetKeyValue("key2", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_BUFFER_TOO_SMALL);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue2));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], sizeof(buf)));

    // Read in too small a buffer, which is nullptr and size == 0: check correct size given
    size = 0;
    err  = storage.SyncGetKeyValue("key2", nullptr, size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_BUFFER_TOO_SMALL);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue2));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], sizeof(buf)));

    // Read in too small a buffer, which is nullptr and size != 0: error
    size = static_cast<uint16_t>(strlen(kStringValue2) - 1);
    err  = storage.SyncGetKeyValue("key2", nullptr, size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_INVALID_ARGUMENT);
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], sizeof(buf)));

    // Read in zero size buffer, which is also nullptr (i.e. just try to find if key exists without
    // using a buffer).
    size = 0;
    err  = storage.SyncGetKeyValue("key2", nullptr, size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_BUFFER_TOO_SMALL);
    NL_TEST_ASSERT(inSuite, size == strlen(kStringValue2));
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], sizeof(buf)));

    // When key not found, size is not touched.
    size = sizeof(buf);
    err  = storage.SyncGetKeyValue("keyDOES_NOT_EXIST", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    NL_TEST_ASSERT(inSuite, sizeof(buf) == size);
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], sizeof(buf)));

    size = 0;
    err  = storage.SyncGetKeyValue("keyDOES_NOT_EXIST", nullptr, size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    NL_TEST_ASSERT(inSuite, 0 == size);
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], sizeof(buf)));

    // Even when key not found, cannot pass nullptr with size != 0.
    size = static_cast<uint16_t>(sizeof(buf));
    err  = storage.SyncGetKeyValue("keyDOES_NOT_EXIST", nullptr, size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_INVALID_ARGUMENT);
    NL_TEST_ASSERT(inSuite, sizeof(buf) == size);
    NL_TEST_ASSERT(inSuite, 0 == memcmp(&buf[0], &all_zeroes[0], size));

    // Attempt an empty key write with either nullptr or zero size works
    err = storage.SyncSetKeyValue("key2", kStringValue2, 0);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    size = 0;
    err  = storage.SyncGetKeyValue("key2", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite, size == 0);

    err = storage.SyncSetKeyValue("key2", nullptr, 0);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    size = 0;
    err  = storage.SyncGetKeyValue("key2", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    NL_TEST_ASSERT(inSuite, size == 0);

    // Failure to set key if buffer is nullptr and size != 0
    size = 10;
    err  = storage.SyncSetKeyValue("key4", nullptr, size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_INVALID_ARGUMENT);

    // Can delete empty key
    err = storage.SyncDeleteKeyValue("key2");
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    size = static_cast<uint16_t>(sizeof(buf));
    err  = storage.SyncGetKeyValue("key2", &buf[0], size);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    NL_TEST_ASSERT(inSuite, size == sizeof(buf));

    if (inSuite->flagError)
    {
        ChipLogError(Automation, "==== STORAGE AUDIT FAILED: %d/%d failed assertions ====", inSuite->failedAssertions, inSuite->performedAssertions);
        return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }

    ChipLogError(Automation, "==== STORAGE AUDIT SUCCESS ====");
    return CHIP_NO_ERROR;
}

/* ================ END OF STORAGE API AUDIT (#) ==================== */

} // namespace chip
