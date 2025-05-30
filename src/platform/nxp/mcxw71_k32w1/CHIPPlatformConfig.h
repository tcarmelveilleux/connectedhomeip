/*
 *
 *    Copyright (c) 2020, 2025 Project CHIP Authors
 *    Copyright (c) 2020 Google LLC.
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

/**
 *    @file
 *          Platform-specific configuration overrides for CHIP on
 *          NXP K32W platforms.
 */

#pragma once

#include <stdint.h>

// ==================== General Platform Adaptations ====================

#define CHIP_CONFIG_ABORT() abort()

#define CHIP_CONFIG_PERSISTED_STORAGE_KEY_TYPE uint16_t
#define CHIP_CONFIG_PERSISTED_STORAGE_ENC_MSG_CNTR_ID 1
#define CHIP_CONFIG_PERSISTED_STORAGE_MAX_KEY_LENGTH 2

#define CHIP_CONFIG_LIFETIIME_PERSISTED_COUNTER_KEY 0x01

#define CHIP_DEVICE_CONFIG_EVENT_LOGGING_DEBUG_BUFFER_SIZE (512)
#define CHIP_DEVICE_CONFIG_EVENT_LOGGING_INFO_BUFFER_SIZE (512)
#define CHIP_DEVICE_CONFIG_EVENT_LOGGING_CRIT_BUFFER_SIZE (512)

#define CHIP_CONFIG_MAX_FABRICS 5

// ==================== Security Adaptations ====================

// FIXME: K32W currently set to CHIP (Does this use Entropy.cpp ?)

// ==================== General Configuration Overrides ====================

#ifndef CHIP_CONFIG_MAX_UNSOLICITED_MESSAGE_HANDLERS
#define CHIP_CONFIG_MAX_UNSOLICITED_MESSAGE_HANDLERS 8
#endif // CHIP_CONFIG_MAX_UNSOLICITED_MESSAGE_HANDLERS

#ifndef CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS
#define CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS 8
#endif // CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS

#ifndef CHIP_LOG_FILTERING
#define CHIP_LOG_FILTERING 0
#endif // CHIP_LOG_FILTERING

#ifndef CHIP_CONFIG_BDX_MAX_NUM_TRANSFERS
#define CHIP_CONFIG_BDX_MAX_NUM_TRANSFERS 1
#endif // CHIP_CONFIG_BDX_MAX_NUM_TRANSFERS

// ==================== WDM Configuration Overrides ====================

#ifndef WDM_MAX_NUM_SUBSCRIPTION_CLIENTS
#define WDM_MAX_NUM_SUBSCRIPTION_CLIENTS 2
#endif // WDM_MAX_NUM_SUBSCRIPTION_CLIENTS

#ifndef WDM_MAX_NUM_SUBSCRIPTION_HANDLERS
#define WDM_MAX_NUM_SUBSCRIPTION_HANDLERS 2
#endif // WDM_MAX_NUM_SUBSCRIPTION_HANDLERS

#ifndef WDM_PUBLISHER_MAX_NOTIFIES_IN_FLIGHT
#define WDM_PUBLISHER_MAX_NOTIFIES_IN_FLIGHT 2
#endif // WDM_PUBLISHER_MAX_NOTIFIES_IN_FLIGHT

#if CONFIG_CHIP_ENABLE_ICD_SUPPORT

#ifndef CHIP_CONFIG_ICD_IDLE_MODE_DURATION_SEC
#define CHIP_CONFIG_ICD_IDLE_MODE_DURATION_SEC CONFIG_CHIP_ICD_IDLE_MODE_DURATION
#endif // CHIP_CONFIG_ICD_IDLE_MODE_DURATION_SEC

#ifndef CHIP_CONFIG_ICD_ACTIVE_MODE_DURATION_MS
#define CHIP_CONFIG_ICD_ACTIVE_MODE_DURATION_MS CONFIG_CHIP_ICD_ACTIVE_MODE_DURATION
#endif // CHIP_CONFIG_ICD_ACTIVE_MODE_DURATION_MS

#ifndef CHIP_CONFIG_ICD_ACTIVE_MODE_THRESHOLD_MS
#define CHIP_CONFIG_ICD_ACTIVE_MODE_THRESHOLD_MS CONFIG_CHIP_ICD_ACTIVE_MODE_THRESHOLD
#endif // CHIP_CONFIG_ICD_ACTIVE_MODE_THRESHOLD_MS

#ifndef CHIP_CONFIG_ICD_CLIENTS_SUPPORTED_PER_FABRIC
#define CHIP_CONFIG_ICD_CLIENTS_SUPPORTED_PER_FABRIC CONFIG_CHIP_ICD_CLIENTS_PER_FABRIC
#endif // CHIP_CONFIG_ICD_CLIENTS_SUPPORTED_PER_FABRIC

#ifndef CHIP_CONFIG_SYNCHRONOUS_REPORTS_ENABLED
#define CHIP_CONFIG_SYNCHRONOUS_REPORTS_ENABLED 1
#endif
#endif

/**
 * @def CHIP_DEVICE_CONFIG_KVS_WEAR_STATS
 *
 * @brief Toggle support for key value store wear stats on or off.
 */
#ifndef CHIP_DEVICE_CONFIG_KVS_WEAR_STATS
#define CHIP_DEVICE_CONFIG_KVS_WEAR_STATS 0
#endif // CHIP_DEVICE_CONFIG_KVS_WEAR_STATS

/**
 * @def CHIP_IM_MAX_NUM_COMMAND_HANDLER
 *
 * @brief Defines the maximum number of CommandHandler, limits the number of active commands transactions on server.
 */
#ifndef CHIP_IM_MAX_NUM_COMMAND_HANDLER
#define CHIP_IM_MAX_NUM_COMMAND_HANDLER 2
#endif // CHIP_IM_MAX_NUM_COMMAND_HANDLER

/**
 * @def CHIP_IM_MAX_NUM_WRITE_HANDLER
 *
 * @brief Defines the maximum number of WriteHandler, limits the number of active write transactions on server.
 */
#ifndef CHIP_IM_MAX_NUM_WRITE_HANDLER
#define CHIP_IM_MAX_NUM_WRITE_HANDLER 2
#endif // CHIP_IM_MAX_NUM_WRITE_HANDLER
