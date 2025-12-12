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

import asyncio
import enum
import logging
import os
import sys
import json
import queue
import time
import threading
from typing import Optional, Any, Callable, Union
from dataclasses import dataclass, asdict
from datetime import datetime, timezone

import nest_asyncio

from matter import ChipDeviceCtrl
import matter.clusters as Clusters
from matter.exceptions import ChipStackError
from matter.interaction_model import InteractionModelError, Status
from matter.testing.matter_testing import (AttributeValue, MatterBaseTest,
                                         TestStep, default_matter_test_main, has_command, run_if_endpoint_matches, async_test_body)
from matter.clusters import ClusterObjects as ClusterObjects
from matter.clusters.Attribute import EventReadResult, SubscriptionTransaction, TypedAttributePath, AttributePath

from mobly import asserts

nest_asyncio.apply()

DEFAULT_PARALLEL_CONNECTIONS = 1
DEFAULT_ATTEMPT_DELAY_SECONDS = 0.5
DEFAULT_ADMIN_NODE_ID = 112233
DEFAULT_GROUP_KEY_SET_ID = 1
SELECTED_SINGLE_NODE = None

# To match src/lib/support/TestGroupData.h:InitData
DEFAULT_GROUP_ID = 0x0103
DEFAULT_GROUP_EPOCH_KEY = bytes(bytearray([0xd1, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf]))

class MonitoringEvent:
    pass

@dataclass
class CaseConnectionEvent(MonitoringEvent):
    node_id: int
    timestamp: float

@dataclass
class SessionFailedEvent(MonitoringEvent):
    node_id: int
    timestamp: float

@dataclass
class SubscriptionEstablishedEvent(MonitoringEvent):
    node_id: int
    timestamp: float

@dataclass
class ResubscriptionAttemptedEvent(MonitoringEvent):
    node_id: int
    timestamp: float

@dataclass
class SubscriptionErrorEvent(MonitoringEvent):
    node_id: int
    timestamp: float
    error: int

@dataclass
class SubscriptionReportBeginEvent(MonitoringEvent):
    node_id: int
    timestamp: float

@dataclass
class SubscriptionReportEndEvent(MonitoringEvent):
    node_id: int
    timestamp: float

@dataclass
class SubscriptionDataEvent(MonitoringEvent):
    node_id: int
    timestamp: float
    path: TypedAttributePath
    value: Any
    # TODO: Add data

@dataclass
class ShutdownEvent(MonitoringEvent):
    pass

# For subscribing to nodes
@dataclass
class RequestNodeIdSubcribeEvent(MonitoringEvent):
    node_id: int

@dataclass
class CommandFromWebEvent(MonitoringEvent):
    data: dict

@dataclass
class LogLineEvent(MonitoringEvent):
    lines: list[str]

class MonitoringEventHandler:
    def on_event(self, event: MonitoringEvent):
        pass


class AttributeSubscriptionHandler:
    """Handle a subscription to a whole node."""
    def __init__(self, handler: MonitoringEventHandler):
        self._subscription = None
        self._handler = handler
        self._lock = threading.Lock()
        self._node_id: Optional[int] = None

    async def start(self, dev_ctrl, node_id: int, fabric_filtered: bool = True, min_interval_sec: int = 1, max_interval_sec: int = 120) -> Any:
        """This starts a wildcard subscription for attributes on the specified node_id."""
        self._node_id = node_id
        try:
            self._subscription = await dev_ctrl.ReadAttribute(
                nodeid=node_id,
                attributes=[()],
                reportInterval=(int(min_interval_sec), int(max_interval_sec)),
                fabricFiltered=fabric_filtered,
                keepSubscriptions=False,
                autoResubscribe=False
            )
        except InteractionModelError as im_error:
            self.on_subscription_error(error_encountered=im_error.status, transaction=None)
            return None
        except ChipStackError as chip_error:
            self.on_subscription_error(error_encountered=chip_error.err, transaction=None)
            return None

        if self._subscription is None:
            self.on_subscription_error(error_encountered=-1, transaction=None)
            return None

        self._subscription.SetAttributeUpdateCallback(self.on_attribute_update)
        # self._subscription.SetResubscriptionAttemptedCallback(self.on_resubscription_attempted)
        # self._subscription.SetResubscriptionSucceededCallback(self.on_resubscription_succeeded)
        self._subscription.SetErrorCallback(self.on_subscription_error)
        self._subscription.SetReportBeginCallback(self.on_report_begin)
        self._subscription.SetReportEndCallback(self.on_report_end)

        with self._lock:
            self._handler.on_event(SubscriptionEstablishedEvent(node_id=self._node_id, timestamp=time.time()))

        return self._subscription

    def on_attribute_update(self, path: TypedAttributePath, transaction: SubscriptionTransaction):
        """
        Callback invoked when an attribute repoort is received via subscription.

        It extracts tha value using the transaction object, wraps into an AttributeValue, enqueues it for later processing,
        and stores it in internal history for verification.

        Parameters:
            path (TypedAttributePath): Contains cluster and attribute metadata for the report.
            transaction (SubscriptionTransaction): Provides access to the actual reported value.
        """

        data = transaction.GetAttribute(path)
        value = AttributeValue(endpoint_id=path.Path.EndpointId, attribute=path.AttributeType,
                                value=data, timestamp_utc=datetime.now(timezone.utc))
        # logging.info(f"[AttributeSubscriptionHandler] Received attribute report: {path.AttributeType} = {data}")
        with self._lock:
            self._handler.on_event(SubscriptionDataEvent(node_id=self._node_id, timestamp=time.time(), path=path, value=value))

        # def on_resubscription_reattempted(self, transaction: SubscriptionTransaction, error_encountered: int, next_resubscribe_interval_msec: int):
        #     with self._lock:
        #         self._handler(ResubscriptionAttemptedEvent(node_id=self._node_id, timestamp=time.time()))

        # def on_resubscription_succeeded(self, transaction: SubscriptionTransaction):
        #     with self._lock:
        #         self._handler(SubscriptionEstablishedEvent(node_id=self._node_id, timestamp=time.time()))

    def on_subscription_error(self, error_encountered: int, transaction: SubscriptionTransaction):
        with self._lock:
            self._handler.on_event(SubscriptionErrorEvent(node_id=self._node_id, timestamp=time.time(), error=error_encountered))

    def on_report_begin(self, transaction: SubscriptionTransaction):
        with self._lock:
            self._handler.on_event(SubscriptionReportBeginEvent(node_id=self._node_id, timestamp=time.time()))

    def on_report_end(self, transaction: SubscriptionTransaction):
        with self._lock:
            self._handler.on_event(SubscriptionReportEndEvent(node_id=self._node_id, timestamp=time.time()))

    @property
    def subscription(self):
        return self._subscription

@dataclass
class SubscriptionAttemptCompleteEvent(MonitoringEvent):
    node_id: int
    # A tuple of the returned subscription object and the handler that created it
    result: Optional[tuple[Any, AttributeSubscriptionHandler]]
    exception: Optional[Exception] = None

@dataclass
class OnboardedNode:
    name: str
    node_id: int

@dataclass
class LightEndpoint:
    name: str
    node_id: int
    endpoint_id: int

@dataclass
class SwitchEndpoint:
    name: str
    node_id: int
    endpoint_id: int


class DeviceConfigRepository:
    def __init__(self, filename: str):
        self.filename = filename
        self.nodes: list[OnboardedNode] = []
        self.light_endpoints: list[LightEndpoint] = []
        self.switch_endpoints: list[SwitchEndpoint] = []
        self._load()

    def _load(self):
        if os.path.exists(self.filename):
            try:
                with open(self.filename, 'r') as f:
                    data = json.load(f)
                    self.nodes = [OnboardedNode(**node_data) for node_data in data.get('nodes', [])]
                    self.light_endpoints = [LightEndpoint(**light_data) for light_data in data.get('light_endpoints', [])]
                    self.switch_endpoints = [SwitchEndpoint(**switch_data) for switch_data in data.get('switch_endpoints', [])]
            except (json.JSONDecodeError, IOError) as e:
                logging.error(f"Could not load device manager file {self.filename}: {e}")
                # Start with empty lists if file is corrupt or unreadable
                self.nodes = []
                self.light_endpoints = []

    def _save(self):
        try:
            with open(self.filename, 'w') as f:
                data = {
                    'nodes': [asdict(node) for node in self.nodes],
                    'light_endpoints': [asdict(light) for light in self.light_endpoints],
                    'switch_endpoints': [asdict(switch) for switch in self.switch_endpoints],

                }
                json.dump(data, f, indent=4)
        except IOError as e:
            logging.error(f"Could not save to device manager file {self.filename}: {e}")

    def has_node(self, node_id: int) -> bool:
        """Determines if there's already an existing node for the node ID."""
        return any(node.node_id == node_id for node in self.nodes)

    def has_light_endpoint(self, node_id: int, endpoint_id) -> bool:
        """Determines if there's already an existing Light endpoint on a given node ID."""
        return any(((light.node_id == node_id) and (light.endpoint_id == endpoint_id)) for light in self.light_endpoints)

    def has_switch_endpoint(self, node_id: int, endpoint_id) -> bool:
        """Determines if there's already an existing Switch endpoint on a given node ID."""
        return any(((switch.node_id == node_id) and (switch.endpoint_id == endpoint_id)) for switch in self.switch_endpoints)

    def add_node(self, name: str, node_id: int) -> OnboardedNode:
        """Adds a new OnboardedNode."""
        if any(node.node_id == node_id for node in self.nodes):
            raise ValueError(f"Node with id {node_id} already exists.")
        if any(node.name == name for node in self.nodes):
            raise ValueError(f"Node with name '{name}' already exists.")

        new_node = OnboardedNode(name=name, node_id=node_id)
        self.nodes.append(new_node)
        self._save()

        return new_node

    def add_light(self, name: str, node_id: int, endpoint_id: int) -> LightEndpoint:
        """Adds a new LightEndpoint, validating the parent node exists."""
        if not any(node.node_id == node_id for node in self.nodes):
            raise ValueError(f"Cannot add light endpoint for non-existent node with id {node_id}.")

        if any(light.node_id == node_id and light.endpoint_id == endpoint_id for light in self.light_endpoints):
            raise ValueError(f"Light with node_id {node_id} and endpoint_id {endpoint_id} already exists.")

        new_light = LightEndpoint(name=name, node_id=node_id, endpoint_id=endpoint_id)
        self.light_endpoints.append(new_light)
        self._save()
        return new_light

    def add_switch(self, name: str, node_id: int, endpoint_id: int) -> SwitchEndpoint:
        """Adds a new SwitchEndpoint, validating the parent node exists."""
        if not any(node.node_id == node_id for node in self.nodes):
            raise ValueError(f"Cannot add switch endpoint for non-existent node with id {node_id}.")

        if any(switch.node_id == node_id and switch.endpoint_id == endpoint_id for switch in self.switch_endpoints):
            raise ValueError(f"Switch with node_id {node_id} and endpoint_id {endpoint_id} already exists.")

        new_switch = SwitchEndpoint(name=name, node_id=node_id, endpoint_id=endpoint_id)
        self.switch_endpoints.append(new_switch)
        self._save()
        return new_switch

    def get_endpoints_for_node(self, node_id: int):
        endpoints = []
        endpoints.extend([endpoint for endpoint in self.light_endpoints if endpoint.node_id == node_id])
        endpoints.extend([endpoint for endpoint in self.switch_endpoints if endpoint.node_id == node_id])
        return endpoints

    def get_all_light_endpoints(self, node_id: Optional[int]=None) -> list[LightEndpoint]:
        endpoints = []
        for node in self.nodes:
            if node_id is not None:
                if node.node_id != node_id: continue
            endpoints.extend([endpoint for endpoint in self.get_endpoints_for_node(node.node_id) if isinstance(endpoint, LightEndpoint)])
        return endpoints

    def get_all_switch_endpoints(self, node_id: Optional[int]=None) -> list[SwitchEndpoint]:
        endpoints = []
        for node in self.nodes:
            if node_id is not None:
                if node.node_id != node_id: continue
            endpoints.extend([endpoint for endpoint in self.get_endpoints_for_node(node.node_id) if isinstance(endpoint, SwitchEndpoint)])
        return endpoints


async def read_single_attribute(
            dev_ctrl: ChipDeviceCtrl.ChipDeviceController, node_id: int, endpoint: int, attribute: object, fabricFiltered: bool = True) -> object:
        result = await dev_ctrl.ReadAttribute(node_id, [(endpoint, attribute)], fabricFiltered=fabricFiltered)
        data = result[endpoint]
        return list(data.values())[0][attribute]

async def write_single_attribute(dev_ctrl: ChipDeviceCtrl.ChipDeviceController, node_id: int, endpoint_id: int, attribute_value: object) -> Status:
    write_result = await dev_ctrl.WriteAttribute(node_id, [(endpoint_id, attribute_value)])
    return write_result[0].Status


class GroupManager:
    def __init__(self, dev_ctrl: ChipDeviceCtrl.ChipDeviceController, admin_node_id:int=DEFAULT_ADMIN_NODE_ID):
        self._dev_ctrl = dev_ctrl
        self._admin_node_id = admin_node_id

    async def setup_key1(self, node_id: int):
        # WE ALWAYS ONLY USE KEY ID 1

        group_key_set = Clusters.GroupKeyManagement.Structs.GroupKeySetStruct(groupKeySetID=DEFAULT_GROUP_KEY_SET_ID, groupKeySecurityPolicy=0,
            epochKey0=DEFAULT_GROUP_EPOCH_KEY,
            epochStartTime0=1
        )

        key_set_write = Clusters.GroupKeyManagement.Commands.KeySetWrite(group_key_set)
        logging.info(f"Command: {key_set_write}")
        await self._dev_ctrl.SendCommand(nodeid=node_id, endpoint=0, payload=key_set_write)

    async def ensure_groups_have_access(self, node_id: int, group_ids: list[int]):
        # WE ALWAYS ONLY USE KEY ID 1

        acl_cluster = Clusters.AccessControl

        group_key_map_attrib = Clusters.GroupKeyManagement.Attributes.GroupKeyMap

        group_key_map = await read_single_attribute(dev_ctrl=self._dev_ctrl, node_id = node_id, endpoint=0, attribute=group_key_map_attrib)
        # logging.info(f"Group Key Map from 0x{node_id:016X}: {group_key_map} BEFORE UPDATE")

        acl_attrib = acl_cluster.Attributes.Acl
        acl = await read_single_attribute(dev_ctrl=self._dev_ctrl, node_id = node_id, endpoint=0, attribute=acl_attrib)
        logging.info(f"ACL: {acl}")

        # Set group key maps for all groups needed where it's missing
        missing_groups_in_key_map: set[int] = set(group_ids)
        for entry in group_key_map:
            missing_groups_in_key_map.discard(entry.groupId)

        group_key_map_struct = Clusters.GroupKeyManagement.Structs.GroupKeyMapStruct
        new_group_key_map = group_key_map[:]
        for group_id in missing_groups_in_key_map:
            new_group_key_map.append(group_key_map_struct(groupId=group_id, groupKeySetID=DEFAULT_GROUP_KEY_SET_ID))

        await write_single_attribute(dev_ctrl=self._dev_ctrl, node_id=node_id, endpoint_id=0, attribute_value=Clusters.GroupKeyManagement.Attributes.GroupKeyMap(new_group_key_map))

        # Rewrite ACL with a combined entry for all groups known
        new_acl = acl[:]
        to_delete = []
        for idx, entry in enumerate(new_acl):
            if entry.authMode == acl_cluster.Enums.AccessControlEntryAuthModeEnum.kGroup:
                to_delete.append(idx)
        for idx in reversed(to_delete):
            logging.info(f"==> Deleting stale ACL entry: {new_acl[idx]}")
            del new_acl[idx]

        group_acl_entry = acl_cluster.Structs.AccessControlEntryStruct(
            privilege = acl_cluster.Enums.AccessControlEntryPrivilegeEnum.kOperate,
            authMode = acl_cluster.Enums.AccessControlEntryAuthModeEnum.kGroup,
            subjects = group_ids,
            targets = [] # All endpoints!
        )
        new_acl.append(group_acl_entry)

        await write_single_attribute(dev_ctrl=self._dev_ctrl, node_id=node_id, endpoint_id=0, attribute_value=acl_cluster.Attributes.Acl(new_acl))

    async def join_groups(self, node_id: int, endpoint_id: int, group_ids: list[int]):
        groups = Clusters.Groups
        for group_id in group_ids:
            join_group = groups.Commands.AddGroup(group_id)
            await self._dev_ctrl.SendCommand(nodeid=node_id, endpoint=endpoint_id, payload=join_group)

    async def dump_groups(self, node_id: int):
        group_key_map_attrib = Clusters.GroupKeyManagement.Attributes.GroupKeyMap
        group_table_attrib = Clusters.GroupKeyManagement.Attributes.GroupTable

        group_table = await read_single_attribute(dev_ctrl=self._dev_ctrl, node_id = node_id, endpoint=0, attribute=group_table_attrib)
        logging.info(f"Group Table from 0x{node_id:016X}: {group_table}")

        group_key_map = await read_single_attribute(dev_ctrl=self._dev_ctrl, node_id = node_id, endpoint=0, attribute=group_key_map_attrib)
        logging.info(f"Group Key Map from 0x{node_id:016X}: {group_key_map} AFTER UPDATE")



class LifecycleState(enum.IntEnum):
    UNSPECIFIED = 0
    UNSUBSCRIBED = 1
    SUBSCRIBING = 2
    SUBSCRIBED = 3
    PENDING_RETRY = 4

@dataclass
class MonitoredNode:
    name: str
    node_id: int
    state: LifecycleState
    last_attempt_timestamp: float  # Valid in PENDING_RETRY state
    attribute_subscription: Optional[AttributeSubscriptionHandler]
    last_error: Optional[int] = None
    # TODO: Add subscription data cache



class SwitchDemoHandler:
    def __init__(self, dev_ctrl: ChipDeviceCtrl.ChipDeviceController, config_repository: DeviceConfigRepository):
        self._dev_ctrl = dev_ctrl
        self._config_repository = config_repository

    def on_switch_update(self, endpoint: SwitchEndpoint, new_position: int):
        if endpoint.node_id == 0x0004_0002 and endpoint.endpoint_id == 1 and new_position == 1:
            cmd = Clusters.OnOff.Commands.On()
            self._dev_ctrl.SendGroupCommand(groupid=DEFAULT_GROUP_ID, payload=cmd)
        elif endpoint.node_id == 0x0004_0002 and endpoint.endpoint_id == 2 and new_position == 1:
            cmd = Clusters.OnOff.Commands.Off()
            self._dev_ctrl.SendGroupCommand(groupid=DEFAULT_GROUP_ID, payload=cmd)


class DeviceSubscriber:
    def __init__(self, config_repository: DeviceConfigRepository, dev_ctrl: ChipDeviceCtrl, publish_update_callback: Callable[[dict], None], switch_demo_handler: SwitchDemoHandler):
        self._device_repository = config_repository
        self._dev_ctrl = dev_ctrl
        self._event_q = queue.Queue()

        self._switch_demo_handler = switch_demo_handler

        if SELECTED_SINGLE_NODE is not None:
            self._monitored_nodes: dict[int, MonitoredNode] = {node.node_id: MonitoredNode(name=node.name, node_id=node.node_id, state=LifecycleState.UNSUBSCRIBED, last_attempt_timestamp=0.0, attribute_subscription=None) for node in self._device_repository.nodes if node.node_id == SELECTED_SINGLE_NODE }
        else:
            self._monitored_nodes: dict[int, MonitoredNode] = {node.node_id: MonitoredNode(name=node.name, node_id=node.node_id, state=LifecycleState.UNSUBSCRIBED, last_attempt_timestamp=0.0, attribute_subscription=None) for node in self._device_repository.nodes }

        self._last_subscribe_attempt_time = 0.0
        self._current_node_ids_attempted: set[int] = set()

        self._not_yet_subscribed: list[int] = []
        self._not_yet_subscribed = list(self._monitored_nodes.keys())
        self._thread: Optional[threading.Thread] = None
        self._publish_update = publish_update_callback

    def start(self):
        def loop_runner():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                loop.run_until_complete(self.main_loop())
            except KeyboardInterrupt:
                self.shutdown()

            loop.close()

        if self._thread and self._thread.is_alive():
            logging.error("Already running! Ignoring DeviceSubscriber start request")
            return

        # Bootstrap possible first request
        self._schedule_subscription_if_needed()

        self._thread = threading.Thread(target=loop_runner, name="DeviceSubscriber", daemon=True)
        self._thread.start()

    def get_attrib_if_exists(self, node_id: int, path: TypedAttributePath):
        if node_id not in self._monitored_nodes:
            return None

        node = self._monitored_nodes[node_id]
        if node.attribute_subscription is None:
            return None

        sub = node.attribute_subscription.subscription
        if sub is None:
            return None

        try:
            return sub.GetAttribute(path)
        except (IndexError, KeyError, ValueError):
            return None

    def get_all_subscribed_node_ids(self) -> list[int]:
        return [node_id for node_id, node in self._monitored_nodes.items() if node.state == LifecycleState.SUBSCRIBED]

    def _get_endpoint(self, node_id: int, endpoint_id: int) -> Optional[object]:
        endpoints_for_node = self._device_repository.get_endpoints_for_node(node_id)

        for endpoint in endpoints_for_node:
            if endpoint.endpoint_id == endpoint_id:
                return endpoint
        else:
            return None

    def _compute_light_state(self, node_id: int, endpoint: LightEndpoint) -> dict[str, Any]:
        endpoint_data = {}
        endpoint_data.update({
            "state": "OFF",
            "brightness": 0
        })

        on_off_path = TypedAttributePath(
            Path=AttributePath.from_attribute(
                EndpointId=endpoint.endpoint_id,
                Attribute=Clusters.OnOff.Attributes.OnOff
            )
        )

        level_path = TypedAttributePath(
            Path=AttributePath.from_attribute(
                EndpointId=endpoint.endpoint_id,
                Attribute=Clusters.LevelControl.Attributes.CurrentLevel
            )
        )

        on_off_state = self.get_attrib_if_exists(node_id, on_off_path)
        if on_off_state is not None:
            endpoint_data["state"] = "ON" if on_off_state else "OFF"

        level_state = self.get_attrib_if_exists(node_id, level_path)
        if level_state is not None:
            endpoint_data["brightness"] = max(1, int(level_state * 100 / 254))

        return endpoint_data

    def _compute_switch_state(self, node_id: int, endpoint: SwitchEndpoint) -> dict[str, Any]:
        endpoint_data = {}
        endpoint_data.update({
            "state": "0",
        })

        current_position_path = TypedAttributePath(
            Path=AttributePath.from_attribute(
                EndpointId=endpoint.endpoint_id,
                Attribute=Clusters.Switch.Attributes.CurrentPosition
            )
        )

        current_position_state = self.get_attrib_if_exists(node_id, current_position_path)
        if current_position_state is not None:
            endpoint_data["state"] = str(current_position_state)

        return endpoint_data

    def compute_endpoint_data(self, node_id: int, endpoint: Any):
        node = self._monitored_nodes[node_id]

        endpoint_data = {
            "online": (node.state == LifecycleState.SUBSCRIBED),
            "endpoint_id": endpoint.endpoint_id,
            "node_id": node_id,
            "id": endpoint.name,
        }

        if isinstance(endpoint, LightEndpoint):
            endpoint_data["type"] = "dimmable"
            endpoint_data.update(self._compute_light_state(node_id, endpoint))

        if isinstance(endpoint, SwitchEndpoint):
            endpoint_data["type"] = "switch"
            endpoint_data.update(self._compute_switch_state(node_id, endpoint))

        if node.last_error is not None:
            endpoint_data["errors"] = [f"Error 0x{node.last_error:02X}"]

        return endpoint_data

    async def publish_all_device_state(self):
        device_state_db = {}
        for node_id, node in self._monitored_nodes.items():
            # TODO: use reachable for online
            endpoints_for_node = self._device_repository.get_endpoints_for_node(node_id)

            for endpoint in endpoints_for_node:
                endpoint_data = self.compute_endpoint_data(node_id, endpoint)
                device_state_db[endpoint.name] = endpoint_data

        await self._publish_update({"type": "FULL_STATE_SYNC", "payload": {"devices": device_state_db}})


    def on_event(self, event: MonitoringEvent):
        self._event_q.put(event, block=True)

    def shutdown(self):
        logging.info("Shutdown of DeviceSubscriber requested!")
        self._event_q.put(ShutdownEvent(), block=True)
        self._thread.join()
        self._thread = None

    async def main_loop(self):
        logging.info("Started DeviceSubscriber run loop!")

        # Send initial unknown device state
        await self.publish_all_device_state()

        while True:
            try:
                event = self._event_q.get(block=True, timeout=1.0)
                if isinstance(event, ShutdownEvent):
                    break
                await self._process_event(event)
                self._schedule_subscription_if_needed()
            except queue.Empty:
                self._schedule_subscription_if_needed()

        logging.info("Done with DeviceSubscriber run loop!")

    async def on_command_from_web(self, command_data: dict):
        self._event_q.put(CommandFromWebEvent(data=command_data), block=True)

    def _schedule_subscription_if_needed(self):
        now = time.time()
        elapsed_since_last_run = now - self._last_subscribe_attempt_time
        if elapsed_since_last_run < DEFAULT_ATTEMPT_DELAY_SECONDS:
            return

        self._last_subscribe_attempt_time = now

        # logging.info(f"Not yet subbed: {self._not_yet_subscribed}")
        # logging.info(f"Nodes: {self._monitored_nodes}")

        if len(self._not_yet_subscribed) > 0 and len(self._current_node_ids_attempted) < DEFAULT_PARALLEL_CONNECTIONS:
            next_node_id_to_try = self._find_first_unsubscribed_node_id()
            self._schedule_connection(next_node_id_to_try)

    def _find_first_unsubscribed_node_id(self) -> Optional[int]:
        if len(self._not_yet_subscribed) == 0:
            return None

        return self._not_yet_subscribed.pop()

    def _mark_device_as_connected(self, node_id: int):
        self._current_node_ids_attempted.discard(node_id)
        self._monitored_nodes[node_id].state = LifecycleState.SUBSCRIBED

    def _schedule_connection(self, node_id: int):
        self._current_node_ids_attempted.add(node_id)
        self._monitored_nodes[node_id].state = LifecycleState.SUBSCRIBING
        self._event_q.put(RequestNodeIdSubcribeEvent(node_id=node_id), block=True)

    def _mark_device_as_needing_reconnect(self, node_id: int):
        # Ensure we don't consider this device in the current try pool.
        self._current_node_ids_attempted.discard(node_id)

        # Maintain fairness consistency by scheduling retry as late as possible.
        try:
            self._not_yet_subscribed.remove(node_id)
        except ValueError:
            pass
        self._not_yet_subscribed.append(node_id)

        self._monitored_nodes[node_id].state = LifecycleState.PENDING_RETRY

    async def _update_light(self, node_id: int, endpoint: LightEndpoint):
        endpoint_state = {
            "id": endpoint.name
        }
        endpoint_state.update(self._compute_light_state(node_id, endpoint))
        await self._publish_update({"type": "DEVICE_UPDATE", "payload": endpoint_state})

    async def _update_switch(self, node_id: int, endpoint: SwitchEndpoint):
        endpoint_state = {
            "id": endpoint.name
        }

        # TODO: UNHACK THIS
        endpoint_state.update(self._compute_switch_state(node_id, endpoint))
        print(endpoint, int(endpoint_state["state"]))
        self._switch_demo_handler.on_switch_update(endpoint, int(endpoint_state["state"]))

        await self._publish_update({"type": "DEVICE_UPDATE", "payload": endpoint_state})

    async def _handle_command_from_web(self, command_data: dict):
        type = command_data.get("type", "")
        if type == "ON_CONNECT":
            await self.publish_all_device_state()

    async def _process_event(self, event: MonitoringEvent):
        if isinstance(event, CaseConnectionEvent):
            logging.info(f"EVENT: CASE ConnectionEvent for node 0x{event.node_id:016X}")
        elif isinstance(event, SessionFailedEvent):
            logging.info(f"EVENT: SessionFailedEvent for node 0x{event.node_id:016X}")
        elif isinstance(event, SubscriptionEstablishedEvent):
            logging.info(f"EVENT: SubscriptionEstablishedEvent for node 0x{event.node_id:016X}")
            self._mark_device_as_connected(event.node_id)
            self._monitored_nodes[event.node_id].last_error = None
            await self.publish_all_device_state()
        elif isinstance(event, ResubscriptionAttemptedEvent):
            logging.info(f"EVENT: ResubscriptionAttemptedEvent for node 0x{event.node_id:016X}")
        elif isinstance(event, SubscriptionErrorEvent):
            logging.info(f"EVENT: SubscriptionErrorEvent for node 0x{event.node_id:016X}: 0x{event.error:02X}")
            self._mark_device_as_needing_reconnect(event.node_id)
            self._monitored_nodes[event.node_id].last_error = event.error
            await self.publish_all_device_state()
        elif isinstance(event, SubscriptionDataEvent):
            node_id = event.node_id
            endpoint_id = event.path.Path.EndpointId
            endpoint = self._get_endpoint(node_id, endpoint_id)

            logging.info(f"@@@ ATTR from 0x{node_id:016X}: {event.path.Path}={event.value.value}")

            if isinstance(endpoint, LightEndpoint):
                await self._update_light(node_id, endpoint)
            elif isinstance(endpoint, SwitchEndpoint):
                await self._update_switch(node_id, endpoint)
        elif isinstance(event, SubscriptionReportBeginEvent):
            logging.info(f"@@@ BEGIN from 0x{event.node_id:016X}")
        elif isinstance(event, SubscriptionReportEndEvent):
            logging.info(f"@@@ END from 0x{event.node_id:016X}")

        elif isinstance(event, RequestNodeIdSubcribeEvent):
            logging.info(f"EVENT: RequestNodeIdSubcribeEvent for node 0x{event.node_id:016X}, attempting to sub")
            attribute_subscription = AttributeSubscriptionHandler(handler=self)
            sub = await attribute_subscription.start(self._dev_ctrl, node_id=event.node_id)
            if sub is None:
                self._monitored_nodes[event.node_id].attribute_subscription = None
                self._mark_device_as_needing_reconnect(event.node_id)
            else:
                self._monitored_nodes[event.node_id].attribute_subscription = attribute_subscription
        elif isinstance(event, CommandFromWebEvent):
            logging.info(f"Received command from web: {event}")
            await self._handle_command_from_web(event.data)
        else:
            logging.error(f"EVENT: Unknown event: {str(event)}!")


class WebCommandHandler:
    def __init__(self, dev_ctrl: ChipDeviceCtrl.ChipDeviceController, device_subscriber: DeviceSubscriber, group_manager: GroupManager, config_repository: DeviceConfigRepository, publish_update_callback: Callable[[dict], None]):
        self._dev_ctrl = dev_ctrl
        self._device_subscriber = device_subscriber
        self._group_manager = group_manager
        self._config_repository = config_repository
        self._publish_update_callback = publish_update_callback

    async def on_command_from_web(self, command_data: dict):
        await self._device_subscriber.on_command_from_web(command_data)

        type = command_data.get("type", "")
        logging.info(f"@@@ Web Action: {command_data}")

        payload = command_data.get("payload", {})

        # Some common params
        transition_time = payload.get("transition_time", 0)
        level = payload.get("level", 254)

        if type == "DEVICE_ACTION":
            node_id = payload.get("node_id")
            endpoint_id = payload.get("endpoint_id")
            action_name = payload.get("action_name", "")
            if node_id is None or endpoint_id is None or not action_name:
                return
            try:
                if action_name == "TURN_ON":
                    logging.info(f"Turning 0x{node_id:016X}.{endpoint_id:X} On")
                    cmd = Clusters.OnOff.Commands.On()
                    await self._dev_ctrl.SendCommand(nodeid=node_id, endpoint=endpoint_id, payload=cmd)
                elif action_name == "TURN_OFF":
                    logging.info(f"Turning 0x{node_id:016X}.{endpoint_id:X} Off")
                    cmd = Clusters.OnOff.Commands.Off()
                    await self._dev_ctrl.SendCommand(nodeid=node_id, endpoint=endpoint_id, payload=cmd)
                elif action_name == "TOGGLE":
                    logging.info(f"Toggling 0x{node_id:016X}.{endpoint_id:X}")
                    cmd = Clusters.OnOff.Commands.Toggle()
                    await self._dev_ctrl.SendCommand(nodeid=node_id, endpoint=endpoint_id, payload=cmd)
                elif action_name == "MOVE_TO_LEVEL":
                    logging.info(f"Moving to level 0x{node_id:016X}.{endpoint_id:X} to {level} over transition time {transition_time}")

                    EXECUTE_IF_OFF = Clusters.LevelControl.Bitmaps.OptionsBitmap.kExecuteIfOff
                    cmd = Clusters.LevelControl.Commands.MoveToLevel(level=level, transitionTime=transition_time, optionsMask=EXECUTE_IF_OFF, optionsOverride=EXECUTE_IF_OFF)
                    await self._dev_ctrl.SendCommand(nodeid=node_id, endpoint=endpoint_id, payload=cmd)
                elif action_name == "SETUP_KEY1":
                    logging.info(f"Setup Key1 from 0x{node_id:016X}.{endpoint_id:X}")
                    await self._group_manager.setup_key1(node_id)
                    await self._group_manager.ensure_groups_have_access(node_id, [DEFAULT_GROUP_ID])
                    await self._group_manager.join_groups(node_id, endpoint_id, [DEFAULT_GROUP_ID])
                    await self._group_manager.dump_groups(node_id)

            except (ChipStackError, InteractionModelError) as e:
                logging.error(f"Command error 0x{node_id:016X}.{endpoint_id:X}: {str(e)}")
        elif type == "ALL_ON":
            pass
        elif type == "ALL_OFF":
            pass
        elif type == "ALL_BRIGHT_GROUP":
            EXECUTE_IF_OFF = Clusters.LevelControl.Bitmaps.OptionsBitmap.kExecuteIfOff
            cmd = Clusters.LevelControl.Commands.MoveToLevel(level=level, transitionTime=transition_time, optionsMask=EXECUTE_IF_OFF, optionsOverride=EXECUTE_IF_OFF)
            self._dev_ctrl.SendGroupCommand(groupid=DEFAULT_GROUP_ID, payload=cmd)
        elif type == "ALL_DIM_GROUP":
            EXECUTE_IF_OFF = Clusters.LevelControl.Bitmaps.OptionsBitmap.kExecuteIfOff
            cmd = Clusters.LevelControl.Commands.MoveToLevel(level=1, transitionTime=transition_time, optionsMask=EXECUTE_IF_OFF, optionsOverride=EXECUTE_IF_OFF)
            self._dev_ctrl.SendGroupCommand(groupid=DEFAULT_GROUP_ID, payload=cmd)
        elif type == "ALL_ON_GROUP":
            cmd = Clusters.OnOff.Commands.On()
            self._dev_ctrl.SendGroupCommand(groupid=DEFAULT_GROUP_ID, payload=cmd)
        elif type == "ALL_OFF_GROUP":
            cmd = Clusters.OnOff.Commands.Off()
            self._dev_ctrl.SendGroupCommand(groupid=DEFAULT_GROUP_ID, payload=cmd)
        elif type == "ALL_SETUP_GROUP":
            for node_id in self._device_subscriber.get_all_subscribed_node_ids():
                try:
                    light_endpoint_ids = [endpoint.endpoint_id for endpoint in self._config_repository.get_all_light_endpoints(node_id=node_id)]

                    logging.info(f"Setup group on endpoints {light_endpoint_ids} for 0x{node_id:016X}")

                    await self._group_manager.setup_key1(node_id)
                    await self._group_manager.ensure_groups_have_access(node_id, [DEFAULT_GROUP_ID])
                    for endpoint_id in light_endpoint_ids:
                        await self._group_manager.join_groups(node_id, endpoint_id, [DEFAULT_GROUP_ID])
                    await self._group_manager.dump_groups(node_id)
                except:
                    logging.exception("Group setup error!!!")
        elif type == "ALL_DIM_GROUP":
            pass

class GroupManagerExperiment(MatterBaseTest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._device_repository = None

    def configure_devices(self):
        device_database_path = self.user_params.get("device_database_path", "device_database.json")
        self._device_repository = DeviceConfigRepository(device_database_path)

    @async_test_body
    async def test_CommissionOneLightBulbNotBridged(self):
        self.configure_devices()

        identify = Clusters.Identify
        onoff = Clusters.OnOff
        endpoint_id = 1

        node_name = self.user_params.get("node_name", "DUT")
        device_name = self.user_params.get("device_name", f"0001_{(self.dut_node_id & 0xFFFF):04X}")

        logging.info(f"Sending identify on endpoint {endpoint_id}")
        await self.send_single_cmd(cmd=identify.Commands.Identify(identifyTime=5), endpoint=endpoint_id)
        logging.info(f"Waiting identify complete")
        time.sleep(5.0)

        if not self._device_repository.has_node(self.dut_node_id):
            self._device_repository.add_node(node_name, self.dut_node_id)
            self._device_repository.add_light(device_name, self.dut_node_id, endpoint_id)

        logging.info(f"Turning light off on endpoint {endpoint_id}")
        await self.send_single_cmd(cmd=onoff.Commands.Off(), endpoint=endpoint_id)

    async def update_node_label_if_present(self, node_id: int, endpoint_id: int, node_label: str):
        descriptor = Clusters.Descriptor
        server_list = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=endpoint_id, attribute=descriptor.Attributes.ServerList)
        dev_ctrl = self.default_controller

        BASIC_INFO = Clusters.BasicInformation.id
        BRIDGED_BASIC_INFO = Clusters.BridgedDeviceBasicInformation.id

        if BASIC_INFO in server_list:
            attribute_value = Clusters.BasicInformation.Attributes.NodeLabel(node_label)
        elif BRIDGED_BASIC_INFO in server_list:
            attribute_value = Clusters.BridgedDeviceBasicInformation.Attributes.NodeLabel(node_label)
        else:
            logging.error(f"Did not find NodeLabel on endpoint {endpoint_id}")
            return

        write_result = await dev_ctrl.WriteAttribute(node_id, [(endpoint_id, attribute_value)])
        asserts.assert_equal(write_result[0].Status, Status.Success,
                              f"Expected write success for write to attribute {attribute_value} on endpoint {endpoint_id}")

    async def read_node_label_if_present(self, node_id:int, endpoint_id: int) -> str:
        descriptor = Clusters.Descriptor
        server_list = await self.read_single_attribute_check_success(cluster=descriptor, node_id=node_id, endpoint=endpoint_id, attribute=descriptor.Attributes.ServerList)

        BASIC_INFO = Clusters.BasicInformation.id
        BRIDGED_BASIC_INFO = Clusters.BridgedDeviceBasicInformation.id

        if BASIC_INFO in server_list:
            return await self.read_single_attribute_check_success(cluster=Clusters.BasicInformation, node_id=node_id, endpoint=endpoint_id, attribute=Clusters.BasicInformation.Attributes.NodeLabel)
        elif BRIDGED_BASIC_INFO in server_list:
            return await self.read_single_attribute_check_success(cluster=Clusters.BridgedDeviceBasicInformation, node_id=node_id, endpoint=endpoint_id, attribute=Clusters.BridgedDeviceBasicInformation.Attributes.NodeLabel)
        else:
            logging.warning(f"Did not find NodeLabel on endpoint {endpoint_id}")
            return ""

    def is_light_like_endpoint(self, device_types: list[Clusters.Descriptor.Structs.DeviceTypeStruct]):
        device_type_ids = set([dt.deviceType for dt in device_types])

        light_likes = set([
            256, # On/Off Light
            257, # Dimmable Light
            259, # On/Off Light Switch
            260, # Dimmer Switch
            261, # Color Dimmer Switch
            262, # Light Sensor
            263, # Occupancy Sensor
            266, # On/Off Plug-in Unit
            267, # Dimmable Plug-In Unit
            268, # Color Temperature Light
            269, # Extended Color Light
            271, # Mounted On/Off Control
            272, # Mounted Dimmable Load Control
        ])

        return len(device_type_ids.intersection(light_likes)) > 0

    def is_switch_endpoint(self, device_types: list[Clusters.Descriptor.Structs.DeviceTypeStruct]):
        device_type_ids = set([dt.deviceType for dt in device_types])

        switch_types = set([
            0xF # Generic switch
        ])

        return len(device_type_ids.intersection(switch_types)) > 0

    async def invoke_unicast_on_all_lights(self, command, predicate=lambda x: True, max_parallel:int =10000):
        candidates = [light for light in self._device_repository.light_endpoints if predicate(light)]

        results = []
        tasks = []
        while candidates:
            target = candidates.pop(0)
            task = self.send_single_cmd(cmd=command, node_id = target.node_id, endpoint=target.endpoint_id)
            tasks.append(task)

            if len(tasks) >= max_parallel:
                results.extend(await asyncio.gather(*tasks, return_exceptions=True))
                tasks = []

        results.extend(await asyncio.gather(*tasks, return_exceptions=True))
        return results

    @async_test_body
    async def test_CommissionOneBridgeAndAllLightBulbs(self):
        self.configure_devices()

        identify = Clusters.Identify
        onoff = Clusters.OnOff
        descriptor = Clusters.Descriptor

        asserts.assert_in("node_name", self.user_params, "Need to provide --string-arg node_name:XXXX")
        asserts.assert_in("device_name", self.user_params, "Need to provide --string-arg device_name:XXXX")
        node_name = self.user_params["node_name"]
        #device_name = self.user_params["device_name"]

        if not self._device_repository.has_node(self.dut_node_id):
            self._device_repository.add_node(node_name, self.dut_node_id)

        all_child_endpoints = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=0, attribute=descriptor.Attributes.PartsList)
        logging.info(f"Endpoints on bridge: {all_child_endpoints}")

        existing_node_label = self.read_node_label_if_present(node_id=self.dut_node_id, endpoint_id=0)
        logging.info(f"Existing root NodeLabel: {existing_node_label}")

        for endpoint_id in all_child_endpoints:
            device_types = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=endpoint_id, attribute=descriptor.Attributes.DeviceTypeList)
            server_list = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=endpoint_id, attribute=descriptor.Attributes.ServerList)
            existing_node_label = await self.read_node_label_if_present(node_id=self.dut_node_id, endpoint_id=endpoint_id)

            logging.info(f"->    EP{endpoint_id}: Device types: {device_types}, Server list: {server_list}")
            logging.info(f"->    EP{endpoint_id}: Existing NodeLabel: {existing_node_label}")

            if self.is_light_like_endpoint(device_types):
                if not self._device_repository.has_light_endpoint(self.dut_node_id, endpoint_id):
                    self._device_repository.add_light(name=existing_node_label, node_id=self.dut_node_id, endpoint_id=endpoint_id)
                    logging.info(f"->    Added light endpoint EP{endpoint_id} with name {existing_node_label}")

                logging.info(f"Turning light off on endpoint {endpoint_id}")
                await self.send_single_cmd(cmd=onoff.Commands.Off(), endpoint=endpoint_id)

    @async_test_body
    async def test_CommissionOneSwitch(self):
        self.configure_devices()

        identify = Clusters.Identify
        switch = Clusters.Switch
        descriptor = Clusters.Descriptor

        asserts.assert_in("node_name", self.user_params, "Need to provide --string-arg node_name:XXXX")
        asserts.assert_in("device_name", self.user_params, "Need to provide --string-arg device_name:XXXX")
        node_name = self.user_params["node_name"]
        device_name = self.user_params["device_name"]

        if not self._device_repository.has_node(self.dut_node_id):
            self._device_repository.add_node(node_name, self.dut_node_id)

        all_child_endpoints = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=0, attribute=descriptor.Attributes.PartsList)
        logging.info(f"Endpoints on device: {all_child_endpoints}")

        for endpoint_id in all_child_endpoints:
            device_types = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=endpoint_id, attribute=descriptor.Attributes.DeviceTypeList)
            server_list = await self.read_single_attribute_check_success(cluster=descriptor, endpoint=endpoint_id, attribute=descriptor.Attributes.ServerList)

            logging.info(f"->    EP{endpoint_id}: Device types: {device_types}, Server list: {server_list}")

            if self.is_switch_endpoint(device_types):
                if not self._device_repository.has_switch_endpoint(self.dut_node_id, endpoint_id):
                    switch_name = f"{device_name}_ep{endpoint_id}"
                    self._device_repository.add_switch(name=switch_name, node_id=self.dut_node_id, endpoint_id=endpoint_id)
                    logging.info(f"->    Added light endpoint EP{endpoint_id} with name {switch_name}")


    @async_test_body
    async def test_BlinkAllLights(self):
        self.configure_devices()

        onoff = Clusters.OnOff

        light_matcher = (lambda light: (light.node_id & 0xFFFF0000 != 0x00010000))
        for _ in range(10):
            await self.invoke_unicast_on_all_lights(onoff.Commands.On(), predicate=light_matcher, max_parallel=8)
            time.sleep(1.0)
            await self.invoke_unicast_on_all_lights(onoff.Commands.Off() , predicate=light_matcher, max_parallel=8)
            time.sleep(1.0)

    @async_test_body
    async def test_TurnAllOff(self):
        self.configure_devices()

        onoff = Clusters.OnOff

        await self.invoke_unicast_on_all_lights(onoff.Commands.Off(), max_parallel=1000)

    @async_test_body
    async def test_TurnAllOn(self):
        self.configure_devices()

        onoff = Clusters.OnOff

        await self.invoke_unicast_on_all_lights(onoff.Commands.On(), max_parallel=1000)

    @async_test_body
    async def test_SetAllDim(self):
        self.configure_devices()

        levelcontrol = Clusters.LevelControl

        await self.invoke_unicast_on_all_lights(levelcontrol.Commands.MoveToLevel(level=1, transitionTime=0, optionsMask=1, optionsOverride=1), max_parallel=1)

    @property
    def default_timeout(self) -> int:
        return 2**63

    @async_test_body
    async def test_RunDeviceSubscriber(self):
        from daemon_runner import DaemonRunner
        from device_management_daemon import publish_update, set_json_handler, get_app

        single_node = self.user_params.get("single_node")
        if single_node:
          global SELECTED_SINGLE_NODE
          SELECTED_SINGLE_NODE = single_node

        self.configure_devices()

        daemon_runner = DaemonRunner(get_app())
        daemon_runner.start()

        dev_ctrl = self.default_controller
        dev_ctrl.SetLocalMRPConfig(idle_ms=3000, active_ms=3000, active_threshold_ms=3000)

        # Ensure group comms can work
        dev_ctrl.InitGroupTestingData()

        switch_demo_handler = SwitchDemoHandler(dev_ctrl, self._device_repository)
        subscriber = DeviceSubscriber(config_repository=self._device_repository, dev_ctrl=dev_ctrl, publish_update_callback=publish_update, switch_demo_handler=switch_demo_handler)
        group_manager = GroupManager(dev_ctrl)
        web_command_handler = WebCommandHandler(dev_ctrl=dev_ctrl, device_subscriber=subscriber, group_manager=group_manager, config_repository=self._device_repository, publish_update_callback=publish_update)

        set_json_handler(web_command_handler.on_command_from_web)

        # Create an event that will never be set, causing await to block forever.
        shutdown_event = asyncio.Event()

        try:
            subscriber.start()
            logging.info("Test is running. Press Ctrl+C to stop.")
            await shutdown_event.wait()
        except KeyboardInterrupt:
            logging.info("Shutdown requested via KeyboardInterrupt...")
        finally:
            logging.info("Initiating shutdown of services...")
            daemon_runner.stop()
            subscriber.shutdown()
            logging.info("Shutdown complete.")


if __name__ == "__main__":
    default_matter_test_main()
