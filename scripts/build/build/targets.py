# Copyright (c) 2021 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

from typing import Any, List
from itertools import combinations

from builders.ameba import AmebaApp, AmebaBoard, AmebaBuilder
from builders.android import AndroidBoard, AndroidApp, AndroidBuilder
from builders.cc13x2x7_26x2x7 import cc13x2x7_26x2x7App, cc13x2x7_26x2x7Builder
from builders.cyw30739 import Cyw30739Builder, Cyw30739App, Cyw30739Board
from builders.efr32 import Efr32Builder, Efr32App, Efr32Board
from builders.esp32 import Esp32Builder, Esp32Board, Esp32App
from builders.host import HostBuilder, HostApp, HostBoard
from builders.infineon import InfineonBuilder, InfineonApp, InfineonBoard
from builders.k32w import K32WApp, K32WBuilder
from builders.mbed import MbedApp, MbedBoard, MbedProfile, MbedBuilder
from builders.nrf import NrfApp, NrfBoard, NrfConnectBuilder
from builders.qpg import QpgApp, QpgBoard, QpgBuilder
from builders.telink import TelinkApp, TelinkBoard, TelinkBuilder
from builders.tizen import TizenApp, TizenBoard, TizenBuilder
from builders.bl602 import Bl602App, Bl602Board, Bl602Builder


class Target:
    """Represents a build target:
        Has a name identifier plus parameters on how to build it (what
        builder class to use and what arguments are required to produce
        the specified build)
    """

    def __init__(self, name, builder_class, **kwargs):
        self.name = name
        self.builder_class = builder_class
        self.glob_blacklist_reason = None

        self.create_kw_args = kwargs

    def Clone(self):
        """Creates a clone of self."""

        clone = Target(self.name, self.builder_class,
                       **self.create_kw_args.copy())
        clone.glob_blacklist_reason = self.glob_blacklist_reason

        return clone

    def Extend(self, suffix, **kargs):
        """Creates a clone of the current object extending its build parameters.
        Arguments:
           suffix: appended with a "-" as separator to the clone name
           **kargs: arguments needed to produce the new build variant
        """
        clone = self.Clone()
        clone.name += "-" + suffix
        clone.create_kw_args.update(kargs)
        return clone

    def Create(self, runner, repository_path: str, output_prefix: str, enable_flashbundle: bool):
        builder = self.builder_class(
            repository_path, runner=runner, **self.create_kw_args)

        builder.target = self
        builder.identifier = self.name
        builder.output_dir = os.path.join(output_prefix, self.name)
        builder.enable_flashbundle(enable_flashbundle)

        return builder

    def GlobBlacklist(self, reason):
        clone = self.Clone()
        if clone.glob_blacklist_reason:
            clone.glob_blacklist_reason += ", "
            clone.glob_blacklist_reason += reason
        else:
            clone.glob_blacklist_reason = reason

        return clone

    @property
    def IsGlobBlacklisted(self):
        return self.glob_blacklist_reason is not None

    @property
    def GlobBlacklistReason(self):
        return self.glob_blacklist_reason


class AcceptAnyName:
    def Accept(self, name: str):
        return True


class AcceptNameWithSubstrings:
    def __init__(self, substr: List[str]):
        self.substr = substr

    def Accept(self, name: str):
        for s in self.substr:
            if s in name:
                return True
        return False


class BuildVariant:
    def __init__(self, name: str, validator=AcceptAnyName(), conflicts: List[str] = [], requires: List[str] = [], **buildargs):
        self.name = name
        self.validator = validator
        self.conflicts = conflicts
        self.buildargs = buildargs
        self.requires = requires


def HasConflicts(items: List[BuildVariant]) -> bool:
    for a, b in combinations(items, 2):
        if (a.name in b.conflicts) or (b.name in a.conflicts):
            return True
    return False


def AllRequirementsMet(items: List[BuildVariant]) -> bool:
    """
    Check that item.requires is satisfied for all items in the given list
    """
    available = set([item.name for item in items])

    for item in items:
        for requirement in item.requires:
            if not requirement in available:
                return False

    return True


class VariantBuilder:
    """Handles creating multiple build variants based on a starting target.
    """

    def __init__(self, targets: List[Target] = []):
        # note the clone in case the default arg is used
        self.targets = targets[:]
        self.variants = []
        self.glob_whitelist = []

    def WhitelistVariantNameForGlob(self, name):
        """
        Whitelist the specified variant to be allowed for globbing.

        By default we do not want a 'build all' to select all variants, so
        variants are generally glob-blacklisted.
        """
        self.glob_whitelist.append(name)

    def AppendVariant(self, **args):
        """
        Add another variant to accepted variants. Arguments are construction
        variants to BuildVariant.

        Example usage:

        builder.AppendVariant(name="ipv6only", enable_ipv4=False)
        """
        self.variants.append(BuildVariant(**args))

    def AllVariants(self):
        """
        Yields a list of acceptable variants for the given targets.

        Handles conflict resolution between build variants and globbing whiltelist
        targets.
        """
        for target in self.targets:
            yield target

            # skip variants that do not work for  this target
            ok_variants = [
                v for v in self.variants if v.validator.Accept(target.name)]

            # Build every possible variant
            for variant_count in range(1, len(ok_variants) + 1):
                for subgroup in combinations(ok_variants, variant_count):
                    if HasConflicts(subgroup):
                        continue

                    if not AllRequirementsMet(subgroup):
                        continue

                    # Target ready to be created - no conflicts
                    variant_target = target.Clone()
                    for option in subgroup:
                        variant_target = variant_target.Extend(
                            option.name, **option.buildargs)

                    # Only a few are whitelisted for globs
                    if '-'.join([o.name for o in subgroup]) not in self.glob_whitelist:
                        if not variant_target.glob_blacklist_reason:
                            variant_target = variant_target.GlobBlacklist(
                                'Reduce default build variants')

                    yield variant_target


def HostTargets():
    target = Target(HostBoard.NATIVE.PlatformName(), HostBuilder)
    targets = [
        target.Extend(HostBoard.NATIVE.BoardName(), board=HostBoard.NATIVE)
    ]

    # x64 linux  supports cross compile
    if (HostBoard.NATIVE.PlatformName() == 'linux') and (
            HostBoard.NATIVE.BoardName() != HostBoard.ARM64.BoardName()):
        targets.append(target.Extend('arm64', board=HostBoard.ARM64))

    app_targets = []

    # Don't cross  compile some builds
    app_targets.append(
        targets[0].Extend('rpc-console', app=HostApp.RPC_CONSOLE))
    app_targets.append(
        targets[0].Extend('tv-app', app=HostApp.TV_APP))

    for target in targets:
        app_targets.append(target.Extend(
            'all-clusters', app=HostApp.ALL_CLUSTERS))
        app_targets.append(target.Extend('chip-tool', app=HostApp.CHIP_TOOL))
        app_targets.append(target.Extend('thermostat', app=HostApp.THERMOSTAT))
        app_targets.append(target.Extend('minmdns', app=HostApp.MIN_MDNS))
        app_targets.append(target.Extend('door-lock', app=HostApp.LOCK))
        app_targets.append(target.Extend('shell', app=HostApp.SHELL))
        app_targets.append(target.Extend(
            'ota-provider', app=HostApp.OTA_PROVIDER, enable_ble=False))
        app_targets.append(target.Extend(
            'ota-requestor', app=HostApp.OTA_REQUESTOR, enable_ble=False))

    builder = VariantBuilder()

    # Possible build variants. Note that number of potential
    # builds is exponential here
    builder.AppendVariant(name="test-group", validator=AcceptNameWithSubstrings(
        ['-all-clusters', '-chip-tool']), test_group=True),
    builder.AppendVariant(name="same-event-loop", validator=AcceptNameWithSubstrings(
        ['-chip-tool']), separate_event_loop=False),
    builder.AppendVariant(name="no-interactive", validator=AcceptNameWithSubstrings(
        ['-chip-tool']), interactive_mode=False),
    builder.AppendVariant(name="ipv6only", enable_ipv4=False),
    builder.AppendVariant(name="no-ble", enable_ble=False),
    builder.AppendVariant(name="no-wifi", enable_wifi=False),
    builder.AppendVariant(name="tsan", conflicts=['asan'], use_tsan=True),
    builder.AppendVariant(name="asan", conflicts=['tsan'], use_asan=True),
    builder.AppendVariant(name="libfuzzer", requires=[
                          "clang"], use_libfuzzer=True),
    builder.AppendVariant(name="clang", use_clang=True),

    builder.WhitelistVariantNameForGlob('ipv6only')

    for target in app_targets:
        if 'rpc-console' in target.name:
            # rpc console  has only one build variant right now
            yield target
        else:
            builder.targets.append(target)

    for target in builder.AllVariants():
        yield target

    # Without extra build variants
    yield targets[0].Extend('chip-cert', app=HostApp.CERT_TOOL)
    yield targets[0].Extend('address-resolve-tool', app=HostApp.ADDRESS_RESOLVE)
    yield targets[0].Extend('address-resolve-tool-clang', app=HostApp.ADDRESS_RESOLVE, use_clang=True).GlobBlacklist("Reduce default build variants")
    yield targets[0].Extend('address-resolve-tool-platform-mdns', app=HostApp.ADDRESS_RESOLVE, use_platform_mdns=True).GlobBlacklist("Reduce default build variants")
    yield targets[0].Extend('address-resolve-tool-platform-mdns-ipv6only', app=HostApp.ADDRESS_RESOLVE, use_platform_mdns=True, enable_ipv4=False).GlobBlacklist("Reduce default build variants")

    test_target = Target(HostBoard.NATIVE.PlatformName(), HostBuilder)
    for board in [HostBoard.NATIVE, HostBoard.FAKE]:
        yield test_target.Extend(board.BoardName() + '-tests', board=board, app=HostApp.TESTS)


def Esp32Targets():
    esp32_target = Target('esp32', Esp32Builder)

    yield esp32_target.Extend('m5stack-all-clusters', board=Esp32Board.M5Stack, app=Esp32App.ALL_CLUSTERS)
    yield esp32_target.Extend('m5stack-all-clusters-ipv6only', board=Esp32Board.M5Stack, app=Esp32App.ALL_CLUSTERS, enable_ipv4=False)
    yield esp32_target.Extend('m5stack-all-clusters-rpc', board=Esp32Board.M5Stack, app=Esp32App.ALL_CLUSTERS, enable_rpcs=True)
    yield esp32_target.Extend('m5stack-all-clusters-rpc-ipv6only', board=Esp32Board.M5Stack, app=Esp32App.ALL_CLUSTERS, enable_rpcs=True, enable_ipv4=False)

    yield esp32_target.Extend('c3devkit-all-clusters', board=Esp32Board.C3DevKit, app=Esp32App.ALL_CLUSTERS)

    devkitc = esp32_target.Extend('devkitc', board=Esp32Board.DevKitC)

    yield devkitc.Extend('all-clusters', app=Esp32App.ALL_CLUSTERS)
    yield devkitc.Extend('all-clusters-ipv6only', app=Esp32App.ALL_CLUSTERS, enable_ipv4=False)
    yield devkitc.Extend('shell', app=Esp32App.SHELL)
    yield devkitc.Extend('light', app=Esp32App.LIGHT)
    yield devkitc.Extend('lock', app=Esp32App.LOCK)
    yield devkitc.Extend('bridge', app=Esp32App.BRIDGE)
    yield devkitc.Extend('temperature-measurement', app=Esp32App.TEMPERATURE_MEASUREMENT)
    yield devkitc.Extend('temperature-measurement-rpc', app=Esp32App.TEMPERATURE_MEASUREMENT, enable_rpcs=True)

    yield esp32_target.Extend('qemu-tests', board=Esp32Board.QEMU, app=Esp32App.TESTS)


def Efr32Targets():
    efr_target = Target('efr32', Efr32Builder)

    board_targets = [
        efr_target.Extend('brd4161a', board=Efr32Board.BRD4161A),
        efr_target.Extend('brd4163a', board=Efr32Board.BRD4163A).GlobBlacklist(
            'only user requested'),
        efr_target.Extend('brd4164a', board=Efr32Board.BRD4164A).GlobBlacklist(
            'only user requested'),
        efr_target.Extend('brd4166a', board=Efr32Board.BRD4166A).GlobBlacklist(
            'only user requested'),
        efr_target.Extend('brd4170a', board=Efr32Board.BRD4170A).GlobBlacklist(
            'only user requested'),
        efr_target.Extend('brd4186a', board=Efr32Board.BRD4186A).GlobBlacklist(
            'only user requested'),
        efr_target.Extend('brd4187a', board=Efr32Board.BRD4187A).GlobBlacklist(
            'only user requested'),
        efr_target.Extend('brd4304a', board=Efr32Board.BRD4304A).GlobBlacklist(
            'only user requested')
    ]

    builder = VariantBuilder()

    for board_target in board_targets:
        builder.targets.append(board_target.Extend(
            'window-covering', app=Efr32App.WINDOW_COVERING))
        builder.targets.append(board_target.Extend(
            'switch', app=Efr32App.SWITCH))
        builder.targets.append(board_target.Extend(
            'unit-test', app=Efr32App.UNIT_TEST))
        builder.targets.append(
            board_target.Extend('light', app=Efr32App.LIGHT))
        builder.targets.append(board_target.Extend('lock', app=Efr32App.LOCK))

    # Possible build variants. Note that number of potential
    # builds is exponential here
    builder.AppendVariant(name="rpc", validator=AcceptNameWithSubstrings(
        ['-light', '-lock']), enable_rpcs=True)
    builder.AppendVariant(name="with-ota-requestor", enable_ota_requestor=True)

    builder.WhitelistVariantNameForGlob('rpc')

    for target in builder.AllVariants():
        yield target


def NrfTargets():
    target = Target('nrf', NrfConnectBuilder)

    yield target.Extend('native-posix-64-tests', board=NrfBoard.NATIVE_POSIX_64, app=NrfApp.UNIT_TESTS)

    targets = [
        target.Extend('nrf5340dk', board=NrfBoard.NRF5340DK),
        target.Extend('nrf52840dk', board=NrfBoard.NRF52840DK),
    ]

    # Enable nrf52840dongle for lighting app only
    yield target.Extend('nrf52840dongle-light', board=NrfBoard.NRF52840DONGLE, app=NrfApp.LIGHT)

    for target in targets:
        yield target.Extend('lock', app=NrfApp.LOCK)
        yield target.Extend('light', app=NrfApp.LIGHT)
        yield target.Extend('shell', app=NrfApp.SHELL)
        yield target.Extend('pump', app=NrfApp.PUMP)
        yield target.Extend('pump-controller', app=NrfApp.PUMP_CONTROLLER)

        rpc = target.Extend('light-rpc', app=NrfApp.LIGHT, enable_rpcs=True)

        if '-nrf5340dk-' in rpc.name:
            rpc = rpc.GlobBlacklist(
                'Compile failure due to pw_build args not forwarded to proto compiler. https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/66760')

        yield rpc


def AndroidTargets():
    target = Target('android', AndroidBuilder)

    yield target.Extend('arm-chip-tool', board=AndroidBoard.ARM, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('arm64-chip-tool', board=AndroidBoard.ARM64, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('x64-chip-tool', board=AndroidBoard.X64, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('x86-chip-tool', board=AndroidBoard.X86, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('arm64-chip-test', board=AndroidBoard.ARM64, app=AndroidApp.CHIP_TEST)
    yield target.Extend('androidstudio-arm-chip-tool', board=AndroidBoard.AndroidStudio_ARM, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('androidstudio-arm64-chip-tool', board=AndroidBoard.AndroidStudio_ARM64, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('androidstudio-x86-chip-tool', board=AndroidBoard.AndroidStudio_X86, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('androidstudio-x64-chip-tool', board=AndroidBoard.AndroidStudio_X64, app=AndroidApp.CHIP_TOOL)
    yield target.Extend('arm64-chip-tvserver', board=AndroidBoard.ARM64, app=AndroidApp.CHIP_TVServer)
    yield target.Extend('arm-chip-tvserver', board=AndroidBoard.ARM, app=AndroidApp.CHIP_TVServer)
    yield target.Extend('arm64-chip-tv-casting-app', board=AndroidBoard.ARM64, app=AndroidApp.CHIP_TV_CASTING_APP)
    yield target.Extend('arm-chip-tv-casting-app', board=AndroidBoard.ARM, app=AndroidApp.CHIP_TV_CASTING_APP)


def MbedTargets():
    target = Target('mbed', MbedBuilder)

    targets = [
        target.Extend('CY8CPROTO_062_4343W',
                      board=MbedBoard.CY8CPROTO_062_4343W),
    ]

    app_targets = []
    for target in targets:
        app_targets.append(target.Extend('lock', app=MbedApp.LOCK))
        app_targets.append(target.Extend('light', app=MbedApp.LIGHT))
        app_targets.append(target.Extend(
            'all-clusters', app=MbedApp.ALL_CLUSTERS))
        app_targets.append(target.Extend('pigweed', app=MbedApp.PIGWEED))
        app_targets.append(target.Extend('shell', app=MbedApp.SHELL))

    for target in app_targets:
        yield target.Extend('release', profile=MbedProfile.RELEASE)
        yield target.Extend('develop', profile=MbedProfile.DEVELOP).GlobBlacklist('Compile only for debugging purpose - https://os.mbed.com/docs/mbed-os/latest/program-setup/build-profiles-and-rules.html')
        yield target.Extend('debug', profile=MbedProfile.DEBUG).GlobBlacklist('Compile only for debugging purpose - https://os.mbed.com/docs/mbed-os/latest/program-setup/build-profiles-and-rules.html')


def InfineonTargets():
    target = Target('infineon', InfineonBuilder)

    yield target.Extend('p6-lock', board=InfineonBoard.P6BOARD, app=InfineonApp.LOCK)
    yield target.Extend('p6-all-clusters', board=InfineonBoard.P6BOARD, app=InfineonApp.ALL_CLUSTERS)
    yield target.Extend('p6-light', board=InfineonBoard.P6BOARD, app=InfineonApp.LIGHT)


def AmebaTargets():
    ameba_target = Target('ameba', AmebaBuilder)

    yield ameba_target.Extend('amebad-all-clusters', board=AmebaBoard.AMEBAD, app=AmebaApp.ALL_CLUSTERS)
    yield ameba_target.Extend('amebad-light', board=AmebaBoard.AMEBAD, app=AmebaApp.LIGHT)
    yield ameba_target.Extend('amebad-pigweed', board=AmebaBoard.AMEBAD, app=AmebaApp.PIGWEED)


def K32WTargets():
    target = Target('k32w', K32WBuilder)

    # This is for testing only  in case debug builds are to be fixed
    # Error is LWIP_DEBUG being redefined between 0 and 1 in debug builds in:
    #    third_party/connectedhomeip/src/lwip/k32w0/lwipopts.h
    #    gen/include/lwip/lwip_buildconfig.h
    yield target.Extend('light', app=K32WApp.LIGHT).GlobBlacklist("Debug builds broken due to LWIP_DEBUG redefition")

    yield target.Extend('light-release', app=K32WApp.LIGHT, release=True)
    yield target.Extend('light-tokenizer-release', app=K32WApp.LIGHT, tokenizer=True, release=True).GlobBlacklist("Only on demand build")
    yield target.Extend('shell-release', app=K32WApp.SHELL, release=True)
    yield target.Extend('lock-release', app=K32WApp.LOCK, release=True)
    yield target.Extend('lock-low-power-release', app=K32WApp.LOCK, low_power=True, release=True).GlobBlacklist("Only on demand build")


def cc13x2x7_26x2x7Targets():
    target = Target('cc13x2x7_26x2x7', cc13x2x7_26x2x7Builder)

    yield target.Extend('lock-ftd', app=cc13x2x7_26x2x7App.LOCK, openthread_ftd=True)
    yield target.Extend('lock-mtd', app=cc13x2x7_26x2x7App.LOCK, openthread_ftd=False)
    yield target.Extend('pump', app=cc13x2x7_26x2x7App.PUMP)
    yield target.Extend('pump-controller', app=cc13x2x7_26x2x7App.PUMP_CONTROLLER)


def Cyw30739Targets():
    yield Target('cyw30739-cyw930739m2evb_01-light', Cyw30739Builder, board=Cyw30739Board.CYW930739M2EVB_01, app=Cyw30739App.LIGHT)
    yield Target('cyw30739-cyw930739m2evb_01-lock', Cyw30739Builder, board=Cyw30739Board.CYW930739M2EVB_01, app=Cyw30739App.LOCK)
    yield Target('cyw30739-cyw930739m2evb_01-ota-requestor', Cyw30739Builder, board=Cyw30739Board.CYW930739M2EVB_01, app=Cyw30739App.OTA_REQUESTOR).GlobBlacklist("Running out of XIP flash space")
    yield Target('cyw30739-cyw930739m2evb_01-ota-requestor-no-progress-logging', Cyw30739Builder, board=Cyw30739Board.CYW930739M2EVB_01, app=Cyw30739App.OTA_REQUESTOR, progress_logging=False)


def QorvoTargets():
    target = Target('qpg', QpgBuilder)

    yield target.Extend('lock', board=QpgBoard.QPG6105, app=QpgApp.LOCK)
    yield target.Extend('light', board=QpgBoard.QPG6105, app=QpgApp.LIGHT)
    yield target.Extend('shell', board=QpgBoard.QPG6105, app=QpgApp.SHELL)
    yield target.Extend('persistent-storage', board=QpgBoard.QPG6105, app=QpgApp.PERSISTENT_STORAGE)


def Bl602Targets():
    target = Target('bl602', Bl602Builder)

    yield target.Extend('light', board=Bl602Board.BL602BOARD, app=Bl602App.LIGHT)


ALL = []

target_generators = [
    HostTargets(),
    Esp32Targets(),
    Efr32Targets(),
    NrfTargets(),
    AndroidTargets(),
    MbedTargets(),
    InfineonTargets(),
    AmebaTargets(),
    K32WTargets(),
    cc13x2x7_26x2x7Targets(),
    Cyw30739Targets(),
    QorvoTargets(),
    Bl602Targets(),
]

for generator in target_generators:
    for target in generator:
        ALL.append(target)

# Simple targets added one by one
ALL.append(Target('telink-tlsr9518adk80d-light', TelinkBuilder,
                  board=TelinkBoard.TLSR9518ADK80D, app=TelinkApp.LIGHT))
ALL.append(Target('tizen-arm-light', TizenBuilder,
                  board=TizenBoard.ARM, app=TizenApp.LIGHT))

# have a consistent order overall
ALL.sort(key=lambda t: t.name)
