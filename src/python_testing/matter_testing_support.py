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

import logging
from chip import ChipDeviceCtrl
import chip.clusters as Clusters
from chip.ChipStack import *
import chip.logging
import chip.FabricAdmin
from chip.utils import CommissioningBuildingBlocks
import builtins
from threading import Lock
from dataclasses import dataclass

# TODO: Add utility to commission a device if needed
# TODO: Add utilities to keep track of controllers/fabrics

logger = logging.getLogger("matter.python_testing")
logger.setLevel(logging.INFO)

@dataclass
class MatterTestConfig:
  storage_path: str = None
  admin_vendor_id: int = 0xFFF1

class MatterStackState:
    def __init__(self, config: MatterTestConfig):
        self._logger = logger
        self._config = config
        self._fabric_admins = []
        self._fabric_admins_root = chip.FabricAdmin.FabricAdmin

        if not hasattr(builtins, "chipStack"):
            if config.storage_path is None:
                raise ValueError("Must have configured a MatterTestConfig.storage_path")
            self._init_stack(already_initialized=False, persistentStoragePath=config.storage_path)
            self._we_initialized_the_stack = True
        else:
            self._init_stack(already_initialized=True)
            self._we_initialized_the_stack = False

    def _init_stack(self, already_initialized: bool, **kwargs):
        if already_initialized:
            self._chip_stack = builtins.chipStack
            self._logger.warn("Re-using existing ChipStack object found in current interpreter: storage path %s will be ignored!" % (self._config.storage_path))
            # TODO: Warn that storage will not follow what we set in config
        else:
            self._chip_stack = ChipStack(**kwargs)
            builtins.chipStack = self._chip_stack

        self._storage = self._chip_stack.GetStorageManager()

        try:
            admin_list = self._storage.GetReplKey('fabricAdmins')
            found_admin_list = True
        except KeyError:
            found_admin_list = False

        if not found_admin_list:
            self._logger.warn("No previous fabric administrative data found in persistent data: initializing new ones")
            self._fabric_admins.append(chip.FabricAdmin.FabricAdmin(self._config.admin_vendor_id))
        else:
            for fabric_idx in admin_list:
                self._logger.info("Restoring FabricAdmin from storage to manage FabricId {admin_list[k]['fabricId']}, FabricIndex {fabric_idx}")
                self._fabric_admins.append(chip.FabricAdmin.FabricAdmin(vendorId=int(admin_list[fabric_idx]['vendorId']),
                                           fabricId=admin_list[fabric_idx]['fabricId'], fabricIndex=int(fabric_idx)))

    # TODO: support getting access to chip-tool credentials issuer's data

    def Shutdown(self):
        if self._we_initialized_the_stack:
            # Unfortunately, all the below are singleton and possibly
            # managed elsewhere so we have to be careful not to touch unless
            # we initialized ourselves.
            chip.FabricAdmin.ShutdownAll()
            ChipDeviceCtrl.ChipDeviceController.ShutdownAll()
            global_chip_stack = builtins.chipStack
            global_chip_stack.Shutdown()

    @property
    def fabric_admins(self):
        return self._fabric_admins

    @property
    def storage(self):
        return self._storage


class MatterBaseTest:
    def __init__(self):
       pass


# def CreateDefaultDeviceController():
#     global _fabricAdmins

#     if (len(_fabricAdmins) == 0):
#         raise RuntimeError("Was called before calling LoadFabricAdmins()")

#     console = Console()

#     console.print('\n')
#     console.print(
#         f"[purple]Creating default device controller on fabric {_fabricAdmins[0]._fabricId}...")
#     return _fabricAdmins[0].NewController()


# def ReplInit(debug):
#     #
#     # Install the pretty printer that rich provides to replace the existing
#     # printer.
#     #
#     pretty.install(indent_guides=True, expand_all=True)

#     console = Console()

#     console.rule('Matter REPL')
#     console.print('''
#             [bold blue]

#             Welcome to the Matter Python REPL!

#             For help, please type [/][bold green]matterhelp()[/][bold blue]

#             To get more information on a particular object/class, you can pass
#             that into [bold green]matterhelp()[/][bold blue] as well.

#             ''')
#     console.rule()

#     coloredlogs.install(level='DEBUG')
#     chip.logging.RedirectToPythonLogging()

#     if debug:
#         logging.getLogger().setLevel(logging.DEBUG)
#     else:
#         logging.getLogger().setLevel(logging.WARN)


# def StackShutdown():
#     chip.FabricAdmin.FabricAdmin.ShutdownAll()
#     ChipDeviceCtrl.ChipDeviceController.ShutdownAll()
#     builtins.chipStack.Shutdown()


# def matterhelp(classOrObj=None):
#     if (classOrObj is None):
#         inspect(builtins.devCtrl, methods=True, help=True, private=False)
#         inspect(mattersetlog)
#         inspect(mattersetdebug)
#     else:
#         inspect(classOrObj, methods=True, help=True, private=False)


# def mattersetlog(level):
#     logging.getLogger().setLevel(level)


# def mattersetdebug(enableDebugMode: bool = True):
#     ''' Enables debug mode that is utilized by some Matter modules
#         to better facilitate debugging of failures (e.g throwing exceptions instead
#         of returning well-formatted results).
#     '''
#     builtins.enableDebugMode = enableDebugMode


# console = Console()

# parser = argparse.ArgumentParser()
# parser.add_argument(
#     "-p", "--storagepath", help="Path to persistent storage configuration file (default: /tmp/repl-storage.json)", action="store", default="/tmp/repl-storage.json")
# parser.add_argument(
#     "-d", "--debug", help="Set default logging level to debug.", action="store_true")
# args = parser.parse_args()

# ReplInit(args.debug)
# chipStack = ChipStack(persistentStoragePath=args.storagepath)
# fabricAdmins = LoadFabricAdmins()
# devCtrl = CreateDefaultDeviceController()

# builtins.devCtrl = devCtrl

# atexit.register(StackShutdown)

# console.print(
#     '\n\n[blue]Default CHIP Device Controller has been initialized to manage [bold red]fabricAdmins[0][blue], and is available as [bold red]devCtrl')

if __name__ == "__main__":
    config = MatterTestConfig()
    config.storage_path = "admin_storage.json"
    stack = MatterStackState(config)
    devCtrl = stack.fabric_admins[0].NewController()
    print(devCtrl.GetFabricId())
    stack.Shutdown()
