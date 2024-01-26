/*
 *
 *    Copyright (c) 2024 Project CHIP Authors
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

#include <app/TestEventTriggerDelegate.h>
#include <lib/support/Span.h>
#include <lib/support/UnitTestRegistration.h>
#include <nlunit-test.h>

using namespace chip;

namespace {

class TestEventHandler : public TestEventTriggerHandler
{
  public:
    TestEventHandler() = delete;

    explicit TestEventHandler(uint64_t supportedEventTriggerValue) : mSupportedEventTriggerValue(supportedEventTriggerValue) {}

    CHIP_ERROR HandleEventTrigger(uint64_t eventTrigger) override
    {
        return (eventTrigger == mSupportedEventTriggerValue) ? CHIP_NO_ERROR : CHIP_ERROR_INVALID_ARGUMENT;
    }

  private:
    uint64_t mSupportedEventTriggerValue;
};

class TestEventDelegate : public TestEventTriggerDelegate
{
    public:
      explicit TestEventDelegate(const ByteSpan & enableKey) : mEnableKey(enableKey) {}

      bool DoesEnableKeyMatch(const ByteSpan & enableKey) const override
      {
          return !mEnableKey.empty() && mEnableKey.data_equal(enableKey);
      }

  private:
    ByteSpan mEnableKey;
};

void TestKeyChecking(nlTestSuite * aSuite, void * aContext)
{
    const uint8_t kTestKey[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const uint8_t kBadKey[16] = {255, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const uint8_t kDiffLenBadKey[17] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    TestEventDelegate delegate{ByteSpan{kTestKey}};

    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{kTestKey}) == true);
    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{kBadKey}) == false);
    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{kDiffLenBadKey}) == false);
    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{}) == false);
}

void TestHandlerManagement(nlTestSuite * aSuite, void * aContext)
{
    const uint8_t kTestKey[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    TestEventDelegate delegate{ByteSpan{kTestKey}};

    TestEventHandler event1Handler{1};
    TestEventHandler event2Handler{2};

    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{kTestKey}) == true);



    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{kBadKey}) == false);
    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{kDiffLenBadKey}) == false);
    NL_TEST_ASSERT(aSuite, delegate.DoesEnableKeyMatch(ByteSpan{}) == false);
}

int TestSetup(void * inContext)
{
    return SUCCESS;
}


int TestTeardown(void * inContext)
{
    return SUCCESS;
}

} // namespace

int TestTestEventTriggerDelegate()
{
    static nlTest sTests[] = { NL_TEST_DEF("TestKeyChecking", TestKeyChecking),
                               NL_TEST_SENTINEL() };

    nlTestSuite theSuite = {
        "TestTestEventTriggerDelegate",
        &sTests[0],
        TestSetup,
        TestTeardown,
    };

    nlTestRunner(&theSuite, nullptr);
    return (nlTestRunnerStats(&theSuite));
}

CHIP_REGISTER_TEST_SUITE(TestTestEventTriggerDelegate)
