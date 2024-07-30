#
#    Copyright (c) 2024 Project CHIP (Matter) Authors
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

import logging

from chip import ChipDeviceCtrl
import chip.clusters as Clusters
from chip.clusters.Types import NullValue
from matter_testing_support import MatterBaseTest, ClusterAttributeChangeAccumulator, TestStep, async_test_body, default_matter_test_main
from mobly import asserts


class TC_OCC_3_2(MatterBaseTest):
    async def read_occ_attribute_expect_success(self, endpoint, attribute):
        cluster = Clusters.Objects.OccupancySensing
        return await self.read_single_attribute_check_success(endpoint=endpoint, cluster=cluster, attribute=attribute)

    def _await_sequence_of_reports(self, report_queue: queue.Queue, endpoint_id: int, attribute: TypedAttributePath, sequence: list[Any], timeout_sec: float):
        start_time = time.time()
        elapsed = 0.0
        time_remaining = timeout_sec

        sequence_idx = 0
        actual_values = []

        while time_remaining > 0:
            expected_value = sequence[sequence_idx]
            logging.info(f"Expecting value {expected_value} for attribute {attribute} on endpoint {endpoint_id}")
            try:
                item: AttributeValue = report_queue.get(block=True, timeout=time_remaining)

                # Track arrival of all values for the given attribute.
                if item.endpoint_id == endpoint_id and item.attribute == attribute:
                    actual_values.append(item.value)

                    if item.value == expected_value:
                        logging.info(f"Got expected attribute change {sequence_idx+1}/{len(sequence)} for attribute {attribute}")
                        sequence_idx += 1
                    else:
                        asserts.assert_equal(item.value, expected_value,
                                             msg="Did not get expected attribute value in correct sequence.")

                    # We are done waiting when we have accumulated all results.
                    if sequence_idx == len(sequence):
                        logging.info("Got all attribute changes, done waiting.")
                        return
            except queue.Empty:
                # No error, we update timeouts and keep going
                pass

            elapsed = time.time() - start_time
            time_remaining = timeout_sec - elapsed

        asserts.fail(f"Did not get full sequence {sequence} in {timeout_sec:.1f} seconds. Got {actual_values} before time-out.")
    
    def desc_TC_OCC_3_2(self) -> str:
        return "[TC-OCC-3.2] Subscription Report Verification with server as DUT"

    def steps_TC_OCC_3_2(self) -> list[TestStep]:
        steps = [
            TestStep(1, "Commission DUT to TH if not already done", is_commissioning=True),
            TestStep(2, "TH establishes a wildcard subscription to all attributes on Occupancy Sensing Cluster on the endpoint under test. Subscription min interval = 0 and max interval = 30 seconds."),
            TestStep(3a, "Do not trigger DUT for occupancy state change."),
            TestStep(3b, "TH reads DUT Occupancy attribute and saves the initial value as initial"),
            TestStep(3c, "Trigger DUT to change the occupancy state."),
            TestStep(3d, "TH awaits a ReportDataMessage containing an attribute report for DUT Occupancy attribute."),
            TestStep(4a, "Check if DUT supports HoldTime attribute, If not supported, then stop and skip the rest of test cases."),
            TestStep(4b, "TH reads DUT HoldTime attribute and saves the initial value as initial"),
            TestStep(4c, "TH writes a different value to DUT HoldTime attribute."),
            TestStep(4d, "TH awaits a ReportDataMessage containing an attribute report for DUT HoldTime attribute."),
            TestStep(5a, "Check if DUT supports DUT feature flag PIR or OTHER, If not supported, then stop and skip to 6a."),
            TestStep(5b, "TH reads DUT PIROccupiedToUnoccupiedDelay attribute and saves the initial value as initial"),
            TestStep(5c, "TH writes a different value to DUT PIROccupiedToUnoccupiedDelay attribute."),
            TestStep(5d, "TH awaits a ReportDataMessage containing an attribute report for DUT PIROccupiedToUnoccupiedDelay attribute."),
            TestStep(6a, "Check if DUT supports DUT feature flag US, If not supported, then stop and skip to 7a."),
            TestStep(6b, "TH reads DUT UltrasonicOccupiedToUnoccupiedDelay attribute and saves the initial value as initial"),
            TestStep(6c, "TH writes a different value to DUT UltrasonicOccupiedToUnoccupiedDelay attribute."),
            TestStep(6d, "TH awaits a ReportDataMessage containing an attribute report for DUT UltrasonicOccupiedToUnoccupiedDelay attribute."),
            TestStep(7a, "Check if DUT supports DUT feature flag PHY, If not supported, terminate this test case."),
            TestStep(7b, "TH reads DUT PhysicalContactOccupiedToUnoccupiedDelay attribute and saves the initial value as initial"),
            TestStep(7c, "TH writes a different value to DUT PhysicalContactOccupiedToUnoccupiedDelay attribute."),
            TestStep(7d, "TH awaits a ReportDataMessage containing an attribute report for DUT PhysicalContactOccupiedToUnoccupiedDelay attribute.")            
        ]
        return steps

    def pics_TC_OCC_3_2(self) -> list[str]:
        pics = [
            "OCC.S",
        ]
        return pics

    @async_test_body
    async def test_TC_OCC_3_2(self):

        endpoint = self.user_params.get("endpoint", 1)
        endpoint_id = self.matter_test_config.endpoint
        node_id = self.matter_test_config.dut_node_ids[0]
        post_prompt_settle_delay_seconds = 10.0
        cluster = Clusters.Objects.OccupancySensing
        

        self.step(1)
        attributes = Clusters.OccupancySensing.Attributes
        attribute_list = await self.read_occ_attribute_expect_success(endpoint=endpoint, attribute=attributes.AttributeList)
        
        self.step(2)
        # min interval = 0, and max interval = 30 seconds
        attrib_listener = ClusterAttributeChangeAccumulator(Clusters.Objects.OccupancySensing)
        await attrib_listener.start(ChipDeviceCtrl, node_id, endpoint=endpoint_id)
            
        self.step(3a)
        self.wait_for_user_input(prompt_msg="Type any letter and press ENTER after DUT goes back to unoccupied state.")
        
        self.step(3b)
        if attributes.Occupancy.attribute_id in attribute_list:
            initial_dut = await self.read_occ_attribute_expect_success(endpoint=endpoint, attribute=attributes.Occupancy)
            asserts.assert_equal(initial_dut, 0, "Occupancy attribute is still detected state")

        self.step(3c)
        self.wait_for_user_input(prompt_msg="Type any letter and press ENTER after the sensor occupancy is triggered and its occupancy state changed.")
        
        self.step(3d)
        self._await_sequence_of_reports(report_queue=attrib_listener.attribute_queue, endpoint_id=endpoint_id, attribute=cluster.Attributes.Occupancy, sequence=[
                                        0, 1], timeout_sec=post_prompt_settle_delay_seconds)
        
        self.step(4a)
        if attributes.HoldTime.attribute_id not in attribute_list:
            logging.info("No HoldTime attribute supports. Terminate this test case")
            self.skip_all_remaining_steps("4b")
        self.step(4b)
        initial_dut = await self.read_occ_attribute_expect_success(endpoint=endpoint, attribute=attributes.HoldTime)
        
        self.step(4c)
        # write a different a HoldTime attibute
        diff_val = 12
        write_res = await ChipDeviceCtrl.WriteAttribute(node_id, [(endpoint, attributes.HoldTime(diff_val))])
        
        self.step(4d)
        self._await_sequence_of_reports(report_queue=attrib_listener.attribute_queue, endpoint_id=endpoint_id, attribute=cluster.Attributes.HoldTime, sequence=[
                                        initial_dut, diff_val], timeout_sec=post_prompt_settle_delay_seconds)
        
        self.step(5a)
        if (Clusters.OccupancySensing.Bitmaps.Feature.kPassiveInfrared | Clusters.OccupancySensing.Bitmaps.Feature.kOther) != 1:
            self.skip(5b)
            self.skip(5c)
            self.skip(5d)
            
        self.step(5b)    
        if attributes.PIROccupiedToUnoccupiedDelay.attribute_id in attribute_list:
            initial_dut = await self.read_occ_attribute_expect_success(endpoint=endpoint, attribute=attributes.PIROccupiedToUnoccupiedDelay)
        
        else:        
            logging.info("No PIROccupiedToUnoccupiedDelay attribute supports. Terminate this test case")
        
        self.step(5c)
        # write the new attribute value
        diff_val = 11
        write_res = await ChipDeviceCtrl.WriteAttribute(node_id, [(endpoint, attributes.PIROccupiedToUnoccupiedDelay(diff_val))])
        
        self.step(5d)
        self._await_sequence_of_reports(report_queue=attrib_listener.attribute_queue, endpoint_id=endpoint_id, attribute=cluster.Attributes.PIROccupiedToUnoccupiedDelay, sequence=[
                                        initial_dut, diff_val], timeout_sec=post_prompt_settle_delay_seconds)
                                        
        self.step(6a)
        if Clusters.OccupancySensing.Bitmaps.Feature.kUltrasonic != 1:
            self.skip(6b)
            self.skip(6c)
            self.skip(6d)
            
        self.step(6b)    
        if attributes.UltrasonicOccupiedToUnoccupiedDelay.attribute_id in attribute_list:
            initial_dut = await self.read_occ_attribute_expect_success(endpoint=endpoint, attribute=attributes.UltrasonicOccupiedToUnoccupiedDelay)
        
        else:        
            logging.info("No UltrasonicOccupiedToUnoccupiedDelay attribute supports. Terminate this test case")
        
        self.step(6c)
        # write the new attribute value
        diff_val = 14
        write_res = await ChipDeviceCtrl.WriteAttribute(node_id, [(endpoint, attributes.UltrasonicOccupiedToUnoccupiedDelay(diff_val))])
        
        self.step(6d)
        self._await_sequence_of_reports(report_queue=attrib_listener.attribute_queue, endpoint_id=endpoint_id, attribute=cluster.Attributes.UltrasonicOccupiedToUnoccupiedDelay, sequence=[
                                        initial_dut, diff_val], timeout_sec=post_prompt_settle_delay_seconds)
                                        
        self.step(7a)
        if Clusters.OccupancySensing.Bitmaps.Feature.kPhysicalContact != 1:
            self.skip(7b)
            self.skip(7c)
            self.skip(7d)
            
        self.step(7b)    
        if attributes.PhysicalContactOccupiedToUnoccupiedDelay.attribute_id in attribute_list:
            initial_dut = await self.t_success(endpoint=endpoint, attribute=attributes.PhysicalContactOccupiedToUnoccupiedDelay)
        
        else:        
            logging.info("No UltrasonicOccupiedToUnoccupiedDelay attribute supports. Terminate this test case")
        
        self.step(7c)
        # write the new attribute value
        diff_val = 9
        write_res = await ChipDeviceCtrl.WriteAttribute(node_id, [(endpoint, attributes.PhysicalContactOccupiedToUnoccupiedDelay(diff_val))])
        
        self.step(7d)
        self._await_sequence_of_reports(report_queue=attrib_listener.attribute_queue, endpoint_id=endpoint_id, attribute=cluster.Attributes.PhysicalContactOccupiedToUnoccupiedDelay, sequence=[
                                        initial_dut, diff_val], timeout_sec=post_prompt_settle_delay_seconds)                                        
