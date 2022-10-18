/*
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

#pragma once

#include <inet/EndpointQueueFilter.h>
#include <inet/IPPacketInfo.h>
#include <system/SystemPacketBuffer.h>

namespace mdns {
namespace Minimal {

class MinimalMdnsPacketFilter : public chip::inet::EndpointQueueFilter
{
public:
    FilterOutcome FilterBeforeEnqueue(const void * endpoint, const IPPacketInfo & pktInfo, const chip::System::PacketBufferHandle & pktPayload) override
    {
        // TODO: Hostname is copied across many different records. The main MAC address belong to DnsSd.
        // TODO:
    }
}


} // namespace Minimal
} // namespace mdns