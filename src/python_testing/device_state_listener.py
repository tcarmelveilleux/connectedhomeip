#
#    Copyright (c) 2022 Project CHIP Authors
#    All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

# See https://github.com/project-chip/connectedhomeip/blob/master/docs/testing/python.md#defining-the-ci-test-arguments
# for details about the block below.
#
# === BEGIN CI TEST ARGUMENTS ===
# test-runner-runs: run1
# test-runner-run/run1/app: ${TYPE_OF_APP}
# test-runner-run/run1/factoryreset: True
# test-runner-run/run1/quiet: True
# test-runner-run/run1/app-args: --discriminator 1234 --KVS kvs1 --trace-to json:${TRACE_APP}.json
# test-runner-run/run1/script-args: --storage-path admin_storage.json --commissioning-method on-network --discriminator 1234 --passcode 20202021 --trace-to json:${TRACE_TEST_JSON}.json --trace-to perfetto:${TRACE_TEST_PERFETTO}.perfetto
# === END CI TEST ARGUMENTS ===

import logging
import time

import chip.clusters as Clusters
from matter_testing_support import MatterBaseTest, TestStep, async_test_body, default_matter_test_main
from mobly import asserts


class DeviceStateListenerTool(MatterBaseTest):
    def steps_DeviceStateListener(self) -> list[TestStep]:
        steps = [TestStep(1, "Commissioning, already done", is_commissioning=True),
                 TestStep(2, "Listen to device activity"),
                 ]
        return steps

    def desc_DeviceStateListener(self) -> str:
        return 'List to entire device state changes over time'

    @async_test_body
    async def test_TC_ENDPOINT_2_1(self):
        self.step(1)  # Commissioning

        self.step(2)
        dev_ctrl = self.default_controller

        event_listener = EventChangeCallback(cluster)
        await event_listener.start(self.default_controller, self.dut_node_id, endpoint=endpoint_id)
        attrib_listener = ClusterAttributeChangeAccumulator(cluster)
        await attrib_listener.start(self.default_controller, self.dut_node_id, endpoint=endpoint_id)

        while True:
          time.sleep()

if __name__ == "__main__":
    default_matter_test_main()
