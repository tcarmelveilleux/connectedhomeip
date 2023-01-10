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
import logging
from mobly import asserts
from esp32_matter_midi import Esp32MatterMidiPort, MidiProcessor
import asyncio
import queue

class JingleBellsTest(MatterBaseTest):
    @async_test_body
    async def test_play_jingle_bells_with_matter(self):
        dev_ctrl = self.default_controller
        vendor_name = await self.read_single_attribute(dev_ctrl, self.dut_node_id, 0, Clusters.BasicInformation.Attributes.VendorName)

        logging.info("Found VendorName: %s" % (vendor_name))
        asserts.assert_equal(vendor_name, "TEST_VENDOR", "VendorName must be TEST_VENDOR!")

        #input_port = mido.open_input(port_name)
        #output_port = PrologixHp3314Port("COM3")
        matter_commands = queue.Queue()

        esp32_midi_port = Esp32MatterMidiPort(dev_ctrl, self.dut_node_id, out_queue=matter_commands, channel_number=0, transpose=-3)
        processor = MidiProcessor(input_port=None, output_port=esp32_midi_port, midi_file="otannenbaum.mid", bpm=90)
        processor.start()

        while True:
          command = matter_commands.get()
          await command


if __name__ == "__main__":
    default_matter_test_main()
