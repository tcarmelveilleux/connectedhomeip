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

#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterface.h>

namespace chip {
namespace app {

class DiscoBallCommandHandler : public CommandHandlerInterface
{
public:
    // This is the disco ball command handler for all endpoints (endpoint ID is NullOptional)
    DiscoBallCommandHandler() : CommandHandlerInterface(chip::NullOptional, Clusters::DiscoBall::Id) {}

    // Inherited from CommandHandlerInterface
    void InvokeCommand(HandlerContext & handlerContext) override;
};

class DiscoBallAttributeAccess : public AttributeAccessInterface
{
public:
    // This is the disco ball attribute access interface for all endpoints (endpoint ID is NullOptional)
    DiscoBallAttributeAccess() : AttributeAccessInterface(chip::NullOptional, Clusters::DiscoBall::Id) {}

    // AttributeAccessInterface
    CHIP_ERROR Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override;
    CHIP_ERROR Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder) override;
};

} // namespace app
} // namespace chip
