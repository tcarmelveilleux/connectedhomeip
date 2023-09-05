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

import logging
import queue

from threading import Event
from chip.clusters import ClusterObjects as ClustersObjects
from chip.clusters.Attribute import SubscriptionTransaction, TypedAttributePath

# TODO: Overall, we need to add validation that session IDs have not changed throughout to be agnostic
#       to some internal behavior assumptions of the SDK we are making relative to the write to
#       the trigger the subscriptions not re-opening a new CASE session
#

class   :
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


class ResubscriptionCatcher:
    def __init__(self, name):
        self._name = name
        self._got_resubscription_event = Event()

    def __call__(self, transaction: SubscriptionTransaction, terminationError, nextResubscribeIntervalMsec):
        self._got_resubscription_event.set()
        logging.info("Got resubscription on client %s" % self.name)

    @property
    def name(self) -> str:
        return self._name

    @property
    def caught_resubscription(self) -> bool:
        return self._got_resubscription_event.is_set()
