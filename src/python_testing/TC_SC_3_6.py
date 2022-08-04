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

from unicodedata import name
from matter_testing_support import MatterBaseTest, default_matter_test_main, async_test_body
from chip.interaction_model import Status
import chip.clusters as Clusters
import chip.FabricAdmin
import logging
from mobly import asserts
from chip.utils import CommissioningBuildingBlocks
from chip.clusters.Attribute import TypedAttributePath, SubscriptionTransaction
import asyncio
import queue

# TODO DOCUMENT:
#  - Clusters.Basic.Attributes.NodeLabel --> not isinstanceable
#  - Transactions *NEED* metadata about the client is an IMMUTABLE safe to access way
#  - MUST be able to set the callback BEFORE you actually subscribe so the first report
#    is handled by yourself not the print
#  - Remove all fabrics at the end
#  - Create empty/null sub at the end to kill all subs

class AttributeChangeAccumulator:
    def __init__(self, name):
        self._name = name
        self._all_data = queue.Queue()

    def __call__(self, path: TypedAttributePath, transaction: SubscriptionTransaction):
        data = transaction.GetAttribute(path)
        value = {
            'sub_name': self._name,
            'endpoint': path.Path.EndpointId,
            'attribute': path.AttributeType,
            'value': data
        }
        self._all_data.put(value)
        logging.info("Got sub poke")

    @property
    def all_data(self):
        data = []
        while True:
            try:
                data.append(self._all_data.get(block=False))
            except queue.Empty:
                break
        return data

    @property
    def name(self) -> str:
        return self._name


class TC_SC_3_6(MatterBaseTest):
    @async_test_body
    async def test_TC_SC_3_6(self):
        all_data = []

        dev_ctrl = self.default_controller

        num_fabrics_to_commission = self.user_params.get("num_fabrics_to_commission", 2)
        num_controllers_per_fabric = self.user_params.get("num_controllers_per_fabric", 3)

        all_names = []
        for fabric_idx in range(num_fabrics_to_commission):
            for controller_idx in range(num_controllers_per_fabric):
                all_names.append("RD%d%s" % (fabric_idx + 1, chr(ord('A') + controller_idx)))

        client_list = []

        # Node IDs for subsequent for subsequent controllers start at 200, follow 200, 300, ...
        node_ids = [200 + (i * 100) for i in range(num_controllers_per_fabric - 1)]

        # Prepare clients for first fabric, that includes the default controller
        dev_ctrl.name = all_names.pop(0)
        client_list.append(dev_ctrl)

        if num_controllers_per_fabric > 1:
            new_controllers = await CommissioningBuildingBlocks.CreateControllersOnFabric(fabricAdmin=dev_ctrl.fabricAdmin, adminDevCtrl=dev_ctrl, controllerNodeIds=node_ids, privilege=Clusters.AccessControl.Enums.Privilege.kAdminister, targetNodeId=self.dut_node_id)
            for controller in new_controllers:
                controller.name = all_names.pop()
                client_list.extend(new_controllers)

        # Prepare clients for subsequent fabrics
        for i in range(num_fabrics_to_commission - 1):
            admin_index = 2 + i
            logging.info("Commissioning fabric %d/%d" % (admin_index, num_fabrics_to_commission))
            new_fabric_admin = chip.FabricAdmin.FabricAdmin(vendorId=0xFFF1, adminIndex=admin_index)
            new_admin_ctrl = new_fabric_admin.NewController(nodeId=self.dut_node_id)
            new_admin_ctrl.name = all_names.pop(0)
            client_list.append(new_admin_ctrl)
            await CommissioningBuildingBlocks.AddNOCForNewFabricFromExisting(commissionerDevCtrl=dev_ctrl, newFabricDevCtrl=new_admin_ctrl, existingNodeId=self.dut_node_id, newNodeId=self.dut_node_id)

            if num_controllers_per_fabric > 1:
                new_controllers = await CommissioningBuildingBlocks.CreateControllersOnFabric(fabricAdmin=new_fabric_admin, adminDevCtrl=new_admin_ctrl,
                    controllerNodeIds=node_ids, privilege=Clusters.AccessControl.Enums.Privilege.kAdminister, targetNodeId=self.dut_node_id)
                for controller in new_controllers:
                    controller.name = all_names.pop()
                    client_list.extend(new_controllers)

        subscriptions = []
        sub_handlers = []
        for client in client_list:
            sub = await client.ReadAttribute(nodeid=self.dut_node_id, attributes=[(0, Clusters.Basic.Attributes.NodeLabel)], reportInterval=(1, 10), keepSubscriptions=False)
            attribute_handler = AttributeChangeAccumulator(name=client.name)
            sub.SetAttributeUpdateCallback(attribute_handler)
            subscriptions.append(sub)
            sub_handlers.append(attribute_handler)

        await asyncio.sleep(1)
        await client_list[0].WriteAttribute(self.dut_node_id, [(0, Clusters.Basic.Attributes.NodeLabel(value="make-sub-move"))])
        await asyncio.sleep(10)

        for handler in sub_handlers:
            for data in handler.all_data:
                logging.info("Handler name: %s, data: %s" % (handler.name, data))


if __name__ == "__main__":
    default_matter_test_main()
