import queue
import time
from enum import IntEnum
from typing import Optional
from threading import Thread, main_thread, current_thread

class Status(IntEnum):
  NO_ERROR = 0
  INVALID_STATE = 1
  RESOURCES_EXHAUSTED = 2

class Event:
  pass

class TimerDoneEvent(Event):
  def __init__(self, callback: callable, context: any):
    self.callback = callback
    self.context = context

class StartTimerEvent(Event):
  def __init__(self, expiry_ms: int, callback: callable, context: any):
    self.expiry_ms = expiry_ms
    self.callback = callback
    self.context = context

class RunWorkEvent(Event):
  def __init__(self, callback: callable, context: any):
    self.callback = callback
    self.context = context

class BlockReceivedEvent(Event):
  def __init__(self, block_data: bytes):
    pass

class RequestorWorkItem:
  TRIGGER_IMMEDIATE = 0
  CANCEL_OTA = 1
  SEND_QUERY_IMAGE_REQUEST = 2
  SEND_APPLY_UPDATE_REQUEST = 3
  SEND_UPDATE_APPLIED = 4

  def __init__(self, work_item_type: int):
    self.work_item_type = work_item_type


class Platform:
  def __init__(self):
    self._queue = queue.Queue()
    self._last_time_ms = 0
    self._elapsed_ms = 0
    self._ota_requestor = None
    self._timers = []
    self._running = True

  def post_event(self, event: Optional[Event]):
    self._queue.put(event)

  def schedule_work(self, callback: callable, context: any):
    self.post_event(RunWorkEvent(callback, context))

  def run_event_loop(self, event_handler: callable):
    self._last_time_ms = int(time.time() * 1000.0)

    while self._running:
      try:
        event = self._queue.get(block=True, timeout=0.1)

        if event is None:
          self._running = False
          break

        if isinstance(event, StartTimerEvent):
          self._handle_start_timer(event)
        else:
          event_handler(event)

      except queue.Empty:
        # Just time passed, with no events: ignore and keep going
        pass

      self.process_timers()

  def _find_timer(self, callback, context):
    for idx, timer in enumerate(self._timers):
      if timer["callback"] == callback and timer["context"] == context:
        return idx

    return None

  def _handle_start_timer(self, start_timer_event: StartTimerEvent):
    expiry_ms = start_timer_event.expiry_ms
    callback = start_timer_event.callback
    context = start_timer_event.context

    self.update_clocking()
    timer_entry = {
      "deadline": self._elapsed_ms + expiry_ms,
      "callback": callback,
      "context": context
    }

    idx = self._find_timer(callback, context)
    if idx is None:
      # New timer
      self._timers.append(timer_entry)
    else:
      # Replace timer
      self._timers[idx] = timer_entry

  def update_clocking(self):
    now = time.time()
    now_ms = int(now * 1000.0)
    self._elapsed_ms += (now_ms - self._last_time_ms)
    self._last_time_ms = now_ms
    print("%.3f: Elapsed_ms=%d" % (now, self._elapsed_ms))

  def fast_forward_clock_for_testing(self, num_ms: int):
    self.process_timers()
    self._elapsed_ms += num_ms

    # Reset "real time" delta to right now, so that further updates of clocking don't have
    # more bias.
    now_ms = int(time.time() * 1000.0)
    self._last_time_ms = now_ms

  def process_timers(self):
    self.update_clocking()

    to_delete = []
    for idx, timer in enumerate(self._timers):
      if self._elapsed_ms >= timer["deadline"]:
        timer["callback"](timer["context"])
        to_delete.append(idx)

    # Delete expired timers starting by the end, so that deletion of
    # a timer by index doesn't cause indices left in the list to relate
    # to different entries
    for idx in reversed(to_delete):
      del self._timers[idx]

  def start_timer(self, expiry_ms: int, callback: callable, context: any):
    self.post_event(StartTimerEvent(expiry_ms, callback, context))

  def shutdown(self):
    self.post_event(None)


class SessionEstablisher:
  """Fake session establishment logic"""
  def __init__(self, platform: Platform):
    self._platform = platform
    self._should_fail = False

  def set_should_fail(self, should_fail: bool):
    self._should_fail = should_fail

  def _handle_success(self, context):
    """Just a trampoline to log"""
    on_success = context["on_success"]
    fabric_index = context["fabric_index"]
    node_id = context["node_id"]

    print("Succeeded to establish session to fabric_index:%d, node_id:%d" % (fabric_index, node_id))
    on_success(self._context)

  def establish_session(self, fabric_index, node_id, on_success: callable, on_failed: callable):
    if self._should_fail:
      print("Failed to establish session to fabric_index:%d, node_id:%d" % (fabric_index, node_id))
      self._platform.schedule_work(callback=on_failed, context=self._context)
      return

    context = {
      "on_success": on_success,
      "fabric_index": fabric_index,
      "node_id": node_id
    }

    kDelayForSimulationMs = 1000
    self._platform.start_timer(expiry_ms=kDelayForSimulationMs, callback=self._handle_success, context=context)

def app_event_handler(event: Event):
  if isinstance(event, RunWorkEvent):
    event.callback(event.context)
  elif isinstance(event, TimerDoneEvent):
    event.callback(event.context)
  elif isinstance(event, StartTimerEvent):
    # TODO: Handle starting a timer
    pass

class OtaContext:
  # Ongoing OTA context
  def __init__(self):
    self.in_progress = False
    self.provider_node_id = 0
    self.download_node_id = 0
    self.fabric_index = 0
    self.file_designator = ""
    ### TODO: Whatever else


### For data model handling abstraction
class QueryImageResponse:
  def __init__(self, download_node_id, file_designator, software_version):
    self.download_node_id = download_node_id
    self.file_designator = file_designator
    self.software_version = software_version

class ApplyUpdateResponse:
  pass

class AnnounceOtaProvider:
  pass

### For OTAImageProcessor
class BlockAction:
  BLOCK_ACTION_DONE = 0
  BLOCK_ACTION_NEXT_BLOCK = 1
  BLOCK_ACTION_NEXT_BLOCK_WITH_SKIP = 2

  def __init__(self, type):
    self.type = type

class BlockActionDone(BlockAction):
  def __init__(self):
      super().__init__(BlockAction.BLOCK_ACTION_DONE)

class BlockActionNextBlock(BlockAction):
  def __init__(self):
      super().__init__(BlockAction.BLOCK_ACTION_NEXT_BLOCK)

class BlockActionNextBlock(BlockAction):
  def __init__(self, next_offset):
      super().__init__(BlockAction.BLOCK_ACTION_NEXT_BLOCK_WITH_SKIP)
      self.next_offset = next_offset


### For processing download
class OtaImageProcessor:
  def get_start_offset(self) -> int:
    raise NotImplementedError

  def get_next_block_action(self) -> BlockAction:
    raise NotImplementedError

  def handle_block(block: bytes):
    raise NotImplementedError

### For adapting a platform's OTA backend to the OTA Requestor logic
class OtaRequestorDriver(OtaImageProcessor):
  def __init__(self, platform: Platform, session_establisher: SessionEstablisher) -> None:
      self._platform = platform
      self._session_establisher = session_establisher

  def schedule_requestor_work(self, work_item: RequestorWorkItem):
    pass

  def start_query_timer(self, duration_ms: int) -> Status:
    pass

  def start_delayed_action_timer(self, duration_ms: int) -> Status:
    pass

  # TODO: Account for timer cancels?

  def establish_provider_session(self, fabric_index: int, node_id: int):
    pass

  def establish_bdx_session(self, fabric_index: int, node_id: int):
    pass

  def close_provider_session(self, fabric_index: int, node_id: int):
    pass

  def prepare_ota(self) -> Status:
    pass

  def load_stored_ota_context(self, ota_context: OtaContext) -> Status:
    return Status.NO_ERROR

  def clear_stored_ota_context(self):
    pass

  def set_progress(self, progress_percent: int):
    pass

  def set_ota_state(self, ota_state: int):
    pass

  # For DownloadError event
  def record_download_error(self, *args):
    pass

  # For StateTransition event
  def record_state_transition(self, *args):
    pass

  # For VersionApplied event
  def record_version_applied(self, *args):
    pass

  # TODO: Handle storge of OTA Providers list

class OtaRequestorInterface:
  def __init__(self, ota_requestor_driver: OtaRequestorDriver, image_processor: OtaImageProcessor):
    self._ota_requestor_driver = ota_requestor_driver
    self._image_processor = image_processor
    self._ota_context = OtaContext()

  def Initialize(self) -> Status:
    # Must be called before any Matter event processed

    # Try to load prior state
    status =  self._ota_requestor_driver.load_stored_ota_context(self._ota_context)

    # Clear everything on error.
    if status != Status.NO_ERROR:
      self._ota_requestor_driver.clear_stored_ota_context()
      self._ota_context = OtaContext()

    # TODO: All other init
    return status

  def trigger_immediate_query(self):
    pass

  def cancel_ongoing_ota(self):
    pass

  # Call-ins to be done by platform/application
  def on_trigger_immediate_query(self):
    pass

  def on_cancel_ongoing_ota(self):
    pass

  def on_query_timer_hit(self):
    pass

  def on_delayed_action_timer_hit(self):
    pass

  def on_provider_session_established(self, context:any):
    pass

  def on_provider_session_error(self):
    pass

  def on_bdx_session_established(self, context:any):
    pass

  def on_bdx_session_error(self, context:any):
    pass

  def on_query_image_response(self, response: QueryImageResponse):
    pass

  def on_query_image_failed(self, status_code: int):
    pass

  def on_apply_update_response(self, response: ApplyUpdateResponse):
    pass

  def on_apply_request_failed(self, status_code: int):
    pass

  def on_announce_ota_provider(self, response: AnnounceOtaProvider):
    pass

  def on_bdx_error(self, status_code: int):
    pass

  def on_bdx_block(self, block_content: bytes):
    # TODO: Call into image_processor
    pass

  def on_ota_prepared(self, status_code: int):
    pass

  def on_ota_applied(self):
    pass


class OtaRequestor(OtaRequestorInterface):
  STATE_IDLE = 0
  STATE_QUERYING = 1
  STATE_WAITING_FOR_NEXT_QUERY = 2
  STATE_DOWNLOADING = 3
  STATE_REQUESTING_APPLY = 4
  STATE_APPLYING = 5
  STATE_BOOTING_INTO_NEW = 6
  STATE_ESTABLISHING_PROVIDER_CONNECTION = 7
  STATE_ESTABLISHING_BDX_CONNECTION = 8

  def __init__(self, ota_requestor_driver: OtaRequestorDriver, image_processor: OtaImageProcessor):
    self._image_processor = image_processor
    self._ota_requestor_driver = ota_requestor_driver
    self._state = self.STATE_IDLE

  def ensure_proper_thread(self):
    if current_thread.name != "event_loop":
      raise RuntimeError("Running from %s instead of event_loop" % current_thread.name)

  def on_trigger_immediate_query(self):
    print("on_trigger_immediate_query")
    if self._state != self.STATE_IDLE:
      print("ERROR: request to trigger immediate query when not idle!")
      return

    self._state = self.STATE_ESTABLISHING_PROVIDER_CONNECTION

    #TODO: How is this conveyed/determined?
    fabric_index = 1
    node_id = 1234

    self._ota_requestor_driver.establish_provider_session(fabric_index, node_id)

  def on_provider_session_established(self, context: any):
    print("on_provider_session_established")
    self.ensure_proper_thread()

    if self._state != self.STATE_ESTABLISHING_PROVIDER_CONNECTION:
      print("ERROR: Got unexpected session establishment call-in!")
      # TODO: Clean-up
      return

    self._ota_requestor_driver.schedule_requestor_work(RequestorWorkItem.SEND_QUERY_IMAGE_REQUEST)

  def on_query_image_response(self, response: QueryImageResponse):
    print("on_query_image_response")
    self.ensure_proper_thread()

  def on_query_image_failed(self, status_code: int):
    pass

  def establish_provider_session(self, fabric_index: int, node_id: int):
    self._ota_requestor_driver.establish_provider_session(fabric_index, node_id)

  def close_provider_session(self, fabric_index: int, node_id: int):
    self._ota_requestor_driver.close_provider_session(fabric_index, node_id)

  def send_query_image(self):
    self._ota_requestor_driver.send_query_image()

def start_ota():
  pass

def resume_ota():
  pass

def process_announce():
  pass

def run_ota_process():
  global platform
  platform = Platform()

  main_thread.name = "app"

  event_loop_thread = Thread(target=platform.run_event_loop, name="event_loop", args=(app_event_handler,), daemon=False)
  event_loop_thread.start()

  session_establisher = SessionEstablisher(platform)

  ota_requestor_driver = OtaRequestorDriver(platform, session_establisher)
  ota_image_processor = OtaImageProcessor()

  ota_requestor = OtaRequestor(ota_requestor_driver, ota_image_processor)
  ota_requestor.trigger_immediate_query()

  def timer_callback(context):
    print("%.3f: Timer expired: %s" % (time.time(), context))
    platform.start_timer(expiry_ms=2000, callback=timer_callback, context="TimerN")

  platform.start_timer(expiry_ms=1000, callback=timer_callback, context="Timer1")

  try:
    while True:
      # More logic occurs here
      time.sleep(1.0)
  except KeyboardInterrupt:
    platform.shutdown()
    event_loop_thread.join()
    print("Event loop done")


def main():
  run_ota_process()
  # CASE

if __name__ == "__main__":
  main()