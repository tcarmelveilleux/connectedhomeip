#
#    Copyright (c) 2025 Project CHIP Authors
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
# test-runner-runs:
#   run1:
#     app: ${ALL_CLUSTERS_APP}
#     app-args: --discriminator 1234 --KVS kvs1 --trace-to json:${TRACE_APP}.json
#     script-args: >
#       --storage-path admin_storage.json
#       --commissioning-method on-network
#       --discriminator 1234
#       --passcode 20202021
#       --PICS src/app/tests/suites/certification/ci-pics-values
#       --trace-to json:${TRACE_TEST_JSON}.json
#       --trace-to perfetto:${TRACE_TEST_PERFETTO}.perfetto
#     factory-reset: true
#     quiet: true
# === END CI TEST ARGUMENTS ===

import chip.clusters as Clusters
from chip.testing.matter_testing import MatterBaseTest, TestStep, async_test_body, default_matter_test_main
from chip.tlv import TLVReader
from chip.utils import CommissioningBuildingBlocks
from mobly import asserts
from test_plan_support import (commission_from_existing, commission_if_required, read_attribute, remove_fabric,
                               verify_commissioning_successful, verify_success)
import dataclasses
import logging


class MatterCertParser:
    SUBJECT_TAG = 6
    SUBJECT_PUBLIC_KEY_TAG = 9
    SUBJECT_FABRIC_ID_TAG = 21

    def __init__(self, matter_cert_bytes: bytes):
        self.parsed_tlv = TLVReader(matter_cert_bytes).get()["Any"]

    def get_subject_names(self) -> dict[int, object]:
        return {tag: value for tag, value in self.parsed_tlv[self.SUBJECT_TAG]}

    def get_public_key_bytes(self) -> bytes:
        public_key_bytes = self.parsed_tlv[self.SUBJECT_PUBLIC_KEY_TAG]
        return public_key_bytes


class TC_OPCREDS_3_9(MatterBaseTest):
    def desc_TC_OPCREDS_3_9(self):
        return " [DUTServer]"

    def steps_TC_OPCREDS_3_9(self):
        """
        Sketch steps:

        * `NOCs` attribute is readable for all fabrics, even a different one (i.e. check that NOCStruct no longer has fabric-sensitive fields)
        * If there the VVSC attribute in `NOC` struct is present for a fabric, the certificate format must conform to the requirements as specified in the spec
        ** If a VVSC does not conform to the specification, the VID cannot be verified for the corresponding fabric
        * If there is a `VidVerificationStatement` attribute present in the `FabricDescriptorStruct` for a fabric, the format must conform to the requirements as specified in the spec
        ** If the `VidVerificationStatement` does not conform to the specification, the VID cannot be verified for the corresponding fabric
        ** If a VVSC cannot be found corresponding to the `vid_verification_signer_skid` in the `VidVerificationStatement`, the fabric cannot be verified
        * If a Fabric Administrator uses non-global roots for issuing NOCs and desires to have their VID verified by other clients:
        ** Set the `VidVerificationStatement` for their respective fabric using `SetVidVerificationStatement` command
        ** If the Fabric Administrator uses VVSC per fabric, set the VVSC attribute in the `NOCStruct` using the `SetVidVerificationStatement` command
        * When a `verifier` client wishes to validate a `candidate` fabric VID, it needs to read the following attributes for the `candidate` fabric:
        ** Read the `NOCs` attribute of the Operational Credentials Cluster using a non-fabric-filtered read
        ** Read the `Fabrics` attribute of the Operational Credentials Cluster using a non-fabric-filtered read
        ** Read the `TrustedRootCertificates` attribute of the Operational Credentials Cluster using a non-fabric-filtered read
        ** Select a `candidate` fabric listed in the `Fabrics` attribute whose the `VendorID` field is to be verified
        ** Validate the `VendorID` for 3 different uses-cases:
        *** Candidate Fabric has no VidVerificationStatement installed:
        **** Generate the payload for `SignVidVerificationRequest` as per spec and send to server against the candidate using the `fabric_index` of the entry in `Fabrics` whose `VendorID` is being verified
        **** Validate signature received as part of `SignVidVerificationResponse` as per the spec
        **** Obtain the RCAC for the `candidate` fabric corresponding to the `Subject Key ID` for that fabric in the `TrustedRootCertificates`
        **** Verify that the NOC chain validates against the entire chain obtained using the `Subject Key ID`
        ***** The `VendorID` is deemed verified only if the chain validates successfully
        *** Candidate Fabric has VidVerificationStatement installed, but no VVSC:
        **** Generate the payload for `SignVidVerificationRequest` as per spec and send to server against the candidate using the `fabric_index` of the entry in `Fabrics` whose `VendorID` is being verified
        **** Validate signature received as part of `SignVidVerificationResponse` as per the spec
        **** Obtain the VVSC matching the `vid_verification_signer_skid` for the `candidate` `VendorID`
        **** Validate the path of the VVSC include any intermediates to the root of the VVSC as per requirements written in the spec
        **** Verify the signature of `VidVerificationStatement` using the VVSC as per the spec.
        ***** The `VendorID` is deemed verified only if the signature of `VidVerificationStatement` validates successfully
        ** Fabric has VidVerificationStatement installed, with a VVSC:
        **** Generate the payload for `SignVidVerificationRequest` as per spec and send to server against the candidate using the `fabric_index` of the entry in `Fabrics` whose `VendorID` is being verified
        **** Validate signature received as part of `SignVidVerificationResponse` as per the spec
        **** Obtain the VVSC from the `NOCStruct` attribute entry associated with the `fabric_index` field of the `candidate` fabric
        **** Obtain the intermediates and root corresponding to the VVSC matching the `vid_verification_signer_skid` for the `candidate` `VendorID`
        **** Validate the path of the VVSC include any intermediates to the root of the VVSC as per requirements written in the spec
        **** Verify the signature of `VidVerificationStatement` using the VVSC as per the spec.
        ***** The `VendorID` is deemed verified only if the signature of `VidVerificationStatement` validates successfully
        """
        def verify_SignVIDVerificationResponseSuccess(controller: str) -> str:
            XXXXXXXXXXXXXXXXXXXXXXXxx
            return (f"- Verify there is one entry returned. Verify FabricIndex matches `fabric_index_{controller}`.\n"
                    f"- Verify the RootPublicKey matches the public key for rcac_{controller}.\n"
                    f"- Verify the VendorID matches the vendor ID for {controller}.\n"
                    f"- Verify the FabricID matches the fabricID for {controller}")



        return [TestStep(0, commission_if_required('CR1'), is_commissioning=True),
                TestStep(1, f"{commission_from_existing('CR1', 'CR2')}\n. - Save the FabricIndex from the NOCResponse as `fabric_index_CR2`.\n -Save the RCAC's subject public key as root_public_key_CR2."),
                TestStep(2, f"CR1 {read_attribute('NOCs')}",
                         "Verify that the NOCs associated with CR1 and CR2 have the right fabric ID"),
                TestStep(3, f"CR1 sends SignVIDVerificationRequest for fabric index of CR2.", verify_SignVIDVerificationResponseSuccess),
                TestStep(3, remove_fabric('fabric_index_CR2', 'CR1'), verify_success())
        ]

    @async_test_body
    async def test_TC_OPCREDS_3_9(self):
        # TODO(test_plans#5046): actually make the test follow final test plan. For now
        # it functionally validates the VID Verification parts of Operational Credentials Cluster

        opcreds = Clusters.OperationalCredentials

        self.step(0)

        # Read fabric index for CR1 after commissioning it.
        cr1_fabric_index = await self.read_single_attribute_check_success(
            dev_ctrl=self.default_controller,
            node_id=self.dut_node_id,
            cluster=opcreds,
            attribute=opcreds.Attributes.CurrentFabricIndex
        )

        self.step(1)
        dev_ctrl = self.default_controller

        new_certificate_authority = self.certificate_authority_manager.NewCertificateAuthority()
        cr2_vid = 0xFFF2
        cr2_fabricId = 2222
        cr2_new_fabric_admin = new_certificate_authority.NewFabricAdmin(
            vendorId=cr2_vid, fabricId=cr2_fabricId)
        cr2_nodeid = self.default_controller.nodeId + 1
        cr2_dut_node_id = self.dut_node_id + 1

        cr2_new_admin_ctrl = cr2_new_fabric_admin.NewController(
            nodeId=cr2_nodeid)
        success, nocResp, chain = await CommissioningBuildingBlocks.AddNOCForNewFabricFromExisting(
            commissionerDevCtrl=dev_ctrl, newFabricDevCtrl=cr2_new_admin_ctrl,
            existingNodeId=self.dut_node_id, newNodeId=cr2_dut_node_id
        )
        asserts.assert_true(success, "Commissioning DUT into CR2's fabrics must succeed.")

        rcacResp = chain.rcacBytes

        cr2_fabric_index = nocResp.fabricIndex
        cr2_rcac = MatterCertParser(rcacResp)
        cr2_root_public_key = cr2_rcac.get_public_key_bytes()

        fabric_indices = {
            "CR1": cr1_fabric_index,
            "CR2": cr2_fabric_index
        }

        fabric_ids = {
            "CR1": self.default_controller.fabricId,
            "CR2": cr2_fabricId
        }

        # Read NOCs and validate that both the entry for CR1 and CR2 are readable
        # and have the right expected fabricId
        self.step(2)
        nocs_list = await self.read_single_attribute_check_success(
            cluster=opcreds,
            attribute=opcreds.Attributes.NOCs,
            fabric_filtered=False
        )

        fabric_ids_from_certs = {}
        for controller_name, fabric_index in fabric_indices.items():
            for noc_struct in nocs_list:
                if noc_struct.fabricIndex != fabric_index:
                    continue

                noc_cert = MatterCertParser(noc_struct.noc)
                for tag, value in noc_cert.get_subject_names().items():
                    if tag == noc_cert.SUBJECT_FABRIC_ID_TAG:
                        fabric_ids_from_certs[controller_name] = value

        logging.info(f"Fabric IDs found: {fabric_ids_from_certs}")

        for controller_name, fabric_id in fabric_ids.items():
            asserts.assert_in(controller_name, fabric_ids_from_certs.keys(), f"Did not find {controller_name}'s fabric the NOCs attribute")
            asserts.assert_equal(fabric_ids_from_certs[controller_name], fabric_id, f"Did not find {controller_name}'s fabric ID in the correct NOC")

        self.step(3)
        cmd = opcreds.Commands.RemoveFabric(cr2_fabric_index)
        resp = await self.send_single_cmd(cmd=cmd)
        asserts.assert_equal(
            resp.statusCode, opcreds.Enums.NodeOperationalCertStatusEnum.kOk)


if __name__ == "__main__":
    default_matter_test_main()
