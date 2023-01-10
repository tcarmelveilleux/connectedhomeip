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

#include "Esp32AppServer.h"
#include "CHIPDeviceManager.h"
#include <app/TestEventTriggerDelegate.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <app/clusters/ota-requestor/OTATestEventTriggerDelegate.h>
#include <app/server/Dnssd.h>
#include <app/server/Server.h>
#include <platform/ESP32/NetworkCommissioningDriver.h>
#include "AppTask.h"
#include <string.h>

using namespace chip;
using namespace chip::Credentials;
using namespace chip::DeviceLayer;

static constexpr char TAG[] = "ESP32Appserver";

namespace {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
app::Clusters::NetworkCommissioning::Instance
    sWiFiNetworkCommissioningInstance(0 /* Endpoint Id */, &(NetworkCommissioning::ESPWiFiDriver::GetInstance()));
#endif

#if 0
#if CONFIG_TEST_EVENT_TRIGGER_ENABLED
static uint8_t sTestEventTriggerEnableKey[TestEventTriggerDelegate::kEnableKeyLength] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                                                                          0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                                                                                          0xcc, 0xdd, 0xee, 0xff };
#endif
#endif

class MidiTestEventTriggerDelegate : public chip::TestEventTriggerDelegate
{
  public:
    bool DoesEnableKeyMatch(const ByteSpan & enableKey) const override
    {
        return enableKey.data_equal(ByteSpan{kEnableKey});
    }

    /**
     * Expectation is that the caller has already validated the enable key before calling this.
     * Handles the test event trigger based on `eventTrigger` provided.
     *
     * @param[in] eventTrigger Event trigger to handle.
     *
     * @return CHIP_NO_ERROR on success or another CHIP_ERROR on failure
     */
    virtual CHIP_ERROR HandleEventTrigger(uint64_t eventTrigger)
    {
        uint8_t midiData[4] = {};
        VerifyOrReturnError((eventTrigger & kMidiPrefixMask) == kMidiPrefix, CHIP_ERROR_INVALID_ARGUMENT);

        // If prefix matches, the lower 4 bytes are the MIDI command bytes
        midiData[0] = static_cast<uint8_t>((eventTrigger >> 24) & 0xFF);
        midiData[1] = static_cast<uint8_t>((eventTrigger >> 16) & 0xFF);
        midiData[2] = static_cast<uint8_t>((eventTrigger >> 8) & 0xFF);
        midiData[3] = static_cast<uint8_t>((eventTrigger >> 0) & 0xFF);

        GetAppTask().PostMidiEvent(ByteSpan{midiData});
        return CHIP_NO_ERROR;
    }

  private:
    static constexpr uint8_t kEnableKey[16] = { 'J', 'i', 'n', 'g', 'l', 'e', ' ', 'T', 'h', 'e', ' ', 'B', 'e', 'l', 'l', 's'};
    static constexpr uint64_t kMidiPrefix = 0xFFFF'FFFF'0000'0000;
    static constexpr uint64_t kMidiPrefixMask = 0xFFFF'FFFF'0000'0000;
};

} // namespace

void Esp32AppServer::Init(AppDelegate * sAppDelegate)
{
    // Init ZCL Data Model and CHIP App Server
    static chip::CommonCaseDeviceServerInitParams initParams;
#if CONFIG_TEST_EVENT_TRIGGER_ENABLED && CONFIG_ENABLE_OTA_REQUESTOR
    if (hex_string_to_binary(CONFIG_TEST_EVENT_TRIGGER_ENABLE_KEY, sTestEventTriggerEnableKey,
                             sizeof(sTestEventTriggerEnableKey)) == 0)
    {
        ESP_LOGE(TAG, "Failed to convert the EnableKey string to octstr type value");
        memset(sTestEventTriggerEnableKey, 0, sizeof(sTestEventTriggerEnableKey));
    }
    static OTATestEventTriggerDelegate testEventTriggerDelegate{ ByteSpan(sTestEventTriggerEnableKey) };
    initParams.testEventTriggerDelegate = &testEventTriggerDelegate;
#endif // CONFIG_TEST_EVENT_TRIGGER_ENABLED

    static MidiTestEventTriggerDelegate sMidiTestEventTriggerHandler;
    initParams.testEventTriggerDelegate = &sMidiTestEventTriggerHandler;

    (void) initParams.InitializeStaticResourcesBeforeServerInit();
    if (sAppDelegate != nullptr)
    {
        initParams.appDelegate = sAppDelegate;
    }
    chip::Server::GetInstance().Init(initParams);

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    sWiFiNetworkCommissioningInstance.Init();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    if (chip::DeviceLayer::ConnectivityMgr().IsThreadProvisioned() &&
        (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0))
    {
        ESP_LOGI(TAG, "Thread has been provisioned, publish the dns service now");
        chip::app::DnssdServer::Instance().StartServer();
    }
#endif
}
