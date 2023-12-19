/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

#include <app/clusters/disco-ball-server/disco-ball-cluster-logic.h>

namespace chip {
namespace app {

class DefaultDiscoBallStorage : public DiscoBallClusterState::NonVolatileStorageInterface
{
  public:
    CHIP_ERROR SaveToStorage(const DiscoBallClusterState & attributes) override
    {
        // TODO: Implement actual storage.
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR LoadFromStorage(DiscoBallClusterState & attributes) override
    {
        // TODO: Implement actual storage.
        return CHIP_ERROR_NOT_FOUND;
    }

    void RemoveDataForFabric(FabricIndex fabric_index) override
    {
        (void)fabric_index;
    }

    void RemoveDataForAllFabrics() override { }
};

} // namespace app
} // namespace chip
