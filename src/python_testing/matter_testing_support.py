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

# TODO: need storage path to be provided
# TODO: Init stack only if not done
# TODO: Add utility to commission a device if needed
# TODO: Add utilities to keep track of controllers/fabrics

@dataclass
class MatterTestConfig:
  storage_path: str = None



class MatterStackState:
  def __init__(self, config: MatterTestConfig):
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
      self._chipStack = builtins.chipStack
      # TODO: Warn that storage will not follow what we set in config
    else:
      self._chipStack = ChipStack(**kwargs)

    self._storage = self._chipStack.GetStorageManager()

  def __del__(self):
    if self._we_initialized_the_stack:
      # Unfortunately, all the below are singleton and possibly
      # managed elsewhere so we have to be careful not to touch unless
      # we initialized ourselves.
      chip.FabricAdmin.FabricAdmin.ShutdownAll()
      ChipDeviceCtrl.ChipDeviceController.ShutdownAll()

      global_chip_stack = builtins.chipStack
      global_chip_stack.Shutdown()
    # self._resourcesLock = Lock()
    # with self._resourcesLock


class MatterBaseTest:

_fabricAdmins = None

def LoadFabricAdmins():
    global _fabricAdmins

    #
    # Shutdown any fabric admins we had before as well as active controllers. This ensures we
    # relinquish some resources if this is called multiple times (e.g in a Jupyter notebook)
    #
    chip.FabricAdmin.FabricAdmin.ShutdownAll()
    ChipDeviceCtrl.ChipDeviceController.ShutdownAll()

    _fabricAdmins = []
    storageMgr = builtins.chipStack.GetStorageManager()

    console = Console()

    try:
        adminList = storageMgr.GetReplKey('fabricAdmins')
    except KeyError:
        console.print(
            "\n[purple]No previous fabric admins discovered in persistent storage - creating a new one...")

        #
        # Initialite a FabricAdmin with a VendorID of TestVendor1 (0xfff1)
        #
        _fabricAdmins.append(chip.FabricAdmin.FabricAdmin(0XFFF1))
        return _fabricAdmins

    console.print('\n')

    for k in adminList:
        console.print(
            f"[purple]Restoring FabricAdmin from storage to manage FabricId {adminList[k]['fabricId']}, FabricIndex {k}...")
        _fabricAdmins.append(chip.FabricAdmin.FabricAdmin(vendorId=int(adminList[k]['vendorId']),
                                                          fabricId=adminList[k]['fabricId'], fabricIndex=int(k)))

    console.print(
        '\n[blue]Fabric Admins have been loaded and are available at [red]fabricAdmins')
    return _fabricAdmins


def CreateDefaultDeviceController():
    global _fabricAdmins

    if (len(_fabricAdmins) == 0):
        raise RuntimeError("Was called before calling LoadFabricAdmins()")

    console = Console()

    console.print('\n')
    console.print(
        f"[purple]Creating default device controller on fabric {_fabricAdmins[0]._fabricId}...")
    return _fabricAdmins[0].NewController()


def ReplInit(debug):
    #
    # Install the pretty printer that rich provides to replace the existing
    # printer.
    #
    pretty.install(indent_guides=True, expand_all=True)

    console = Console()

    console.rule('Matter REPL')
    console.print('''
            [bold blue]

            Welcome to the Matter Python REPL!

            For help, please type [/][bold green]matterhelp()[/][bold blue]

            To get more information on a particular object/class, you can pass
            that into [bold green]matterhelp()[/][bold blue] as well.

            ''')
    console.rule()

    coloredlogs.install(level='DEBUG')
    chip.logging.RedirectToPythonLogging()

    if debug:
        logging.getLogger().setLevel(logging.DEBUG)
    else:
        logging.getLogger().setLevel(logging.WARN)


def StackShutdown():
    chip.FabricAdmin.FabricAdmin.ShutdownAll()
    ChipDeviceCtrl.ChipDeviceController.ShutdownAll()
    builtins.chipStack.Shutdown()


def matterhelp(classOrObj=None):
    if (classOrObj is None):
        inspect(builtins.devCtrl, methods=True, help=True, private=False)
        inspect(mattersetlog)
        inspect(mattersetdebug)
    else:
        inspect(classOrObj, methods=True, help=True, private=False)


def mattersetlog(level):
    logging.getLogger().setLevel(level)


def mattersetdebug(enableDebugMode: bool = True):
    ''' Enables debug mode that is utilized by some Matter modules
        to better facilitate debugging of failures (e.g throwing exceptions instead
        of returning well-formatted results).
    '''
    builtins.enableDebugMode = enableDebugMode


console = Console()

parser = argparse.ArgumentParser()
parser.add_argument(
    "-p", "--storagepath", help="Path to persistent storage configuration file (default: /tmp/repl-storage.json)", action="store", default="/tmp/repl-storage.json")
parser.add_argument(
    "-d", "--debug", help="Set default logging level to debug.", action="store_true")
args = parser.parse_args()

ReplInit(args.debug)
chipStack = ChipStack(persistentStoragePath=args.storagepath)
fabricAdmins = LoadFabricAdmins()
devCtrl = CreateDefaultDeviceController()

builtins.devCtrl = devCtrl

atexit.register(StackShutdown)

console.print(
    '\n\n[blue]Default CHIP Device Controller has been initialized to manage [bold red]fabricAdmins[0][blue], and is available as [bold red]devCtrl')
