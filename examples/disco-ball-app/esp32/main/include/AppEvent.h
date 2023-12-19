/*
 *
 *    Copyright (c) 2022-2023 Project CHIP Authors
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

#pragma once

#include <stdint.h>

#include <app-common/zap-generated/cluster-enums.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/BitFlags.h>

struct AppEvent
{
    AppEvent() {};

    typedef void (*EventHandler)(AppEvent *);

    enum AppEventTypes : uint16_t
    {
        kTimer = 0,
        kSetMotorDirectionSpeed,
        kStopMotor,
        kSetLightState,
        kUpdateWobble,
    };

    AppEventTypes event_type;

    union
    {
        struct
        {
            void * context;
        } timer_event;

        struct
        {
            chip::EndpointId endpoint_id;
            bool is_clockwise;
            uint8_t speed_rpm;
        } set_motor_direction_speed_event;

        struct
        {
            chip::EndpointId endpoint_id;
        } stop_motor_event;

        struct
        {
            chip::EndpointId endpoint_id;
            bool enabled;
        } set_light_state_event;

        struct
        {
            chip::EndpointId endpoint_id;
            chip::BitFlags<chip::app::Clusters::DiscoBall::WobbleBitmap> wobble_setting;
            uint8_t speed;
        } update_wobble_event;
    };

    EventHandler handler;
    void * context;
};
