/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
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

// THIS FILE IS GENERATED BY ZAP

#include <app-common/zap-generated/callback.h>
#include <app-common/zap-generated/cluster-id.h>
#include <lib/support/Span.h>
#include <protocols/interaction_model/Constants.h>

using namespace chip;

// Cluster Init Functions
void emberAfClusterInitCallback(EndpointId endpoint, ClusterId clusterId)
{
    switch (clusterId)
    {
    case ZCL_ACCESS_CONTROL_CLUSTER_ID:
        emberAfAccessControlClusterInitCallback(endpoint);
        break;
    case ZCL_ACCOUNT_LOGIN_CLUSTER_ID:
        emberAfAccountLoginClusterInitCallback(endpoint);
        break;
    case ZCL_ADMINISTRATOR_COMMISSIONING_CLUSTER_ID:
        emberAfAdministratorCommissioningClusterInitCallback(endpoint);
        break;
    case ZCL_APPLICATION_BASIC_CLUSTER_ID:
        emberAfApplicationBasicClusterInitCallback(endpoint);
        break;
    case ZCL_APPLICATION_LAUNCHER_CLUSTER_ID:
        emberAfApplicationLauncherClusterInitCallback(endpoint);
        break;
    case ZCL_AUDIO_OUTPUT_CLUSTER_ID:
        emberAfAudioOutputClusterInitCallback(endpoint);
        break;
    case ZCL_BARRIER_CONTROL_CLUSTER_ID:
        emberAfBarrierControlClusterInitCallback(endpoint);
        break;
    case ZCL_BASIC_CLUSTER_ID:
        emberAfBasicClusterInitCallback(endpoint);
        break;
    case ZCL_BINARY_INPUT_BASIC_CLUSTER_ID:
        emberAfBinaryInputBasicClusterInitCallback(endpoint);
        break;
    case ZCL_BINDING_CLUSTER_ID:
        emberAfBindingClusterInitCallback(endpoint);
        break;
    case ZCL_BOOLEAN_STATE_CLUSTER_ID:
        emberAfBooleanStateClusterInitCallback(endpoint);
        break;
    case ZCL_BRIDGED_ACTIONS_CLUSTER_ID:
        emberAfBridgedActionsClusterInitCallback(endpoint);
        break;
    case ZCL_BRIDGED_DEVICE_BASIC_CLUSTER_ID:
        emberAfBridgedDeviceBasicClusterInitCallback(endpoint);
        break;
    case ZCL_CHANNEL_CLUSTER_ID:
        emberAfChannelClusterInitCallback(endpoint);
        break;
    case ZCL_COLOR_CONTROL_CLUSTER_ID:
        emberAfColorControlClusterInitCallback(endpoint);
        break;
    case ZCL_CONTENT_LAUNCH_CLUSTER_ID:
        emberAfContentLauncherClusterInitCallback(endpoint);
        break;
    case ZCL_DESCRIPTOR_CLUSTER_ID:
        emberAfDescriptorClusterInitCallback(endpoint);
        break;
    case ZCL_DIAGNOSTIC_LOGS_CLUSTER_ID:
        emberAfDiagnosticLogsClusterInitCallback(endpoint);
        break;
    case ZCL_DOOR_LOCK_CLUSTER_ID:
        emberAfDoorLockClusterInitCallback(endpoint);
        break;
    case ZCL_ELECTRICAL_MEASUREMENT_CLUSTER_ID:
        emberAfElectricalMeasurementClusterInitCallback(endpoint);
        break;
    case ZCL_ETHERNET_NETWORK_DIAGNOSTICS_CLUSTER_ID:
        emberAfEthernetNetworkDiagnosticsClusterInitCallback(endpoint);
        break;
    case ZCL_FIXED_LABEL_CLUSTER_ID:
        emberAfFixedLabelClusterInitCallback(endpoint);
        break;
    case ZCL_FLOW_MEASUREMENT_CLUSTER_ID:
        emberAfFlowMeasurementClusterInitCallback(endpoint);
        break;
    case ZCL_GENERAL_COMMISSIONING_CLUSTER_ID:
        emberAfGeneralCommissioningClusterInitCallback(endpoint);
        break;
    case ZCL_GENERAL_DIAGNOSTICS_CLUSTER_ID:
        emberAfGeneralDiagnosticsClusterInitCallback(endpoint);
        break;
    case ZCL_GROUP_KEY_MANAGEMENT_CLUSTER_ID:
        emberAfGroupKeyManagementClusterInitCallback(endpoint);
        break;
    case ZCL_GROUPS_CLUSTER_ID:
        emberAfGroupsClusterInitCallback(endpoint);
        break;
    case ZCL_IAS_ZONE_CLUSTER_ID:
        emberAfIasZoneClusterInitCallback(endpoint);
        break;
    case ZCL_IDENTIFY_CLUSTER_ID:
        emberAfIdentifyClusterInitCallback(endpoint);
        break;
    case ZCL_ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
        emberAfIlluminanceMeasurementClusterInitCallback(endpoint);
        break;
    case ZCL_KEYPAD_INPUT_CLUSTER_ID:
        emberAfKeypadInputClusterInitCallback(endpoint);
        break;
    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
        emberAfLevelControlClusterInitCallback(endpoint);
        break;
    case ZCL_LOCALIZATION_CONFIGURATION_CLUSTER_ID:
        emberAfLocalizationConfigurationClusterInitCallback(endpoint);
        break;
    case ZCL_LOW_POWER_CLUSTER_ID:
        emberAfLowPowerClusterInitCallback(endpoint);
        break;
    case ZCL_MEDIA_INPUT_CLUSTER_ID:
        emberAfMediaInputClusterInitCallback(endpoint);
        break;
    case ZCL_MEDIA_PLAYBACK_CLUSTER_ID:
        emberAfMediaPlaybackClusterInitCallback(endpoint);
        break;
    case ZCL_MODE_SELECT_CLUSTER_ID:
        emberAfModeSelectClusterInitCallback(endpoint);
        break;
    case ZCL_NETWORK_COMMISSIONING_CLUSTER_ID:
        emberAfNetworkCommissioningClusterInitCallback(endpoint);
        break;
    case ZCL_OTA_PROVIDER_CLUSTER_ID:
        emberAfOtaSoftwareUpdateProviderClusterInitCallback(endpoint);
        break;
    case ZCL_OTA_REQUESTOR_CLUSTER_ID:
        emberAfOtaSoftwareUpdateRequestorClusterInitCallback(endpoint);
        break;
    case ZCL_OCCUPANCY_SENSING_CLUSTER_ID:
        emberAfOccupancySensingClusterInitCallback(endpoint);
        break;
    case ZCL_ON_OFF_CLUSTER_ID:
        emberAfOnOffClusterInitCallback(endpoint);
        break;
    case ZCL_ON_OFF_SWITCH_CONFIG_CLUSTER_ID:
        emberAfOnOffSwitchConfigurationClusterInitCallback(endpoint);
        break;
    case ZCL_OPERATIONAL_CREDENTIALS_CLUSTER_ID:
        emberAfOperationalCredentialsClusterInitCallback(endpoint);
        break;
    case ZCL_POWER_SOURCE_CLUSTER_ID:
        emberAfPowerSourceClusterInitCallback(endpoint);
        break;
    case ZCL_POWER_SOURCE_CONFIGURATION_CLUSTER_ID:
        emberAfPowerSourceConfigurationClusterInitCallback(endpoint);
        break;
    case ZCL_PRESSURE_MEASUREMENT_CLUSTER_ID:
        emberAfPressureMeasurementClusterInitCallback(endpoint);
        break;
    case ZCL_PUMP_CONFIG_CONTROL_CLUSTER_ID:
        emberAfPumpConfigurationAndControlClusterInitCallback(endpoint);
        break;
    case ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID:
        emberAfRelativeHumidityMeasurementClusterInitCallback(endpoint);
        break;
    case ZCL_SCENES_CLUSTER_ID:
        emberAfScenesClusterInitCallback(endpoint);
        break;
    case ZCL_SOFTWARE_DIAGNOSTICS_CLUSTER_ID:
        emberAfSoftwareDiagnosticsClusterInitCallback(endpoint);
        break;
    case ZCL_SWITCH_CLUSTER_ID:
        emberAfSwitchClusterInitCallback(endpoint);
        break;
    case ZCL_TARGET_NAVIGATOR_CLUSTER_ID:
        emberAfTargetNavigatorClusterInitCallback(endpoint);
        break;
    case ZCL_TEMP_MEASUREMENT_CLUSTER_ID:
        emberAfTemperatureMeasurementClusterInitCallback(endpoint);
        break;
    case ZCL_TEST_CLUSTER_ID:
        emberAfTestClusterClusterInitCallback(endpoint);
        break;
    case ZCL_THERMOSTAT_CLUSTER_ID:
        emberAfThermostatClusterInitCallback(endpoint);
        break;
    case ZCL_THERMOSTAT_UI_CONFIG_CLUSTER_ID:
        emberAfThermostatUserInterfaceConfigurationClusterInitCallback(endpoint);
        break;
    case ZCL_THREAD_NETWORK_DIAGNOSTICS_CLUSTER_ID:
        emberAfThreadNetworkDiagnosticsClusterInitCallback(endpoint);
        break;
    case ZCL_TIME_FORMAT_LOCALIZATION_CLUSTER_ID:
        emberAfTimeFormatLocalizationClusterInitCallback(endpoint);
        break;
    case ZCL_UNIT_LOCALIZATION_CLUSTER_ID:
        emberAfUnitLocalizationClusterInitCallback(endpoint);
        break;
    case ZCL_USER_LABEL_CLUSTER_ID:
        emberAfUserLabelClusterInitCallback(endpoint);
        break;
    case ZCL_WAKE_ON_LAN_CLUSTER_ID:
        emberAfWakeOnLanClusterInitCallback(endpoint);
        break;
    case ZCL_WIFI_NETWORK_DIAGNOSTICS_CLUSTER_ID:
        emberAfWiFiNetworkDiagnosticsClusterInitCallback(endpoint);
        break;
    case ZCL_WINDOW_COVERING_CLUSTER_ID:
        emberAfWindowCoveringClusterInitCallback(endpoint);
        break;
    default:
        // Unrecognized cluster ID
        break;
    }
}

void __attribute__((weak)) emberAfAccessControlClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfAccountLoginClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfAdministratorCommissioningClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfApplicationBasicClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfApplicationLauncherClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfAudioOutputClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBarrierControlClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBasicClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBinaryInputBasicClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBindingClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBooleanStateClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBridgedActionsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfBridgedDeviceBasicClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfChannelClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfColorControlClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfContentLauncherClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfDescriptorClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfDiagnosticLogsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfDoorLockClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfElectricalMeasurementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfEthernetNetworkDiagnosticsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfFixedLabelClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfFlowMeasurementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfGeneralCommissioningClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfGeneralDiagnosticsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfGroupKeyManagementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfGroupsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfIasZoneClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfIdentifyClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfIlluminanceMeasurementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfKeypadInputClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfLevelControlClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfLocalizationConfigurationClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfLowPowerClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfMediaInputClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfMediaPlaybackClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfModeSelectClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfNetworkCommissioningClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfOtaSoftwareUpdateProviderClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfOtaSoftwareUpdateRequestorClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfOccupancySensingClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfOnOffClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfOnOffSwitchConfigurationClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfOperationalCredentialsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfPowerSourceClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfPowerSourceConfigurationClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfPressureMeasurementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfPumpConfigurationAndControlClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfRelativeHumidityMeasurementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfScenesClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfSoftwareDiagnosticsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfSwitchClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfTargetNavigatorClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfTemperatureMeasurementClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfTestClusterClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfThermostatClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfThermostatUserInterfaceConfigurationClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfThreadNetworkDiagnosticsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfTimeFormatLocalizationClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfUnitLocalizationClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfUserLabelClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfWakeOnLanClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfWiFiNetworkDiagnosticsClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}
void __attribute__((weak)) emberAfWindowCoveringClusterInitCallback(EndpointId endpoint)
{
    // To prevent warning
    (void) endpoint;
}

//
// Non-Cluster Related Callbacks
//

void __attribute__((weak)) emberAfAddToCurrentAppTasksCallback(EmberAfApplicationTask tasks) {}

void __attribute__((weak)) emberAfRemoveFromCurrentAppTasksCallback(EmberAfApplicationTask tasks) {}

EmberAfAttributeWritePermission __attribute__((weak))
emberAfAllowNetworkWriteAttributeCallback(EndpointId endpoint, ClusterId clusterId, AttributeId attributeId, uint8_t mask,
                                          uint8_t * value, uint8_t type)
{
    return EMBER_ZCL_ATTRIBUTE_WRITE_PERMISSION_ALLOW_WRITE_NORMAL; // Default
}

bool __attribute__((weak)) emberAfAttributeReadAccessCallback(EndpointId endpoint, ClusterId clusterId, AttributeId attributeId)
{
    return true;
}

bool __attribute__((weak)) emberAfAttributeWriteAccessCallback(EndpointId endpoint, ClusterId clusterId, AttributeId attributeId)
{
    return true;
}

bool __attribute__((weak)) emberAfDefaultResponseCallback(ClusterId clusterId, CommandId commandId, EmberAfStatus status)
{
    return false;
}

bool __attribute__((weak)) emberAfPreMessageSendCallback(EmberAfMessageStruct * messageStruct, EmberStatus * status)
{
    return false;
}

bool __attribute__((weak)) emberAfMessageSentCallback(const MessageSendDestination & destination, EmberApsFrame * apsFrame,
                                                      uint16_t msgLen, uint8_t * message, EmberStatus status)
{
    return false;
}

EmberAfStatus __attribute__((weak))
emberAfExternalAttributeReadCallback(EndpointId endpoint, ClusterId clusterId, EmberAfAttributeMetadata * attributeMetadata,
                                     uint8_t * buffer, uint16_t maxReadLength)
{
    return EMBER_ZCL_STATUS_FAILURE;
}

EmberAfStatus __attribute__((weak))
emberAfExternalAttributeWriteCallback(EndpointId endpoint, ClusterId clusterId, EmberAfAttributeMetadata * attributeMetadata,
                                      uint8_t * buffer)
{
    return EMBER_ZCL_STATUS_FAILURE;
}

uint32_t __attribute__((weak)) emberAfGetCurrentTimeCallback()
{
    return 0;
}

bool __attribute__((weak))
emberAfGetEndpointInfoCallback(EndpointId endpoint, uint8_t * returnNetworkIndex, EmberAfEndpointInfoStruct * returnEndpointInfo)
{
    return false;
}

void __attribute__((weak)) emberAfRegistrationAbortCallback() {}

EmberStatus __attribute__((weak))
emberAfInterpanSendMessageCallback(EmberAfInterpanHeader * header, uint16_t messageLength, uint8_t * message)
{
    return EMBER_LIBRARY_NOT_PRESENT;
}

bool __attribute__((weak)) emberAfStartMoveCallback()
{
    return false;
}

chip::Protocols::InteractionModel::Status __attribute__((weak))
MatterPreAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t mask, uint8_t type, uint16_t size,
                                 uint8_t * value)
{
    return chip::Protocols::InteractionModel::Status::Success;
}

void __attribute__((weak)) MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t mask,
                                                             uint8_t type, uint16_t size, uint8_t * value)
{}
