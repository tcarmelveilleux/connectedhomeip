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
from dataclasses import dataclass, field
import logging
import queue
import time
from typing import Any, Optional

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
class Cluster:
    id: int
    attributes: dict[str, dict[str,Any]]

@dataclass
class Endpoint:
    id: int
    clusters: dict[str, Cluster] = field(default_factory=dict)

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
                asserts.fail(f"Manual code code '{manual_code}' failed to parse properly as a Matter setup code. Check that all digits are correct and length is 11 or 21 characters.")
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
        else:
            asserts.fail("Failed to find the DUT according to command line arguments.")

        self.node_content = (await dev_ctrl.Read(node_id, [()])).tlvAttributes
        for endpoint in self.node_content:
            print(type(self.node_content[endpoint]))
            print(dir(self.node_content[endpoint]))
            print(f"EP{endpoint}: {MatterTlvToJson(self.node_content[endpoint])}")


    @async_test_body
    async def test_some_cool_stuff(self):
        pass

if __name__ == "__main__":
    default_matter_test_main()
