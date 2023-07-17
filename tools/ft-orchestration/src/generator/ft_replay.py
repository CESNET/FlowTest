"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Connector for ft-replay tool.
"""


import logging
import re
import shutil
import tempfile
import time
from dataclasses import dataclass
from os import path
from pathlib import Path
from typing import Iterable, Optional, Union

import invoke
import yaml
from src.common.typed_dataclass import typed_dataclass
from src.generator.ft_generator import FtGenerator, FtGeneratorConfig
from src.generator.interface import (
    GeneratorException,
    GeneratorStats,
    MbpsSpeed,
    MultiplierSpeed,
    PpsSpeed,
    ReplaySpeed,
    Replicator,
    TopSpeed,
)
from src.host.host import Host


@typed_dataclass
@dataclass
class FtReplayOutputPluginSettings:
    """Helper class containing ft-replay output plugin parameters.

    Attributes
    ----------
    output_plugin: str, optional
        Name of output plugin.
    queue_count: int, optional
        Number of output queues. Not relevant with raw plugin.
    burst_size: int, optional
        Size of burst of outgoing packets.
    packet_size: int, optional
        Maximal size (Bytes) of single packet. Not usable with nfb plugin.
    super_packet: str, optional
        Enable Super Packet feature (merging of small packets into larger ones).
        Value: no/yes/auto. Only for nfb plugin.
    super_packet_size: int, optional
        Maximal size (Bytes) of single Super Packet.
    """

    output_plugin: str = "raw"
    queue_count: int = 1
    burst_size: Optional[int] = None
    packet_size: Optional[int] = None
    super_packet: Optional[str] = None
    super_packet_size: Optional[int] = None

    def __post_init__(self) -> None:
        """Check combination of input plugin and parameters."""

        err = False
        if self.output_plugin == "raw":
            if self.queue_count > 1 or self.super_packet or self.super_packet_size:
                err = True
        elif self.output_plugin == "nfb":
            if self.packet_size:
                err = True
        elif self.output_plugin in ["xdp", "pcapFile"]:
            if self.super_packet or self.super_packet_size:
                err = True
        else:
            logging.getLogger().error("FtReplay: Used unknown output plugin '%s'", self.output_plugin)
            raise RuntimeError(f"FtReplay: Used unknown output plugin '{self.output_plugin}'")

        if err:
            logging.getLogger().error("FtReplay: Used unsupported %s output plugin parameters.", self.output_plugin)
            raise RuntimeError(f"FtReplay: Used unsupported {self.output_plugin} output plugin parameters.")

    def get_cmd_args(self, interface: str) -> str:
        """Get output plugin argument for ft-replay.

        Parameters
        ----------
        interface : str
            Identification of output interface/device/file.

        Returns
        -------
        str
            Argument string.
        """

        args = []
        if self.output_plugin != "raw":
            args.append(f"queueCount={self.queue_count}")
        if self.burst_size:
            args.append(f"burstSize={self.burst_size}")
        if self.packet_size:
            args.append(f"packetSize={self.packet_size}")
        if self.super_packet:
            args.append(f"superPacket={self.super_packet}")
        if self.super_packet_size:
            args.append(f"superPacketSize={self.super_packet_size}")

        if self.output_plugin in ["raw", "xdp"]:
            args.append(f"ifc={interface}")
        if self.output_plugin == "nfb":
            args.append(f"device={interface}")
        if self.output_plugin == "pcapFile":
            args.append(f"file={interface}")

        return f"{self.output_plugin}:{','.join(args)}"


@dataclass
class _ReplicationUnit:
    """Internal representation of replication unit.

    Parameters
    ----------
    srcip : Replicator.IpModifier, optional
        Source IP address modifier. E.g. AddCounter or AddConstant.
    dstip : Replicator.IpModifier, optional
        Destination IP address modifier. E.g. AddCounter or AddConstant.
    srcmac : str, optional
        Source mac address for packets replicated by unit, in str form AA:BB:CC:DD:EE:FF.
    dstmac : str, optional
        Destination mac address for packets replicated by unit, in str form AA:BB:CC:DD:EE:FF.
    loop_only : int or Iterable or None, optional
        Specifies the replication loops in which the replication unit will be applied.
        Index of loop or list of indices.
    """

    srcip: Optional[Replicator.IpModifier] = None
    dstip: Optional[Replicator.IpModifier] = None
    srcmac: Optional[str] = None
    dstmac: Optional[str] = None
    loop_only: Optional[Union[int, Iterable]] = None

    def to_dict(self) -> dict:
        """Convert structure to dict accepted by ft-replay.

        Returns
        -------
        dict
            Replication unit configuration dict.
        """

        res = {}
        res["srcip"] = str(self.srcip)
        res["dstip"] = str(self.dstip)
        res["srcmac"] = str(self.srcmac)
        res["dstmac"] = str(self.dstmac)
        # loopOnly is not implemented by ft-replay yet
        # res["loopOnly"] = str(self.loop_only)
        return res


# pylint: disable=too-many-instance-attributes
class FtReplay(Replicator):
    """Connector for ft-replay tool."""

    def __init__(
        self, host: Host, add_vlan: Optional[int] = None, verbose: bool = False, cache_path: str = None, **kwargs: dict
    ) -> None:
        """FtReplay initializer.

        Parameters
        ----------
        host : Host
            Host class with established connection.
        add_vlan : int, optional
            If specified, vlan header with given tag will be added to replayed packets.
        verbose : bool, optional
            If True, logs will collect all debug messages.
        cache_path : str, optional
            Custom cache path for PCAPs generated by ft-generator on (remote) host.
        kwargs : dict
            Additional arguments used to set up ft-replay output plugin.

        Raises
        ------
        RuntimeError
            When ft-replay binary missing on host.
        """

        self._host = host
        self._vlan = add_vlan
        self._verbose = verbose
        self._output_plugin = FtReplayOutputPluginSettings(**kwargs)
        self._interface = None
        self._dst_mac = None
        self._process = None

        self._replication_units = []
        self._srcip_offset = None
        self._dstip_offset = None

        self._work_dir = tempfile.mkdtemp()
        self._config_file = path.join(self._work_dir, "config.yaml")

        self._ft_generator = FtGenerator(host, cache_path)

        res = host.run("command -v ft-replay", check_rc=False)
        if res.exited != 0:
            logging.getLogger().error("ft-replay is missing")
            raise RuntimeError("Unable to locate or execute ft-replay binary")
        # use absolute binary path when run via sudo, needed if ft-replay was installed with cmake
        self._bin = res.stdout.strip()

        if self._host.is_local():
            self._log_file = path.join(tempfile.mkdtemp(), "ft-replay.log")
            self._generator_log_file = path.join(tempfile.mkdtemp(), "ft-generator.log")
        else:
            self._log_file = path.join(self._host.get_storage().get_remote_directory(), "ft-replay.log")
            self._generator_log_file = path.join(self._host.get_storage().get_remote_directory(), "ft-generator.log")

    def add_interface(self, ifc_name: str, dst_mac: Optional[str] = None) -> None:
        """Add interface on which traffic will be replayed.

        Parameters
        ----------
        ifc_name : str
            String name of interface, e.g. os name or pci address.
        dst_mac : str, optional
            If specified, destination mac address will be edited in packets which are replayed on interface.

        Raises
        ------
        RuntimeError
            If more than one interface is added.
        """

        if self._interface:
            raise RuntimeError(
                "Ft-replay supports only one replaying interface at the moment. "
                "Behavior will be changed with future updates of ft-replay."
            )
        self._interface = ifc_name
        self._dst_mac = dst_mac

    def add_replication_unit(
        self,
        srcip: Optional[Replicator.IpModifier] = None,
        dstip: Optional[Replicator.IpModifier] = None,
        srcmac: Optional[str] = None,
        dstmac: Optional[str] = None,
        loop_only: Optional[Union[int, Iterable]] = None,
    ) -> None:
        """Add replication unit.
        Replication unit specifies how to modify each single packet, the number of replication units
        specifies the multiplication rate of the input PCAP file.

        Parameters
        ----------
        srcip : IpModifier, optional
            Modifier for source IP address.
        dstip : IpModifier, optional
            Modifier for destination IP address.
        srcmac : str, optional
            Modifier for source MAC address. Original MAC address could be overwritten by given in string form.
        dstmac : str, optional
            Modifier for destination MAC address. Original MAC address could be overwritten by given in string form.
            Warning: dstmac could be changed to real interface MAC when sending packets over a switch.
        loop_only : int or Iterable or None, optional
            Specifies the replication loops in which the replication unit will be applied.
            Index of loop or list of indices.
        """

        self._replication_units.append(_ReplicationUnit(srcip, dstip, srcmac, dstmac, loop_only))

    def set_loop_modifiers(self, srcip_offset: Optional[int] = None, dstip_offset: Optional[int] = None) -> None:
        """Define how to rewrite IP addresses in each loop. Used to distinguish flows in individual loops.

        Parameters
        ----------
        srcip_offset : int, optional
            Add offset to the source IP address. Offset is increased by integer value on each replication loop.
        dstip_offset : int, optional
            Add offset to the destination IP address. Offset is increased by integer value on each replication loop.
        """

        self._srcip_offset = srcip_offset
        self._dstip_offset = dstip_offset

    def get_replicator_config(self) -> dict:
        """Get replicator configuration in form of a dict accepted by ft-replay.

        Returns
        -------
        dict
            Replicator configuration.
        """

        config = {
            "units": [{}],
            "loop": {},
        }

        if len(self._replication_units) > 0:
            config["units"] = [unit.to_dict() for unit in self._replication_units]

        if self._dst_mac:
            # override dstmac modifier in replication units in case we need packets with real mac address
            for unit in config["units"]:
                unit["dstmac"] = self._dst_mac

        if self._srcip_offset:
            config["loop"]["srcip"] = f"addOffset({self._srcip_offset})"
        if self._dstip_offset:
            config["loop"]["dstip"] = f"addOffset({self._dstip_offset})"

        return config

    # pylint: disable=arguments-differ
    def start(
        self,
        pcap_path: str,
        speed: ReplaySpeed = MultiplierSpeed(1.0),
        loop_count: int = 1,
        asynchronous: bool = False,
        check_rc: bool = True,
        timeout: Optional[float] = None,
    ) -> None:
        """Start ft-replay with given command line options.

        ft-replay is started with root permissions.

        Parameters
        ----------
        pcap_path : str
            Path to pcap file to replay. Path to PCAP file must be local path.
            Method will synchronize pcap file on remote machine.
        speed : ReplaySpeed, optional
            Argument to modify packets replay speed.
        loop_count : int, optional
            Packets from pcap will be replayed loop_count times.
            Zero or negative value means infinite loop and must be started asynchronous.
        asynchronous : bool, optional
            If True, start ft-replay in asynchronous (non-blocking) mode.
        check_rc : bool, optional
            If True, raise ``invoke.exceptions.UnexpectedExit`` when
            return code is nonzero. If False, do not raise any exception.
        timeout : float, optional
            Raise ``CommandTimedOut`` if command doesn't finish within
            specified timeout (in seconds).

        Raises
        ------
        GeneratorException
            Bad configuration or ft-replay process exited unexpectedly with an error.
        """

        if self._process is not None:
            self.stop()

        if not self._interface:
            logging.getLogger().error("no output interface was specified")
            raise GeneratorException("no output interface was specified")

        # negative values mean infinite loop
        loop_count = max(loop_count, 0)

        if loop_count == 0 and not asynchronous:
            logging.getLogger().error("ft-replay can not be started synchronous in infinite loop")
            raise GeneratorException("ft-replay can not be started synchronous in infinite loop")

        self._save_config()
        cmd_args = ["-c", self._config_file]
        cmd_args += ["-l", str(loop_count)]
        cmd_args += [self._get_speed_arg(speed)]
        if self._vlan:
            cmd_args += ["-v", str(self._vlan)]
        cmd_args += ["-o", self._output_plugin.get_cmd_args(self._interface)]
        cmd_args += ["-i", pcap_path]

        if self._output_plugin.output_plugin in ["raw", "xdp"]:
            self._host.run(f"sudo ip link set dev {self._interface} up")

        start = time.time()
        res = self._process = self._host.run(
            f"(set -o pipefail; sudo {self._bin} {' '.join(cmd_args)} |& tee -i {self._log_file})",
            asynchronous,
            check_rc,
            timeout=timeout,
        )
        end = time.time()

        if not asynchronous:
            logging.getLogger().info("Traffic sent by ft-replay in %.2f seconds.", (end - start))
            logging.getLogger().info("Ft-replay output:")
            logging.getLogger().info(res.stdout)
        else:
            runner = self._process.runner
            if runner.process_is_finished:
                res = self._process.join()
                if res.failed:
                    # If pty is true, stderr is redirected to stdout
                    err = res.stdout if runner.opts["pty"] else res.stderr
                    logging.getLogger().error("ft-replay return code: %d, error: %s", res.return_code, err)
                    raise GeneratorException("ft-replay startup error")

    def start_profile(
        self,
        profile_path: str,
        report_path: str,
        speed: ReplaySpeed = MultiplierSpeed(1.0),
        loop_count: int = 1,
        generator_config: Optional[FtGeneratorConfig] = None,
        asynchronous: bool = False,
        check_rc: bool = True,
        timeout: Optional[float] = None,
    ) -> None:
        """Start network traffic replaying from given profile.

        Parameters
        ----------
        profile_path : str
            Path to network profile in CSV form. Path to CSV file must be local path.
            Method will synchronize CSV file when remote is used.
        report_path : str
            Local path to save report CSV (table with generated flows).
            Preprocessed to use with StatisticalModel/FlowReplicator.
        speed : ReplaySpeed, optional
            Argument to modify packets replay speed.
        loop_count : int, optional
            Packets from pcap will be replayed loop_count times.
            Zero or negative value means infinite loop and must be started asynchronous.
        generator_config : FtGeneratorConfig, optional
            Configuration of PCAP generation from profile. Passed to ft-generator.
        asynchronous : bool, optional
            If True, start tcpreplay in asynchronous (non-blocking) mode.
        check_rc : bool, optional
            If True, raise ``invoke.exceptions.UnexpectedExit`` when
            return code is nonzero. If False, do not raise any exception.
        timeout : float, optional
            Raise ``CommandTimedOut`` if command doesn't finish within
            specified timeout (in seconds).

        Raises
        ------
        GeneratorException
            When failed generating traffic from profile.
        """

        start = time.time()
        pcap, report = self._ft_generator.generate(profile_path, generator_config, self._generator_log_file)
        end = time.time()
        logging.getLogger().info("Generated traffic from profile ft-generator in %.2f seconds.", (end - start))

        self.start(pcap, speed, loop_count, asynchronous, check_rc, timeout)

        if self._host.is_local():
            shutil.copy(report, report_path)
        else:
            self._host.get_storage().pull(report, path.dirname(report_path))
            shutil.move(path.join(path.dirname(report_path), path.basename(report)), report_path)

    def stop(self) -> None:
        """Stop current execution of ft-replay.

        If ft-replay was started in synchronous mode, do nothing.
        Otherwise stop the process.

        Raises
        ------
        GeneratorException
            ft-replay process exited unexpectedly with an error.
        """

        if self._process is None:
            return

        if not isinstance(self._process, invoke.runners.Promise) or not self._process.runner.opts["asynchronous"]:
            self._process = None
            return

        self._host.stop(self._process)
        res = self._host.wait_until_finished(self._process)
        if not isinstance(res, invoke.Result):
            raise TypeError("Host method wait_until_finished returned non result object.")
        runner = self._process.runner

        if res.failed:
            # If pty is true, stderr is redirected to stdout
            # Since stdout could be filled with normal output, print only last 1 line
            err = runner.stdout[-1] if runner.opts["pty"] else runner.stderr
            logging.getLogger().error("ft-replay runtime error: %s, error: %s", res.return_code, err)
            raise GeneratorException("ft-replay runtime error")

        self._process = None

    def download_logs(self, directory: str) -> None:
        """Download logs generated by ft-replay.

        Parameters
        ----------
        directory : str
            Path to a local directory where logs should be stored.
        """

        log_files = [self._log_file, self._generator_log_file]

        if self._host.is_local():
            Path(directory).mkdir(parents=True, exist_ok=True)
            for file in log_files:
                shutil.copy(file, directory)
        else:
            for file in log_files:
                try:
                    self._host.get_storage().pull(file, directory)
                except RuntimeError as err:
                    logging.getLogger().warning("%s", err)

    def stats(self) -> GeneratorStats:
        """Get stats of last generator run.

        Returns
        -------
        GeneratorStats
            Class containing statistics of sent packets and bytes.
        """

        process = self._process
        if not hasattr(process, "stdout"):
            process = self._host.wait_until_finished(process)

        pkts = int(re.findall(r"(\d+) packets", process.stdout)[0])
        bts = int(re.findall(r"(\d+) bytes", process.stdout)[0])

        start_time = int(re.findall(r"Start time:.*\[ms since epoch: (\d+)\]", process.stdout)[0])
        end_time = int(re.findall(r"End time:.*\[ms since epoch: (\d+)\]", process.stdout)[0])

        return GeneratorStats(pkts, bts, start_time, end_time)

    def _save_config(self) -> None:
        """Prepare and save replicator configuration.

        Raises
        ------
        OSError
            Could not save configuration file.
        """

        with open(self._config_file, "w", encoding="ascii") as file:
            yaml.safe_dump(self.get_replicator_config(), file)

    @staticmethod
    def _get_speed_arg(speed: ReplaySpeed) -> str:
        """Prepare ft-replay argument according to given replay speed modifier.

        Parameters
        ----------
        speed : ReplaySpeed
            Replay speed modifier.

        Returns
        -------
        str
            ft-replay speed argument.

        Raises
        ------
        TypeError
            If speed parameter has unsupported type.
        """

        if isinstance(speed, MultiplierSpeed):
            raise TypeError("MultiplierSpeed is not implemented yet.")
        if isinstance(speed, MbpsSpeed):
            return f"--mbps={speed.mbps}"
        if isinstance(speed, PpsSpeed):
            return f"--pps={speed.pps}"
        if isinstance(speed, TopSpeed):
            return "-r"

        raise TypeError("Unsupported speed type.")
