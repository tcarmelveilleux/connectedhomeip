#
#    Copyright (c) 2023 Project CHIP Authors
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

import asyncio
import logging
import queue
import time
from threading import Event

from chip import discovery
import chip.clusters as Clusters
from chip.setup_payload import SetupPayload
from chip.clusters import ClusterObjects as ClustersObjects
from chip.clusters.Attribute import SubscriptionTransaction, TypedAttributePath
from chip.utils import CommissioningBuildingBlocks
from matter_testing_support import MatterBaseTest, async_test_body, default_matter_test_main
from mobly import asserts


class AttributeChangeAccumulator:
    def __init__(self, name: str, expected_attribute: ClustersObjects.ClusterAttributeDescriptor, output: queue.Queue):
        self._name = name
        self._output = output
        self._expected_attribute = expected_attribute

    def __call__(self, path: TypedAttributePath, transaction: SubscriptionTransaction):
        if path.AttributeType == self._expected_attribute:
            data = transaction.GetAttribute(path)

            value = {
                'name': self._name,
                'endpoint': path.Path.EndpointId,
                'attribute': path.AttributeType,
                'value': data
            }
            logging.info("Got subscription report on client %s for %s: %s" % (self.name, path.AttributeType, data))
            self._output.put(value)

    @property
    def name(self) -> str:
        return self._name


class TC_DeviceBasicComposition(MatterBaseTest):
    @async_test_body
    async def test_TC_DeviceBasicComposition(self):
        dev_ctrl = self.default_controller

        # Get overrides for debugging the test
        num_fabrics_to_commission = self.user_params.get("num_fabrics_to_commission", 5)
        num_controllers_per_fabric = self.user_params.get("num_controllers_per_fabric", 3)
        # Immediate reporting
        min_report_interval_sec = self.user_params.get("min_report_interval_sec", 0)
        # 10 minutes max reporting interval --> We don't care about keep-alives per-se and
        # want to avoid resubscriptions
        max_report_interval_sec = self.user_params.get("max_report_interval_sec", 10 * 60)
        # Time to wait after changing NodeLabel for subscriptions to all hit. This is dependant
        # on MRP params of subscriber and on actual min_report_interval.
        # TODO: Determine the correct max value depending on target. Test plan doesn't say!
        timeout_delay_sec = self.user_params.get("timeout_delay_sec", max_report_interval_sec * 2)

        # Determine final result
        if False:
            asserts.fail("Failed test !")

        # Pass is implicit if not failed

        if self.matter_test_config.qr_code_content is not None:
            qr_code = self.matter_test_config.qr_code_content
            setup_payload = SetupPayload().ParseQrCode(qr_code)
        elif self.matter_test_config.manual_code is not None:
            manual_code = self.matter_test_config.manual_code
            setup_payload = SetupPayload().ParseManualPairingCode(manual_code)
        else:
            asserts.fail("Require either --qr-code or --manual-code to proceed with PASE needed for test.")

        if setup_payload.short_discriminator != None:
            filter_type = discovery.FilterType.SHORT_DISCRIMINATOR
            filter_value = setup_payload.short_discriminator
        else:
            filter_type = discovery.FilterType.LONG_DISCRIMINATOR
            filter_value = setup_payload.long_discriminator

        commissionable_nodes = dev_ctrl.DiscoverCommissionableNodes(filter_type, filter_value, stopOnFirst=True, timeoutSecond=15)
        print(commissionable_nodes)
        #TODO: Support BLE
        if commissionable_nodes is not None and len(commissionable_nodes) > 0:
            commissionable_node = commissionable_nodes[0]
            instance_name = f"{commissionable_node.instanceName}._matterc._udp.local"
            vid = f"{commissionable_node.vendorId}"
            pid = f"{commissionable_node.productId}"
            address = f"{commissionable_node.addresses[0]}"
            logging.info(f"Found instance {instance_name}, VID={vid}, PID={pid}, Address={address}")

            node_id = 1
            dev_ctrl.EstablishPASESessionIP(address, setup_payload.setup_passcode, node_id)

            node_content = await dev_ctrl.ReadAttribute(node_id, [()])
            print(node_content)


if __name__ == "__main__":
    default_matter_test_main()
