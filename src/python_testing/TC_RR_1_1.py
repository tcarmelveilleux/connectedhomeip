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

from matter_testing_support import MatterBaseTest, default_matter_test_main, async_test_body
from chip.interaction_model import Status
import chip.clusters as Clusters
import chip.FabricAdmin
import logging
from mobly import asserts
from chip.utils import CommissioningBuildingBlocks


class TC_RR_1_1(MatterBaseTest):
    @async_test_body
    async def test_TC_RR_1_1(self):
        logging.info("==== Step 1: Commission DUT on 5 fabrics with maximized NOC chains")
        """
        |1|11.18.6, 6.1.3|| TH begins the process of commissioning the DUT. After receiving the CSRResponse TH obtains or generates a NOC, the Root CA Certificate, ICAC and IPK. The certificates shall have their subjects padded with additional data such that they are each the maximum certificate size of 400 bytes when encoded in the MatterCertificateEncoding. TH sends AddNOC command with CaseAdminSubject as 0xFFFF_FFFD_0001_0001, IpkValue with 3 Epoch Keys and NOC must have CAT 0x0001_0001. Repeat the process to commission DUT to 5 different fabrics. If, for a given fabric, it is not possible for all certificates in the chain to be of length 400 bytes, then at least 1 of the set must be larger or equal to 370 bytes and at least one of the set must be larger or equal to 350 bytes, and at least one must be exactly 400 bytes. All certificate chains have to be valid. | Verify that the device can be commissioned to the minimum value of _SupportedFabrics_ Attribute on the Node Operational Credentials Cluster which is 5.

        |2|11.18.5.3, 11.1.6.3|| TH writes the user-visible Label field for Fabrics to maximum size of 32 characters. TH writes the _NodeLabel_ Attribute to maximum size of 32 characters. | Verify that the device supports minimum constraints of Fabric Label and _NodeLabel_ Attribute as mentioned in spec.

        """

        # TODO: Maximize NOC chain
        dev_ctrl = self.default_controller

        num_fabrics_to_commission = self.user_params.get("num_fabrics_to_commissiong", 5)

        clientList = []
        clientList.append(dev_ctrl)
        clientList.extend(await CommissioningBuildingBlocks.CreateControllersOnFabric(fabricAdmin=dev_ctrl.fabricAdmin, adminDevCtrl=dev_ctrl, controllerNodeIds=[200, 300], privilege=Clusters.AccessControl.Enums.Privilege.kAdminister, targetNodeId=self.dut_node_id))

        for i in range(num_fabrics_to_commission  - 1):
            logging.info("Commissioning fabric %d/%d" % (1 + i, num_fabrics_to_commission))
            newFabricAdmin = chip.FabricAdmin.FabricAdmin(vendorId=0xFFF1)
            newAdminCtrl = newFabricAdmin.NewController()
            clientList.append(newAdminCtrl)
            await CommissioningBuildingBlocks.AddNOCForNewFabricFromExisting(commissionerDevCtrl=dev_ctrl, newFabricDevCtrl=newAdminCtrl, existingNodeId=self.dut_node_id, newNodeId=self.dut_node_id)

            clientList.extend(await CommissioningBuildingBlocks.CreateControllersOnFabric(fabricAdmin=newFabricAdmin, adminDevCtrl=newAdminCtrl,
                controllerNodeIds=[200, 300], privilege=Clusters.AccessControl.Enums.Privilege.kAdminister, targetNodeId=self.dut_node_id))

        subscriptions = []
        for client in clientList:
            sub = await client.ReadAttribute(nodeid=self.dut_node_id, attributes=[(0, Clusters.Basic.Attributes.NodeLabel)], reportInterval=(0, 1), keepSubscriptions=False)
            # TODO: What is this for?
            sub.OverrideLivenessTimeoutMs(1500)
            subscriptions.append(sub)



if __name__ == "__main__":
    default_matter_test_main()
