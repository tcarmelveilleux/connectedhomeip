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

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "main/gdm_wifi_base_rpc.rpc.pb.h"
#include "main/ipv6_only_rpc.rpc.pb.h"
#include "nvs_flash.h"
#include "pw_containers/flat_map.h"
#include "pw_log/log.h"
#include "pw_rpc/echo_service_nanopb.h"
#include "pw_rpc/server.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_sys_io/sys_io.h"
#include "pw_sys_io_esp32/init.h"
#include <functional>

#include "RpcService.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

const char * TAG = "chip-pigweed-app";

#define PW_ESP_TRY(expr) _PW_ESP_TRY(_PW_TRY_UNIQUE(__LINE__), expr)

#define _PW_ESP_TRY(result, expr)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        if (auto result = ::pw::internal::ConvertToStatus(expr); !result.ok())                                                     \
        {                                                                                                                          \
            return result;                                                                                                         \
        }                                                                                                                          \
    } while (0)

namespace pw::internal {

constexpr Status ConvertToStatus(esp_err_t err)
{
    switch (err)
    {
    case ESP_OK:
        return pw::OkStatus();
    case ESP_ERR_WIFI_NOT_INIT:
        return pw::Status::FailedPrecondition();
    case ESP_ERR_INVALID_ARG:
        return pw::Status::InvalidArgument();
    case ESP_ERR_ESP_NETIF_INVALID_PARAMS:
        return pw::Status::InvalidArgument();
    case ESP_ERR_WIFI_IF:
        return pw::Status::NotFound();
    case ESP_ERR_WIFI_NOT_CONNECT:
        return pw::Status::FailedPrecondition();
    case ESP_ERR_WIFI_NOT_STARTED:
        return pw::Status::FailedPrecondition();
    case ESP_ERR_WIFI_CONN:
        return pw::Status::Internal();
    case ESP_FAIL:
        return pw::Status::Internal();
    default:
        return pw::Status::Unknown();
    }
}

} // namespace pw::internal

namespace {

constexpr pw::containers::FlatMap<uint8_t, uint32_t, 73> kChannelToFreqMap({ {
    { 1, 2412 },   { 2, 2417 },   { 3, 2422 },   { 4, 2427 },   { 5, 2432 },   { 6, 2437 },   { 7, 2442 },   { 8, 2447 },
    { 9, 2452 },   { 10, 2457 },  { 11, 2462 },  { 12, 2467 },  { 13, 2472 },  { 14, 2484 },  { 32, 5160 },  { 34, 5170 },
    { 36, 5180 },  { 38, 5190 },  { 40, 5200 },  { 42, 5210 },  { 44, 5220 },  { 46, 5230 },  { 48, 5240 },  { 50, 5250 },
    { 52, 5260 },  { 54, 5270 },  { 56, 5280 },  { 58, 5290 },  { 60, 5300 },  { 62, 5310 },  { 64, 5320 },  { 68, 5340 },
    { 96, 5480 },  { 100, 5500 }, { 102, 5510 }, { 104, 5520 }, { 106, 5530 }, { 108, 5540 }, { 110, 5550 }, { 112, 5560 },
    { 114, 5570 }, { 116, 5580 }, { 118, 5590 }, { 120, 5600 }, { 122, 5610 }, { 124, 5620 }, { 126, 5630 }, { 128, 5640 },
    { 132, 5660 }, { 134, 5670 }, { 136, 5680 }, { 138, 5690 }, { 140, 5700 }, { 142, 5710 }, { 144, 5720 }, { 149, 5745 },
    { 151, 5755 }, { 153, 5765 }, { 155, 5775 }, { 157, 5785 }, { 159, 5795 }, { 161, 5805 }, { 165, 5825 }, { 169, 5845 },
    { 173, 5865 }, { 183, 4915 }, { 184, 4920 }, { 185, 4925 }, { 187, 4935 }, { 188, 4940 }, { 189, 4945 }, { 192, 4960 },
    { 196, 4980 },
} });

constexpr size_t kUdpBufferSize = 512;

constexpr uint8_t kWiFiConnectRetryMax = 5;

/* The event group allows multiple bits for each event, but we only care about
 * whether the STA initialization was successful
 */
constexpr uint8_t WIFI_STA_UP_BIT = BIT0;

/* The event group allows multiple bits for each event, but we only care about 3 events:
 * - we are connected to the AP with an IP6
 * - we are connected to the AP with an IP4
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_IP4_BIT BIT1
#define WIFI_CONNECTED_IP6_BIT BIT2
#define WIFI_FAIL_BIT BIT3

SemaphoreHandle_t wifi_init_semaphore;
SemaphoreHandle_t scanresults_semaphore;

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

int s_retry_num        = 0;
esp_netif_t * wifi_sta = nullptr;

int sockfd = 0;
unsigned int clientlen;         // byte size of client's address
struct sockaddr_in6 clientaddr; // client addr

::pw::rpc::ServerWriter<chip_rpc_ScanResults> scanresult_writer;

class RAIIGuard
{
    std::function<void()> release_;

public:
    RAIIGuard(std::function<void()> acquire, std::function<void()> release) : release_(release) { acquire(); }
    virtual ~RAIIGuard() { release_(); }
};

void wifi_start_event_handler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "EVENT: WIFI_EVENT_STA_START");
        xEventGroupSetBits(s_wifi_event_group, WIFI_STA_UP_BIT);
    }
}

void wifi_conn_event_handler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "EVENT: WIFI_EVENT_STA_DISCONNECTED, reason: %d",
                 (static_cast<system_event_sta_disconnected_t *>(event_data))->reason);
        if (s_retry_num < kWiFiConnectRetryMax)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            (static_cast<chip_rpc_ConnectionResult *>(arg))->error =
                chip_rpc_CONNECTION_ERROR((static_cast<system_event_sta_disconnected_t *>(event_data))->reason);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_ERROR_CHECK(esp_netif_create_ip6_linklocal(wifi_sta));
        ESP_LOGI(TAG, "Connected, link local address created");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        auto * event = static_cast<ip_event_got_ip_t *>(event_data);
        ESP_LOGI(TAG, "got ip4: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = kWiFiConnectRetryMax;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_IP4_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6)
    {
        auto * event = static_cast<ip_event_got_ip6_t *>(event_data);
        ESP_LOGI(TAG, "got ip6: " IPV6STR, IPV62STR(event->ip6_info.ip));
        s_retry_num = kWiFiConnectRetryMax;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_IP6_BIT);
        xSemaphoreGive(wifi_init_semaphore);
    }
}

void event_handler2(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "******** DISCONNECTED FROM AP *********");
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler2);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &event_handler2);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6)
    {
        // This is in case not only link-local address is provided
        auto * event = static_cast<ip_event_got_ip6_t *>(event_data);
        ESP_LOGI(TAG, "got ip6 :" IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

void ReturnScanResults(void *)
{
    while (true)
    {
        xSemaphoreTake(scanresults_semaphore, portMAX_DELAY);
        chip_rpc_ScanResults out_scan_records;
        constexpr size_t kScanRecordsMax               = sizeof(chip_rpc_ScanResults().aps) / sizeof(chip_rpc_ScanResult);
        uint16_t num_scan_records                      = kScanRecordsMax;
        wifi_ap_record_t scan_records[kScanRecordsMax] = { 0 };
        esp_err_t err                                  = esp_wifi_scan_get_ap_records(&num_scan_records, scan_records);
        if (ESP_OK != err)
        {
            ESP_LOGI(TAG, "Error getting scanned APs: %d", err);
            num_scan_records = 0;
        }
        out_scan_records.aps_count = num_scan_records;
        for (size_t i = 0; i < num_scan_records; ++i)
        {
            memcpy(out_scan_records.aps[i].ssid.bytes, scan_records[i].ssid, sizeof(out_scan_records.aps[i].ssid.bytes));
            out_scan_records.aps[i].ssid.size = sizeof(out_scan_records.aps[i].ssid.bytes);
            memcpy(out_scan_records.aps[i].bssid.bytes, scan_records[i].bssid, sizeof(out_scan_records.aps[i].bssid.bytes));
            out_scan_records.aps[i].bssid.size    = sizeof(out_scan_records.aps[i].bssid.bytes);
            out_scan_records.aps[i].security_type = static_cast<chip_rpc_WIFI_SECURITY_TYPE>(scan_records[i].authmode);
            out_scan_records.aps[i].channel       = scan_records[i].primary;
            auto found_channel                    = kChannelToFreqMap.find(scan_records[i].primary);
            out_scan_records.aps[i].frequency     = (found_channel ? found_channel->second : 0);
            out_scan_records.aps[i].signal        = scan_records[i].rssi;
        }
        scanresult_writer.Write(out_scan_records);
    }
}

void scan_event_handler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        ESP_LOGI(TAG, "******** SCAN DONE *********");
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &scan_event_handler);
        xSemaphoreGive(scanresults_semaphore);
    }
}

pw::Status wifi_init_sta(void)
{
    PW_ESP_TRY(esp_netif_init());

    PW_ESP_TRY(esp_event_loop_create_default());
    wifi_sta = esp_netif_create_default_wifi_sta();
    PW_ESP_TRY(esp_netif_dhcpc_stop(wifi_sta));

    RAIIGuard guard(
        []() {
            s_wifi_event_group = xEventGroupCreate();
            esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_start_event_handler, NULL);
        },
        []() {
            esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_start_event_handler);
            vEventGroupDelete(s_wifi_event_group);
        });

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    PW_ESP_TRY(esp_wifi_init(&cfg));

    PW_ESP_TRY(esp_wifi_set_mode(WIFI_MODE_STA));
    PW_ESP_TRY(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_STA_UP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (!(bits & WIFI_STA_UP_BIT))
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return pw::Status::Unknown();
    }
    return pw::OkStatus();
}

pw::Status wifi_connect_sta(wifi_config_t * wifi_config, chip_rpc_ConnectionResult * result)
{
    {
        RAIIGuard guard(
            [result]() {
                s_wifi_event_group = xEventGroupCreate();
                esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_conn_event_handler, result);
                esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_conn_event_handler, NULL);
                esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &wifi_conn_event_handler, NULL);
            },
            []() {
                vEventGroupDelete(s_wifi_event_group);
                esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_conn_event_handler);
                esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &wifi_conn_event_handler);
                esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_conn_event_handler);
            });

        PW_ESP_TRY(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
        esp_err_t err = esp_wifi_connect();
        if (ESP_ERR_WIFI_SSID == err)
        {
            result->error = chip_rpc_CONNECTION_ERROR_NO_AP_FOUND;
            return pw::Status::NotFound();
        }

        EventBits_t bits =
            xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_IP6_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits & WIFI_CONNECTED_IP6_BIT)
        {
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifi_config->sta.ssid, "****" /*wifi_config.sta.password*/);
        }
        else if (bits & WIFI_FAIL_BIT)
        {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifi_config->sta.ssid, "****" /*wifi_config.sta.password*/);
            return pw::Status::Unavailable();
        }
        else
        {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
            return pw::Status::Unknown();
        }
    }

    // If successfully connected start responding to disconnect events and
    // potential further ip6 address assignments.
    PW_ESP_TRY(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler2, NULL));
    PW_ESP_TRY(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &event_handler2, NULL));

    result->error = chip_rpc_CONNECTION_ERROR_OK;
    return pw::OkStatus();
}

void UdpReceiver(void * pvParameters)
{
    int portno;                     // port to listen on
    struct sockaddr_in6 serveraddr; // server's addr
    char buf[kUdpBufferSize];       // rx message buf
    char * hostaddrp;               // dotted decimal host addr string
    int optval;                     // flag value for setsockopt
    int n;                          // message byte size

    while (1)
    {

        // Wait for wifi be initialized and ip6 address assigned.
        xSemaphoreTake(wifi_init_semaphore, portMAX_DELAY);

        ESP_LOGI(TAG, "UDP server (re)starting");

        portno = 8765;
        // socket: create the parent socket
        sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            ESP_LOGE(TAG, "ERROR opening socket");
            assert(0);
            return;
        }

        // setsockopt: Handy debugging trick that lets
        // us rerun the server immediately after we kill it;
        // otherwise we have to wait about 20 secs.
        // Eliminates "ERROR on binding: Address already in use" error.
        optval = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, static_cast<const void *>(&optval), sizeof(int));

        // build the server's Internet address
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin6_len    = sizeof(serveraddr);
        serveraddr.sin6_family = AF_INET6;
        serveraddr.sin6_addr   = in6addr_any;
        serveraddr.sin6_port   = htons((unsigned short) portno);

        //  bind: associate the parent socket with a port
        if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&serveraddr), sizeof(serveraddr)) < 0)
        {
            ESP_LOGE(TAG, "ERROR on binding");
            assert(0);
            return;
        }

        ESP_LOGI(TAG, "UDP server bound to port %d", portno);

        // main loop: wait for a datagram, then respond
        clientlen = sizeof(clientaddr);
        wifi_ap_record_t ap_info;
        fd_set readset;
        while (ESP_OK == esp_wifi_sta_get_ap_info(&ap_info))
        {
            // recvfrom: receive a UDP datagram from a client
            memset(buf, 0, sizeof(buf));

            FD_ZERO(&readset);
            FD_SET(sockfd, &readset);
            // TODO: somehow this throws an exception on wifi disconnect
            // "Guru Meditation Error: Core  0 panic'ed (LoadProhibited). Exception was unhandled"
            int select_err = select(sockfd + 1, &readset, nullptr, nullptr, nullptr);
            if (select_err < 0)
                continue;

            n = recvfrom(sockfd, buf, kUdpBufferSize, 0, reinterpret_cast<struct sockaddr *>(&clientaddr), &clientlen);
            if (n < 0)
                continue;
            else if (strncmp(buf, "reboot", 3) == 0)
            {
                esp_restart();
            }
            // Echo back
            n = sendto(sockfd, buf, n, 0, reinterpret_cast<struct sockaddr *>(&clientaddr), clientlen);
        }
    }
    // Never returns
}

} // namespace

namespace chip {
namespace rpc {

class GDMWifiBase final : public generated::GDMWifiBase<GDMWifiBase>
{
public:
    pw::Status GetChannel(ServerContext &, const chip_rpc_Empty & request, chip_rpc_Channel & response)
    {
        uint8_t channel = 0;
        wifi_second_chan_t second;
        PW_ESP_TRY(esp_wifi_get_channel(&channel, &second));
        response.channel = channel;
        return pw::OkStatus();
    }

    pw::Status GetSsid(ServerContext &, const chip_rpc_Empty & request, chip_rpc_Ssid & response)
    {
        wifi_config_t config;
        PW_ESP_TRY(esp_wifi_get_config(ESP_IF_WIFI_STA, &config));
        memcpy(response.ssid.bytes, config.sta.ssid, sizeof(response.ssid.bytes));
        response.ssid.size = 32;
        return pw::OkStatus();
    }

    pw::Status GetState(ServerContext &, const chip_rpc_Empty & request, chip_rpc_State & response)
    {
        wifi_ap_record_t ap_info;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
        PW_ESP_TRY(err);
        response.connected = (err != ESP_ERR_WIFI_NOT_CONNECT);
        return pw::OkStatus();
    }

    pw::Status GetMacAddress(ServerContext &, const chip_rpc_Empty & request, chip_rpc_MacAddress & response)
    {
        uint8_t mac[6];
        PW_ESP_TRY(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
        sprintf(response.mac_address, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return pw::OkStatus();
    }

    pw::Status GetWiFiInterface(ServerContext &, const chip_rpc_Empty & request, chip_rpc_WiFiInterface & response)
    {

        wifi_ap_record_t ap_info;
        PW_ESP_TRY(esp_wifi_sta_get_ap_info(&ap_info));
        sprintf(response.interface, "STA");
        return pw::OkStatus();
    }

    pw::Status GetIP4Address(ServerContext &, const chip_rpc_Empty & request, chip_rpc_IP4Address & response)
    {
        esp_netif_ip_info_t ip_info;
        PW_ESP_TRY(esp_netif_get_ip_info(wifi_sta, &ip_info));
        sprintf(response.address, IPSTR, IP2STR(&ip_info.ip));
        return pw::OkStatus();
    }

    pw::Status GetIP6Address(ServerContext &, const chip_rpc_Empty & request, chip_rpc_IP6Address & response)
    {
        esp_ip6_addr_t ip6{ 0 };
        PW_ESP_TRY(esp_netif_get_ip6_linklocal(wifi_sta, &ip6));
        sprintf(response.address, IPV6STR, IPV62STR(ip6));
        return pw::OkStatus();
    }

    void StartScan(ServerContext &, const chip_rpc_ScanConfig & request, ServerWriter<chip_rpc_ScanResults> & writer)
    {
        // Unregister any previous listeners so that no false
        // WIFI_EVENT_SCAN_DONE event is triggered.
        esp_err_t err = esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &scan_event_handler);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "Cannot unregister previous scan event handler, error: %d", err);
            return;
        }
        scanresult_writer = std::move(writer);
        wifi_scan_config_t scan_config{ 0 };
        if (request.ssid_count != 0)
        {
            scan_config.ssid = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(request.ssid[0].bytes));
        }
        if (request.bssid_count != 0)
        {
            scan_config.bssid = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(request.bssid[0].bytes));
        }
        scan_config.channel     = request.channel;
        scan_config.show_hidden = request.show_hidden;
        scan_config.scan_type   = static_cast<wifi_scan_type_t>(request.active_scan);
        if (request.active_scan)
        {
            scan_config.scan_time.active.min = request.scan_time_min_ms;
            scan_config.scan_time.active.max = request.scan_time_max_ms;
        }
        else
        {
            scan_config.scan_time.passive = request.scan_time_min_ms;
        }
        esp_wifi_scan_start(&scan_config, false /* block */);
        // Scan results will be sent to scanresult_writer once
        // WIFI_EVENT_SCAN_DONE event is received
        err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &scan_event_handler, NULL);
    }

    pw::Status StopScan(ServerContext &, const chip_rpc_Empty & request, chip_rpc_Empty & response)
    {
        esp_wifi_scan_stop();
        return pw::OkStatus();
    }

    pw::Status Connect(ServerContext &, const chip_rpc_ConnectionData & request, chip_rpc_ConnectionResult & response)
    {
        wifi_config_t wifi_config {
        .sta = {
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        .threshold = {
                .authmode = static_cast<wifi_auth_mode_t>(request.security_type),
            },

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
        memcpy(wifi_config.sta.ssid, request.ssid.bytes,
               std::min(sizeof(wifi_config.sta.ssid), static_cast<size_t>(request.ssid.size)));
        memcpy(wifi_config.sta.password, request.secret, std::min(sizeof(wifi_config.sta.password), strlen(request.secret)));
        PW_ESP_TRY(wifi_connect_sta(&wifi_config, &response));
        return pw::OkStatus();
    }

    pw::Status Disconnect(ServerContext &, const chip_rpc_Empty & request, chip_rpc_Empty & response)
    {
        PW_ESP_TRY(esp_wifi_disconnect());
        return pw::OkStatus();
    }
};

class TestService final : public generated::TestService<TestService>
{
public:
    void PingTest(ServerContext &, const chip_rpc_PingTestConfig & request, ServerWriter<chip_rpc_PingTestResponse> & writer)
    {
        // TODO: send a packet to request.ipv6addr_of_counterparty periodically at
        // every request.interval_sec
    }
};

} // namespace rpc
} // namespace chip

namespace {
using std::byte;

constexpr size_t kRpcStackSizeBytes         = (4 * 1024);
constexpr uint8_t kRpcTaskPriority          = 5;
constexpr size_t kScanResultsStackSizeBytes = (6 * 1024);
constexpr uint8_t kScanResultsTaskPriority  = 3;
constexpr size_t kTestStackSizeBytes        = (8 * 1024);
constexpr uint8_t kTestTaskPriority         = 5;

SemaphoreHandle_t uart_mutex;

class LoggerMutex : public chip::rpc::Mutex
{
public:
    void Lock() override { xSemaphoreTake(uart_mutex, portMAX_DELAY); }
    void Unlock() override { xSemaphoreGive(uart_mutex); }
};

LoggerMutex logger_mutex;

TaskHandle_t rpcTaskHandle;
TaskHandle_t scanResultsTaskHandle;
TaskHandle_t testTaskHandle;

pw::rpc::EchoService echo_service;
chip::rpc::GDMWifiBase gdm_wifi_service;
chip::rpc::TestService test_service;

void RegisterServices(pw::rpc::Server & server)
{
    server.RegisterService(echo_service);
    server.RegisterService(gdm_wifi_service);
    server.RegisterService(test_service);
}

void RunRpcService(void *)
{
    ::chip::rpc::Start(RegisterServices, &logger_mutex);
}

} // namespace

extern "C" void app_main()
{
    pw_sys_io_Init();
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_semaphore = xSemaphoreCreateBinary();
    ESP_LOGI(TAG, "wifi_init_sta: %s", pw_StatusString(wifi_init_sta()));

    ESP_LOGI(TAG, "----------- chip-esp32-pigweed-example starting -----------");

    uart_mutex            = xSemaphoreCreateMutex();
    scanresults_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(RunRpcService, "RPC", kRpcStackSizeBytes / sizeof(StackType_t), nullptr, kRpcTaskPriority, &rpcTaskHandle);

    xTaskCreate(ReturnScanResults, "ScanResults", kScanResultsStackSizeBytes / sizeof(StackType_t), nullptr,
                kScanResultsTaskPriority, &scanResultsTaskHandle);

    xTaskCreate(&UdpReceiver, "TestTask", kTestStackSizeBytes / sizeof(StackType_t), nullptr, kTestTaskPriority, &testTaskHandle);

    while (1)
    {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
