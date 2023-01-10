# Copyright (c) 2022 Tennessee Carmel-Veilleux
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


import math
import mido
import queue
import serial
import sys
import threading
import time
import traceback
import chip.clusters as Clusters
import asyncio

EXITCODE_NO_PORT = 1


class Esp32MatterMidiPort(mido.ports.BaseOutput):
  def __init__(self, dev_ctrl, node_id, out_queue, channel_number=0, transpose=0):
    super().__init__(self)

    self._dev_ctrl = dev_ctrl
    self._node_id = node_id
    self._q = queue.Queue()
    self._out_queue = out_queue
    self._running = False
    self._thread = threading.Thread(target=self._message_loop, name=f"esp32_matter_midi_{node_id}")
    self._port = None
    self._last_time = 0.0
    self._channel_number = channel_number
    self._transpose = transpose

    self._thread.start()

  def _send(self, message):
    self._q.put(message)

  def _close(self):
    self._q.put(None)
    if self._thread.is_alive():
      self._thread.join()

  def _message_loop(self):
    self._running = True
    while self._running:
      message = self._q.get()
      if message is None:
        self._running = False
        continue

      print(message)

      command_byte, note, velocity = 0, 0, 0
      kChannel = 0
      on_delay_10ms = 6
      off_delay_10ms = 4

      if message.type == "note_on":
          # Use a TestEventTrigger for Note On
          kNoteOn = 0x90
          command_byte = kNoteOn | kChannel
          note, velocity = message.note, message.velocity

          # Don't care about 0 velocity: they are note-off, we won't send for xylophone
          if velocity == 0:
            command_byte = 0

      # elif message.type == "note_off":
      #     kNoteOff = 0x80
      #     command_byte = kNoteOff | kChannel
      #     note, velocity = message.note, message.velocity

      if command_byte != 0:
          note = note + self._transpose
          event_trigger = 0xFFFF_FFFF_0000_0000 | (command_byte << 24) | (note << 16) | (velocity << 8) | ((on_delay_10ms & 0xf) << 4) | ((off_delay_10ms & 0xf) << 0)
          enableKey = b"Jingle The Bells"
          self._out_queue.put(self._dev_ctrl.SendCommand(self._node_id, endpoint=0, payload=Clusters.GeneralDiagnostics.Commands.TestEventTrigger(enableKey=enableKey, eventTrigger=event_trigger)))

      self._last_time = message.time


class MidiProcessor(object):
  def __init__(self, input_port, output_port, midi_file, bpm=80):
    self._input_port = input_port
    self._output_port = output_port
    self._midi_file = midi_file
    self._tempo = mido.bpm2tempo(bpm)

    self._shutdown_event = threading.Event()

    self._input_q = queue.Queue()

    self._input_loop_thread = threading.Thread(target=self._input_loop, name="input loop")
    self._process_thread = threading.Thread(target=self._process_loop, name="playback loop")

  def start(self):
    self._input_loop_thread.start()
    self._process_thread.start()

  def shutdown(self):
    self._shutdown_event.set()
    self._process_thread.join()

  def _input_loop(self):
    while not self._shutdown_event.is_set():
      if self._input_port is None:
        time.sleep(1.0)
        continue

      message = self._input_port.poll()
      if message is None:
        time.sleep(0.01)
      else:
        self._input_q.put(message)

  def _process_loop(self):
    try:
      if True:
        if self._midi_file is not None:
          midi_file = mido.MidiFile(self._midi_file)
          #print(midi_file.print_tracks())
          music_track_number = None
          for track_number, track in enumerate(midi_file.tracks):
            for msg in track:
              # Find first track with notes
              if msg.type == "note_on" and (music_track_number is None):
                music_track_number = track_number
                break

          if music_track_number is not None:
            for msg in midi_file.tracks[music_track_number]:
              #print(msg.time)
              message_time = mido.tick2second(msg.time, midi_file.ticks_per_beat, self._tempo)
              time.sleep(message_time)

              msg.time = message_time
              #print(msg)
              if not msg.is_meta:
                self._output_port.send(msg)

              if self._shutdown_event.is_set():
                break

      if False:
        while not self._shutdown_event.is_set():
          try:
            message = self._input_q.get(block=True, timeout=0.01)
          except queue.Empty:
            continue

          self._output_port.send(message)

      # Basic xylophone scale to test addressing
      if False:
        note_idx = 0
        while not self._shutdown_event.is_set():
          try:
            message = mido.Message('note_on', note=63 + note_idx, velocity=64, time=0.1)
            #message = self._input_q.get(block=True, timeout=0.01)
          except queue.Empty:
            continue

          self._output_port.send(message)
          time.sleep(1.0)
          note_idx += 1
          if note_idx == 14:
            note_idx = 0

    except KeyboardInterrupt:
      self._shutdown_event.set()
    except Exception as e:
      print(e)
      traceback.print_exc()

    if self._input_loop_thread.is_alive():
      self._input_loop_thread.join()
    self._input_port.close()
    self._output_port.close()


def main():
  if len(sys.argv) < 2:
    print("Available ports:")
    for port_name in mido.get_input_names():
      print(" - '%s'" % port_name)

    return EXITCODE_NO_PORT

  port_name = sys.argv[1]

  input_port = mido.open_input(port_name)
  #output_port = PrologixHp3314Port("COM3")

  processor = MidiProcessor(input_port, output_port, midi_file="JingleBells.mid")
  processor.start()

  try:
    while True:
      time.sleep(1.0)
  finally:
    processor.shutdown()

  return 0

if __name__ == "__main__":
  sys.exit(main())
