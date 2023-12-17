/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <app/clusters/disco-ball-server/disco-ball-cluster-logic.h>

#include <nlunit-test.h>
#include <stdint.h>

#include <lib/core/CHIPError.h>
#include <lib/core/CHIPPersistentStorageDelegate.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/support/Span.h>
#include <lib/support/BitFlags.h>
#include <lib/support/UnitTestRegistration.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/cluster-enum.h>

using namespace chip;
using namespace chip::app;

namespace {

void TestDiscoBallInitialization(nlTestSuite * inSuite, void * inContext)
{
    NL_TEST_ASSERT(inSuite, false);
}

void TestDiscoBallAttributeSetters(nlTestSuite * inSuite, void * inContext)
{
    NL_TEST_ASSERT(inSuite, false);
}

const nlTest sLifecyleTests[] = {
    NL_TEST_DEF("Test Disco Ball initialization error handling", TestDiscoBallInitialization),
    NL_TEST_SENTINEL()
};

const nlTest sLogicTests[] = {
    NL_TEST_DEF("Test Disco Ball attribute setters", TestDiscoBallAttributeSetters),
    NL_TEST_SENTINEL()
};

int TestSetup(void * inContext)
{
    VerifyOrReturnError(CHIP_NO_ERROR == chip::Platform::MemoryInit(), FAILURE);
    return SUCCESS;
}

int TestTearDown(void * inContext)
{
    chip::Platform::MemoryShutdown();
    return SUCCESS;
}

} // namespace

int TestDiscoBallClusterLogicLifecycle()
{
    nlTestSuite theSuite = { "Test Disco Ball Cluster's common portable logic lifecycle", &sLifecyleTests[0], TestSetup, TestTearDown };
    nlTestRunner(&theSuite, nullptr);
    return nlTestRunnerStats(&theSuite);
}

int TestDiscoBallClusterLogic()
{
    nlTestSuite theSuite = { "Test Disco Ball Cluster's common portable logic", &sLogicTests[0], TestSetup, TestTearDown };
    nlTestRunner(&theSuite, nullptr);
    return nlTestRunnerStats(&theSuite);
}

CHIP_REGISTER_TEST_SUITE(TestDiscoBallClusterLogicLifecycle)
CHIP_REGISTER_TEST_SUITE(TestDiscoBallClusterLogic)
