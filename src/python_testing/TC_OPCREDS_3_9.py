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

import enum
import hashlib
import logging
import inspect
import re
import sys
from binascii import unhexlify
from typing import Optional

import chip.clusters as Clusters
from chip.interaction_model import InteractionModelError, Status
from chip.testing.matter_testing import MatterBaseTest, async_test_body, default_matter_test_main, TestStep
from chip.tlv import TLVReader
from chip.utils import CommissioningBuildingBlocks
from ecdsa import NIST256p, VerifyingKey
from ecdsa.keys import BadSignatureError
from mobly import asserts

import nest_asyncio
nest_asyncio.apply()


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


# From Matter spec src/crypto_primitives/crypto_primitives.py
class MappingsV1(enum.IntEnum):
    CHIP_CRYPTO_HASH_LEN_BITS = 256
    CHIP_CRYPTO_HASH_LEN_BYTES = 32
    CHIP_CRYPTO_HASH_BLOCK_LEN_BYTES = 64
    CHIP_CRYPTO_GROUP_SIZE_BITS = 256
    CHIP_CRYPTO_GROUP_SIZE_BYTES = 32
    CHIP_CRYPTO_PUBLIC_KEY_SIZE_BYTES = (2 * CHIP_CRYPTO_GROUP_SIZE_BYTES) + 1
    CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BITS = 128
    CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES = 16
    CHIP_CRYPTO_AEAD_MIC_LENGTH_BITS = 128
    CHIP_CRYPTO_AEAD_MIC_LENGTH_BYTES = 16
    CHIP_CRYPTO_AEAD_NONCE_LENGTH_BYTES = 13


# From Matter spec src/crypto_primitives/crypto_primitives.py
def bytes_from_hex(hex: str) -> bytes:
    """Converts any `hex` string representation including `01:ab:cd` to bytes

    Handles any whitespace including newlines, which are all stripped.
    """
    return unhexlify(re.sub(r'(\s|:)', "", hex))


# From Matter spec src/crypto_primitives/vid_verify_payload_test_vector.py
# ECDSA-with-SHA256 using NIST P-256
FABRIC_BINDING_VERSION_1 = 0x01
STATEMENT_VERSION_1 = 0x21
VID_VERIFICATION_CLIENT_CHALLENGE_SIZE_BYTES = 32
ATTESTATION_CHALLENGE_SIZE_BYTES = MappingsV1.CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES
VID_VERIFICATION_STAMEMENT_SIZE_BYTES_V1 = 85


def verify_signature(public_key: bytes, message: bytes, signature: bytes) -> bool:
    """Verify a `signature` on the given `message` using `public_key`. Returns True on success."""

    verifying_key: VerifyingKey = VerifyingKey.from_string(public_key, curve=NIST256p)
    assert verifying_key.curve == NIST256p

    try:
        return verifying_key.verify(signature, message, hashfunc=hashlib.sha256)
    except BadSignatureError:
        return False


# From Matter spec src/crypto_primitives/vid_verify_payload_test_vector.py
def generate_vendor_fabric_binding_message(
        root_public_key_bytes: bytes,
        fabric_id: int,
        vendor_id: int) -> bytes:

    assert len(root_public_key_bytes) == MappingsV1.CHIP_CRYPTO_PUBLIC_KEY_SIZE_BYTES
    assert fabric_id > 0 and fabric_id <= 0xFFFF_FFFF_FFFF_FFFF
    assert vendor_id > 0 and vendor_id <= 0xFFF4

    fabric_id_bytes = fabric_id.to_bytes(length=8, byteorder='big')
    vendor_id_bytes = vendor_id.to_bytes(length=2, byteorder='big')
    vendor_fabric_binding_message = FABRIC_BINDING_VERSION_1.to_bytes(
        length=1) + root_public_key_bytes + fabric_id_bytes + vendor_id_bytes
    return vendor_fabric_binding_message

# From Matter spec src/crypto_primitives/vid_verify_payload_test_vector.py


def generate_vendor_id_verification_tbs(fabric_binding_version: int,
                                        attestation_challenge: bytes,
                                        client_challenge: bytes,
                                        fabric_index: int,
                                        vendor_fabric_binding_message: bytes,
                                        vid_verification_statement: Optional[bytes] = None) -> bytes:
    assert len(attestation_challenge) == ATTESTATION_CHALLENGE_SIZE_BYTES
    assert len(client_challenge) == VID_VERIFICATION_CLIENT_CHALLENGE_SIZE_BYTES
    # Valid fabric indices are [1..254]. 255 is forbidden.
    assert fabric_index >= 1 and fabric_index <= 254
    assert vendor_fabric_binding_message
    assert fabric_binding_version == FABRIC_BINDING_VERSION_1

    vendor_id_verification_tbs = fabric_binding_version.to_bytes(length=1)
    vendor_id_verification_tbs += client_challenge + attestation_challenge + fabric_index.to_bytes(length=1)
    vendor_id_verification_tbs += vendor_fabric_binding_message
    if vid_verification_statement is not None:
        vendor_id_verification_tbs += vid_verification_statement

    return vendor_id_verification_tbs


def get_unassigned_fabric_index(fabric_indices: list[int]) -> int:
    for fabric_index in range(1, 255):
        if fabric_index not in fabric_indices:
            return fabric_index
    else:
        asserts.fail(f"Somehow could not find an unallocated fabric index in {fabric_indices}")


class TestStepBlockPassException(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
    pass

class test_step(object):
    def __init__(self, id=None, description="", verification=None, is_commissioning=False):
        caller = inspect.currentframe().f_back.f_locals.get('self', None)
        if isinstance(caller, MatterBaseTest):
            self._test_instance = caller
        else:
            raise RuntimeError("Can only use `test_step` inside a MatterBaseTest-derived class")

        if id is None:
            id = self._test_instance.current_step_id
            next_step_id = self._test_instance.get_next_step_id(self._test_instance.current_step_id)
            self._test_instance.current_step_id = next_step_id
        else:
            self._test_instance.current_step_id = id

        self._id = id
        self._description = description
        self._verification = verification
        self._is_commissioning = is_commissioning

    def trace(self, frame, event, arg):
        raise TestStepBlockPassException("If you see this, you have a `pass` in a `with test_step(...):` block!")

    def __enter__(self):
        if self._test_instance.is_aggregating_steps:
            self._test_instance.aggregated_steps.append(TestStep(self._id, self._description, self._verification, self._is_commissioning))

            # Let's manipulate the callstack via tracing to skip the entire `with` block in steps extraction mode.
            sys.settrace(lambda *args, **keys: None)
            frame = sys._getframe(1)
            frame.f_trace = self.trace

            ########## NO MORE CALLS CAN BE PAST HERE IN __enter__(). Otherwise, the "skip block" feature doesn't work.
        else:
            self._test_instance.step(self._id)

        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if type is None:
            return  # No exception
        if isinstance(exc_value, TestStepBlockPassException):
            return True  # Suppress special exception we expect to see.

    @property
    def id(self):
        return self._id

    def skip(self):
        self._test_instance.mark_current_step_skipped()


class TC_OPCREDS_3_9(MatterBaseTest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.is_aggregating_steps: bool = False
        self.aggregated_steps: list[TestStep] = []
        self.current_step_id = 0
        self.reset_steps()

    def desc_TC_OPCREDS_3_9(self):
        return "[DUTServer] Pre-release TC-OPCREDS-3.9 test case."

    def steps_TC_OPCREDS_3_9(self) -> list[TestStep]:
        try:
            self.current_step_id = 0
            self.is_aggregating_steps = True
            self.aggregated_steps = []
            self.test_TC_OPCREDS_3_9()
        finally:
            self.is_aggregating_steps = False

        self.current_step_id = 0
        return self.aggregated_steps

    def reset_steps(self):
        self.aggregated_steps = []
        self.is_aggregating_steps = False
        self.current_step_id = 0

    def get_next_step_id(self, current_step_id) -> object:
        if isinstance(current_step_id, int):
            return current_step_id + 1
        elif isinstance(current_step_id, str):
            match = re.search(r"^(?P<step_number>\d+)", current_step_id)
            if match:
                return int(match.group('step_number')) + 1

        raise ValueError(f"Can't find correct next step for {current_step_id}")

    @async_test_body
    async def test_TC_OPCREDS_3_9(self):
        # TODO(test_plans#5046): actually make the test follow final test plan. For now
        # it functionally validates the VID Verification parts of Operational Credentials Cluster

        with test_step(description="Commission DUT in TH1's fabric.", is_commissioning=True):
            opcreds = Clusters.OperationalCredentials
            dev_ctrl = self.default_controller


            # Read fabric index for CR1 after commissioning it.
            cr1_fabric_index = await self.read_single_attribute_check_success(
                dev_ctrl=self.default_controller,
                node_id=self.dut_node_id,
                cluster=opcreds,
                attribute=opcreds.Attributes.CurrentFabricIndex
            )

            root_certs = await self.read_single_attribute_check_success(
                dev_ctrl=self.default_controller,
                node_id=self.dut_node_id,
                cluster=opcreds,
                attribute=opcreds.Attributes.TrustedRootCertificates,
                fabric_filtered=True
            )
            asserts.assert_true(len(root_certs) == 1,
                                f"Expecting exactly one root from TrustedRootCertificates (TH1's), got {len(root_certs)}")
            th1_root_parser = MatterCertParser(root_certs[0])
            cr1_root_public_key = th1_root_parser.get_public_key_bytes()

        with test_step(description="Commission DUT in TH2's fabric."):
            new_certificate_authority = self.certificate_authority_manager.NewCertificateAuthority()
            new_certificate_authority.omitIcacAlways = True
            cr2_vid = 0xFFF2
            cr2_fabricId = 2222
            cr2_new_fabric_admin = new_certificate_authority.NewFabricAdmin(
                vendorId=cr2_vid, fabricId=cr2_fabricId)
            cr2_nodeid = dev_ctrl.nodeId + 1
            cr2_dut_node_id = self.dut_node_id + 1

            cr2_dev_ctrl = cr2_new_fabric_admin.NewController(
                nodeId=cr2_nodeid)
            success, nocResp, chain = await CommissioningBuildingBlocks.AddNOCForNewFabricFromExisting(
                commissionerDevCtrl=dev_ctrl, newFabricDevCtrl=cr2_dev_ctrl,
                existingNodeId=self.dut_node_id, newNodeId=cr2_dut_node_id
            )
            asserts.assert_true(success, "Commissioning DUT into CR2's fabrics must succeed.")

            rcacResp = chain.rcacBytes

            cr2_fabric_index = nocResp.fabricIndex
            cr2_rcac = MatterCertParser(rcacResp)
            cr2_root_public_key = cr2_rcac.get_public_key_bytes()

        # Read NOCs and validate that both the entry for CR1 and CR2 are readable
        # and have the right expected fabricId
        with test_step(description="Read DUT's NOCs attribute and validate both fabrics have expected values extractable from their NOC."):
            nocs_list = await self.read_single_attribute_check_success(
                cluster=opcreds,
                attribute=opcreds.Attributes.NOCs,
                fabric_filtered=False
            )

            fabric_ids_from_certs = {}
            noc_public_keys_from_certs = {}

            fabric_indices = {
                "CR1": cr1_fabric_index,
                "CR2": cr2_fabric_index
            }

            fabric_ids = {
                "CR1": self.default_controller.fabricId,
                "CR2": cr2_fabricId
            }

            for controller_name, fabric_index in fabric_indices.items():
                for noc_struct in nocs_list:
                    if noc_struct.fabricIndex != fabric_index:
                        continue

                    noc_cert = MatterCertParser(noc_struct.noc)
                    for tag, value in noc_cert.get_subject_names().items():
                        if tag == noc_cert.SUBJECT_FABRIC_ID_TAG:
                            fabric_ids_from_certs[controller_name] = value
                    noc_public_keys_from_certs[controller_name] = noc_cert.get_public_key_bytes()

            logging.info(f"Fabric IDs found: {fabric_ids_from_certs}")

            for controller_name, fabric_id in fabric_ids.items():
                asserts.assert_in(controller_name, fabric_ids_from_certs.keys(),
                                  f"Did not find {controller_name}'s fabric the NOCs attribute")
                asserts.assert_equal(fabric_ids_from_certs[controller_name], fabric_id,
                                    f"Did not find {controller_name}'s fabric ID in the correct NOC")

        with test_step(description="Read DUT's Fabrics attribute and validate both fabrics have expected values."):
            fabric_roots = {
                "CR1": cr1_root_public_key,
                "CR2": cr2_root_public_key
            }

            fabric_vendor_ids = {
                "CR1": 0xfff1,
                "CR2": cr2_vid
            }

            fabrics_list = await self.read_single_attribute_check_success(
                cluster=opcreds,
                attribute=opcreds.Attributes.Fabrics,
                fabric_filtered=False
            )

            for controller_name, fabric_index in fabric_indices.items():
                for fabric_struct in fabrics_list:
                    if fabric_struct.fabricIndex != fabric_index:
                        continue

                    asserts.assert_equal(fabric_struct.rootPublicKey, fabric_roots[controller_name],
                                        f"Did not find matching root public key in Fabrics attribute for {controller_name}")
                    asserts.assert_equal(
                        fabric_struct.vendorID, fabric_vendor_ids[controller_name], f"Did not find matching VendorID in Fabrics attribute for {controller_name}")
                    asserts.assert_equal(
                        fabric_struct.fabricID, fabric_ids[controller_name], f"Did not find matching FabricID in Fabrics attribute for {controller_name}")

        with test_step(description="TH1 sends SignVIDVerificationRequest for TH2's fabric. Verify the response and signature."):

            client_challenge = bytes_from_hex(
                "a1:a2:a3:a4:a5:a6:a7:a8:a9:aa:ab:ac:ad:ae:af:b0:b1:b2:b3:b4:b5:b6:b7:b8:b9:ba:bb:bc:bd:be:bf:c0")
            sign_vid_verification_response = await self.send_single_cmd(cmd=opcreds.Commands.SignVIDVerificationRequest(fabricIndex=cr2_fabric_index, clientChallenge=client_challenge))

            asserts.assert_equal(sign_vid_verification_response.fabricIndex, cr2_fabric_index,
                                "FabricIndex in SignVIDVerificationResponse must match request.")

            # Locally generate the vendor_id_verification_tbs to check the signature.
            expected_vendor_fabric_binding_message = generate_vendor_fabric_binding_message(
                root_public_key_bytes=cr2_root_public_key, fabric_id=cr2_fabricId, vendor_id=cr2_vid)
            attestation_challenge = dev_ctrl.GetConnectedDeviceSync(self.dut_node_id, allowPASE=False).attestationChallenge
            vendor_id_verification_tbs = generate_vendor_id_verification_tbs(sign_vid_verification_response.fabricBindingVersion, attestation_challenge,
                                                                            client_challenge, sign_vid_verification_response.fabricIndex, expected_vendor_fabric_binding_message, vid_verification_statement=b"")

            # Check signature against vendor_id_verification_tbs
            noc_cert = MatterCertParser(noc_struct.noc)
            asserts.assert_true(verify_signature(public_key=noc_public_keys_from_certs["CR2"], message=vendor_id_verification_tbs,
                                signature=sign_vid_verification_response.signature), "VID Verification Signature must validate using DUT's NOC public key")

        with test_step(description="Send bad SignVIDVerificationRequest commands and verify failures"):
            # Must fail with correct client challenge but non-existent fabric.
            unassigned_fabric_index = get_unassigned_fabric_index(fabric_indices.values())
            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SignVIDVerificationRequest(fabricIndex=unassigned_fabric_index, clientChallenge=client_challenge))

            asserts.assert_equal(exception_context.exception.status, Status.ConstraintError,
                                f"Expected CONSTRAINT_ERROR from SignVIDVerificationRequest against fabricIndex {unassigned_fabric_index}")

            # Must fail with correct client challenge but fabricIndex 0.
            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SignVIDVerificationRequest(fabricIndex=0, clientChallenge=client_challenge))

            asserts.assert_equal(exception_context.exception.status, Status.ConstraintError,
                                "Expected CONSTRAINT_ERROR from SignVIDVerificationRequest against fabricIndex 0")

            # Must fail with client challenge different than expected length
            CHALLENGE_TOO_SMALL = b"\x01" * (VID_VERIFICATION_CLIENT_CHALLENGE_SIZE_BYTES - 1)
            CHALLENGE_TOO_BIG = b"\x02" * (VID_VERIFICATION_CLIENT_CHALLENGE_SIZE_BYTES + 1)

            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SignVIDVerificationRequest(fabricIndex=cr2_fabric_index, clientChallenge=CHALLENGE_TOO_SMALL))

            asserts.assert_equal(exception_context.exception.status, Status.ConstraintError,
                                f"Expected CONSTRAINT_ERROR from SignVIDVerificationRequest with ClientChallenge of length {len(CHALLENGE_TOO_SMALL)}")

            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SignVIDVerificationRequest(fabricIndex=cr2_fabric_index, clientChallenge=CHALLENGE_TOO_BIG))

            asserts.assert_equal(exception_context.exception.status, Status.ConstraintError,
                                f"Expected CONSTRAINT_ERROR from SignVIDVerificationRequest with ClientChallenge of length {len(CHALLENGE_TOO_BIG)}")

        with test_step(description="Invoke SetVIDVerificationStatement with VVSC, VIDVerificationStatement and VID fields all missing, outside fail-safe. Expect INVALID_COMMAND"):
            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SetVIDVerificationStatement())
            asserts.assert_equal(exception_context.exception.status, Status.InvalidCommand,
                                "Expected INVALID_COMMAND for SetVIDVerificationStatement with no arguments present")

        with test_step(description="Invoke SetVIDVerificationStatement with VVSC field set to a size > 400 bytes, outside fail-safe. Expect CONSTRAINT_ERROR"):
            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SetVIDVerificationStatement(vvsc=(b"\x01" * 401)))
            asserts.assert_equal(exception_context.exception.status, Status.ConstraintError,
                                "Expected CONSTRAINT_ERROR for SetVIDVerificationStatement with VVSC too large")

        with test_step(description="Invoke SetVIDVerificationStatement with VIDVerificationStatement field set to a size > 85 bytes, outside fail-safe. Expect CONSTRAINT_ERROR"):
            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SetVIDVerificationStatement(VIDVerificationStatement=(b"\x01" * (VID_VERIFICATION_STAMEMENT_SIZE_BYTES_V1 + 1))))
            asserts.assert_equal(exception_context.exception.status, Status.ConstraintError,
                                "Expected CONSTRAINT_ERROR for SetVIDVerificationStatement with VIDVerificationStatement too large")

        with test_step(description="Invoke SetVIDVerificationStatement with maximum-sized VVSC and VIDVerificationStatement present and setting VID to 0x6a01 on TH2's fabric, outside fail-safe. Verify VIDVerificationStatement, VVSC and VID updates are correct."):
            vvsc = b"\xaa" * 400
            VIDVerificationStatement = b"\x01" * VID_VERIFICATION_STAMEMENT_SIZE_BYTES_V1
            vendorID = 0x6a01

            await self.send_single_cmd(dev_ctrl=cr2_dev_ctrl, node_id=cr2_dut_node_id, cmd=opcreds.Commands.SetVIDVerificationStatement(VIDVerificationStatement=VIDVerificationStatement, vvsc=vvsc, vendorID=vendorID))

        with test_step(description="Invoke SetVIDVerificationStatement with maximum-sized VVSC on TH1's fabric, outside fail-safe. Verify INVALID_COMMAND due to presence of ICAC."):
            vvsc = b"\xaa" * 400

            with asserts.assert_raises(InteractionModelError) as exception_context:
                await self.send_single_cmd(cmd=opcreds.Commands.SetVIDVerificationStatement(vvsc=vvsc))
            asserts.assert_equal(exception_context.exception.status, Status.InvalidCommand,
                                "Expected INVALID_COMMAND for SetVIDVerificationStatement with VVSC present against DUT on TH1's fabric due to presence of ICAC.")

        with test_step(description="Invoke SetVIDVerificationStatement with setting VID to 0xFFF1 on TH2's fabric, outside fail-safe. Verify VID is now 0xFFF1."):
            do_nothing = None # Can't use pass in with test_step.

        with test_step(description="Arm a fail safe for 60s."):
            do_nothing = None # Can't use pass in with test_step.

        with test_step(description="Invoke SetVIDVerificationStatement with VVSC and VIDVerificationStatement to empty, and VID set to 0xFFF3 on TH2's fabric, inside fail-safe. Verify VVSC, VIDVerificationStamtement are now empty and VID is 0xFFF3 for that fabric."):
            do_nothing = None # Can't use pass in with test_step.

        with test_step(description="Disarm fail safe with ArmFailSafe(0s). Verify VVSC and VIDVerificatioStatement for TH1's fabric are still empty, and VID is still 0xFFF3."):
            do_nothing = None # Can't use pass in with test_step.

        with test_step(description="Create a new fabric under TH2's root with fabric ID 0x3333 by invoking ArmFailSafe(600s), CSRRequest, AddTrustedRootCertificate and AddNOC. Do not disarm failsafe, do not execute commissioning complete."):
            do_nothing = None # Can't use pass in with test_step.

        with test_step(description="Invoke SetVIDVerificationStatement with VVSC and VIDVerificationStatement present and setting VID to on fabric ID 0x3333 under TH2's root, inside fail-safe. Verify VIDVerificationStatement, VVSC and VID values match values set."):
            do_nothing = None # Can't use pass in with test_step.

        with test_step(description="Disarm failsafe with ArmFailSafe(0s). Verify that fabric table no longer has VVSC and VIDVerificatioNStatement for the pending fabric that was dropped.") as step:
            step.skip()

        with test_step(description="Remove TH2's fabric"):
            cmd = opcreds.Commands.RemoveFabric(cr2_fabric_index)
            resp = await self.send_single_cmd(cmd=cmd)
            asserts.assert_equal(
                resp.statusCode, opcreds.Enums.NodeOperationalCertStatusEnum.kOk)

if __name__ == "__main__":
    default_matter_test_main()
