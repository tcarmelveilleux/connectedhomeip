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
import base64
import copy
from dataclasses import dataclass, asdict, field
from enum import StrEnum, auto
import json
import logging
import queue
import sys
import time
import pickle
from typing import Any, Optional, Union

from pprint import pprint
from threading import Event

from chip import discovery
import chip.clusters as Clusters

from chip.setup_payload import SetupPayload
from chip.clusters import ClusterObjects as ClustersObjects
from chip.clusters.Attribute import SubscriptionTransaction, TypedAttributePath, ValueDecodeFailure
from chip.exceptions import ChipStackError
from chip.utils import CommissioningBuildingBlocks

import chip.tlv
from matter_testing_support import MatterBaseTest, async_test_body, default_matter_test_main

from mobly import asserts


@dataclass
class AttributePathLocation:
    endpoint_id: int
    cluster_id: Optional[int] = None
    attribute_id: Optional[int] = None


@dataclass
class EventPathLocation:
    endpoint_id: int
    cluster_id: int
    event_id: int


@dataclass
class CommandPathLocation:
    endpoint_id: int
    cluster_id: int
    command_id: int


class ProblemSeverity(StrEnum):
    NOTE = auto()
    WARNING = auto()
    ERROR = auto()


@dataclass
class ProblemNotice:
    test_name: str
    location: Union[AttributePathLocation, EventPathLocation, CommandPathLocation]
    severity: ProblemSeverity
    problem: str
    spec_location: str = ""


def MatterTlvToJson(tlv_data: dict[int, Any]) -> dict[str, any]:
    """Given TLV data for a specific cluster instance, convert to the Matter JSON format."""

    matter_json_dict = {}

    key_type_mappings = {
        chip.tlv.uint: "UINT",
        int: "INT",
        bool: "BOOL",
        list: "ARRAY",
        dict: "STRUCT",
        chip.tlv.float32: "FLOAT",
        float: "DOUBLE",
        bytes: "BYTES",
        str: "STRING",
        ValueDecodeFailure: "ERROR",
        type(None): "NULL",
    }

    def ConvertValue(value) -> Any:
        value_type = type(value)

        if value_type == ValueDecodeFailure:
            raise ValueError(f"Bad Value: {str(value)}")

        if value_type == bytes:
            return base64.b64encode(value).decode("UTF-8")
        elif value_type == list:
            for idx, item in enumerate(value):
                value[idx] = ConvertValue(item)
        elif value_type == dict:
            value = MatterTlvToJson(value)

        return value

    for key in tlv_data:
        value_type = type(tlv_data[key])
        value = copy.deepcopy(tlv_data[key])

        element_type: str = key_type_mappings[value_type]
        sub_element_type = ""

        new_key = ""
        if element_type:
            if sub_element_type:
                new_key = f"{str(key)}:{element_type}-{sub_element_type}"
            else:
                new_key = f"{str(key)}:{element_type}"
        else:
            new_key = str(key)

        try:
            new_value = ConvertValue(value)
        except ValueError as e:
            new_value = str(e)

        if element_type:
            if element_type == "ARRAY":
                if len(new_value):
                    sub_element_type = key_type_mappings[type(tlv_data[key][0])]
                else:
                    sub_element_type = "?"

        matter_json_dict[new_key] = new_value

    return matter_json_dict


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
    async def setup_class(self):
        dev_ctrl = self.default_controller

        if self.matter_test_config.qr_code_content is not None:
            qr_code = self.matter_test_config.qr_code_content
            try:
                setup_payload = SetupPayload().ParseQrCode(qr_code)
            except ChipStackError:
                asserts.fail(f"QR code '{qr_code} failed to parse properly as a Matter setup code.")

        elif self.matter_test_config.manual_code is not None:
            manual_code = self.matter_test_config.manual_code
            try:
                setup_payload = SetupPayload().ParseManualPairingCode(manual_code)
            except ChipStackError:
                asserts.fail(
                    f"Manual code code '{manual_code}' failed to parse properly as a Matter setup code. Check that all digits are correct and length is 11 or 21 characters.")
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
        # TODO: Support BLE
        if commissionable_nodes is not None and len(commissionable_nodes) > 0:
            commissionable_node = commissionable_nodes[0]
            instance_name = f"{commissionable_node.instanceName}._matterc._udp.local"
            vid = f"{commissionable_node.vendorId}"
            pid = f"{commissionable_node.productId}"
            address = f"{commissionable_node.addresses[0]}"
            logging.info(f"Found instance {instance_name}, VID={vid}, PID={pid}, Address={address}")

            node_id = 1
            dev_ctrl.EstablishPASESessionIP(address, setup_payload.setup_passcode, node_id)
        else:
            asserts.fail("Failed to find the DUT according to command line arguments.")

        wildcard_read = (await dev_ctrl.Read(node_id, [()]))
        endpoints_tlv = wildcard_read.tlvAttributes

        node_dump_dict = {endpoint_id: MatterTlvToJson(endpoints_tlv[endpoint_id]) for endpoint_id in endpoints_tlv}
        logging.info(f"Raw contents of Node: {json.dumps(node_dump_dict, indent=2)}")

        ########### State kept for use by all tests ###########

        # List of accumulated problems across all tests
        self.problems = []

        # All endpoints in "full object" indexing format
        self.endpoints = wildcard_read.attributes

        # All endpoints in raw TLV format
        self.endpoints_tlv = wildcard_read.tlvAttributes

    def teardown_class(self):
        """Final teardown after all tests: log all problems"""
        if len(self.problems) == 0:
            return

        logging.info("Problems found:")
        for problem in self.problems:
            logging.info(f"- {json.dumps(asdict(problem))}")

    def get_test_name(self) -> str:
        """Return the function name of the caller. Used to create logging entries."""
        return sys._getframe().f_back.f_code.co_name

    def record_error(self, test_name: str, location: Union[AttributePathLocation, EventPathLocation, CommandPathLocation], problem: str, spec_location: str = ""):
        self.problems.append(ProblemNotice(test_name, location, ProblemSeverity.ERROR, problem, spec_location))

    def record_warning(self, test_name: str, location: Union[AttributePathLocation, EventPathLocation, CommandPathLocation], problem: str, spec_location: str = ""):
        self.problems.append(ProblemNotice(test_name, location, ProblemSeverity.WARNING, problem, spec_location))

    def record_note(self, test_name: str, location: Union[AttributePathLocation, EventPathLocation, CommandPathLocation], problem: str, spec_location: str = ""):
        self.problems.append(ProblemNotice(test_name, location, ProblemSeverity.NOTE, problem, spec_location))

    def fail_current_test(self, msg: Optional[str] = None):
        if not msg:
            asserts.fail(msg=self.problems[-1].problem)
        else:
            asserts.fail(msg)

    @async_test_body
    async def test_endpoint_zero_present(self):
        logging.info("Validating that the Root Node endpoint is present (EP0)")
        if not 0 in self.endpoints:
            self.record_error(self.get_test_name(), location=AttributePathLocation(endpoint_id=0),
                              problem="Did not find Endpoint 0.", spec_location="Endpoint Composition")
            self.fail_current_test()

    @async_test_body
    async def test_descriptor_present_on_each_endpoint(self):
        logging.info("Validating each endpoint has a descriptor cluster")

        success = True
        for endpoint_id, endpoint in self.endpoints.items():
            has_descriptor = (Clusters.Descriptor in endpoint)
            logging.info(f"Checking descriptor on Endpoint {endpoint_id}: {'found' if has_descriptor else 'not_found'}")
            if not has_descriptor:
                self.record_error(self.get_test_name(), location=AttributePathLocation(endpoint_id=endpoint_id, cluster_id=Clusters.Descriptor.id),
                                  problem=f"Did not find a descriptor on endpoint {endpoint_id}", spec_location="Base Cluster Requirements for Matter")
                success = False

        if not success:
            self.fail_current_test("At least one endpoint was missing the descriptor cluster.")

    # @async_test_body
    # async def test_accepted_commands_attribute_present_on_each_cluster(self):
    #     logging.info("Validating each cluster has the accepted_commands global attribute")

    #     ACCEPTED_COMMAND_LIST_ID = 0x0000FFF9

    #     success = True
    #     for endpoint_id, endpoint in self.endpoints_tlv.items():
    #         for cluster_id, cluster in endpoint.items():
    #             location = AttributePathLocation(endpoint_id, cluster_id, ACCEPTED_COMMAND_LIST_ID)

    #             has_attribute = ()
    #             logging.info(f"Checking descriptor on Endpoint {endpoint_id}: {'found' if has_descriptor else 'not_found'}")
    #             if not has_descriptor:
    #                 self.record_error(self.get_test_name(), location=AttributePathLocation(endpoint_id=endpoint_id, cluster_id=Clusters.Descriptor.id),
    #                                 problem=f"Did not find a descriptor on endpoint {endpoint_id}", spec_location="Base Cluster Requirements for Matter")
    #                 success = False

    #     if not success:
    #         self.fail_current_test("At least one endpoint was missing the descriptor cluster.")


if __name__ == "__main__":
    default_matter_test_main()
