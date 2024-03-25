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
from dataclasses import dataclass, field, fields
from os import path
from pathlib import Path
from typing import Iterable, Optional, Union

import yaml
from lbr_testsuite.executable import (
    AsyncTool,
    ExecutableProcessError,
    Executor,
    Rsync,
    Tool,
)
from src.common.tool_is_installed import assert_tool_is_installed
from src.common.typed_dataclass import bool_convertor, typed_dataclass
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


# pylint: disable=too-many-instance-attributes
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
    super_packet: str, optional
        Enable Super Packet feature (merging of small packets into larger ones).
        Value: no/yes/auto. Only for nfb plugin.
    umem_size: int, optional
        Size of UMEM array, used to exchange packets between kernel and user space.
        Only for xdp plugin.
    xsk_queue_size: int, optional
        Size of TX and Completion queue, used to transfer UMEM descriptors between kernel and user space.
        Only for xdp plugin.
    zero_copy: bool, optional
        Use Zero Copy mode with xdp plugin.
    native_mode: bool, optional
        Use Native driver mode with xdp plugin.
    mlx_legacy: bool, optional
        Enable support for legacy Mellanox/NVIDIA drivers with shifted zero-copy queues.
        Only for xdp plugin.
    block_size: int, optional
        PACKET_MMAP block size. Frames/packets are grouped in blocks.
        Only for "packet" plugin.
    frame_count: int, optional
        PACKET_MMAP frame count. Corresponds to the length of the output queue.
        Only for "packet" plugin.
    qdisk_bypass: bool, optional
        Bypass kernel's qdisk (traffic control) layer.
        Only for "packet" plugin.
    packet_loss: bool, optional
        Ignore malformed packet on a transmit ring.
        Only for "packet" plugin.
    """

    output_plugin: str = "raw"
    queue_count: int = field(default=1, metadata={"plugins": ["pcapFile", "xdp", "nfb", "packet"]})
    burst_size: Optional[int] = field(default=None, metadata={"plugins": ["pcapFile", "raw", "xdp", "nfb", "packet"]})
    super_packet: Optional[str] = field(default=None, metadata={"plugins": ["nfb"]})
    umem_size: Optional[int] = field(default=None, metadata={"plugins": ["xdp"]})
    xsk_queue_size: Optional[int] = field(default=None, metadata={"plugins": ["xdp"]})
    zero_copy: Optional[bool] = field(default=None, metadata={"convert_func": bool_convertor, "plugins": ["xdp"]})
    native_mode: Optional[bool] = field(default=None, metadata={"convert_func": bool_convertor, "plugins": ["xdp"]})
    mlx_legacy: Optional[bool] = field(default=None, metadata={"convert_func": bool_convertor, "plugins": ["xdp"]})
    block_size: Optional[int] = field(default=None, metadata={"plugins": ["packet"]})
    frame_count: Optional[int] = field(default=None, metadata={"plugins": ["packet"]})
    qdisk_bypass: Optional[bool] = field(default=None, metadata={"convert_func": bool_convertor, "plugins": ["packet"]})
    packet_loss: Optional[bool] = field(default=None, metadata={"convert_func": bool_convertor, "plugins": ["packet"]})

    def __post_init__(self) -> None:
        """Check combination of input plugin and parameters."""

        if self.output_plugin not in ["pcapFile", "raw", "xdp", "nfb", "packet"]:
            logging.getLogger().error("FtReplay: Used unknown output plugin '%s'", self.output_plugin)
            raise RuntimeError(f"FtReplay: Used unknown output plugin '{self.output_plugin}'")

        params = [field for field in fields(self) if "plugins" in field.metadata]
        for param in params:
            if getattr(self, param.name) != param.default and self.output_plugin not in param.metadata["plugins"]:
                logging.getLogger().error("FtReplay: Used unsupported %s output plugin parameters.", self.output_plugin)
                raise RuntimeError(f"FtReplay: Used unsupported {self.output_plugin} output plugin parameters.")

    # pylint: disable=too-many-branches
    def get_cmd_args(self, interface: str, mtu: int) -> str:
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
        if self.super_packet:
            args.append(f"superPacket={self.super_packet}")

        args.append(f"packetSize={mtu}")

        if self.output_plugin in ["raw", "xdp", "packet"]:
            args.append(f"ifc={interface}")
        if self.output_plugin == "nfb":
            args.append(f"device={interface}")
        if self.output_plugin == "pcapFile":
            args.append(f"file={interface}")

        if self.umem_size is not None:
            args.append(f"umemSize={self.umem_size}")
        if self.xsk_queue_size is not None:
            args.append(f"xskQueueSize={self.xsk_queue_size}")
        if self.zero_copy is not None:
            str_zero_copy = "true" if self.zero_copy else "false"
            args.append(f"zeroCopy={str_zero_copy}")
        if self.native_mode is not None:
            str_native_mode = "true" if self.native_mode else "false"
            args.append(f"nativeMode={str_native_mode}")
        if self.mlx_legacy is not None:
            str_mlx_legacy = "true" if self.mlx_legacy else "false"
            args.append(f"mlxLegacy={str_mlx_legacy}")

        if self.block_size is not None:
            args.append(f"blockSize={self.block_size}")
        if self.frame_count is not None:
            args.append(f"frameCount={self.frame_count}")
        if self.qdisk_bypass is not None:
            str_qdisk_bypass = "true" if self.qdisk_bypass else "false"
            args.append(f"qdiskBypass={str_qdisk_bypass}")
        if self.packet_loss is not None:
            str_packet_loss = "true" if self.packet_loss else "false"
            args.append(f"packetLoss={str_packet_loss}")

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

        if self.loop_only is None:
            res["loopOnly"] = "All"
        elif isinstance(self.loop_only, int):
            res["loopOnly"] = self.loop_only
        else:
            res["loopOnly"] = list(self.loop_only)

        return res


# pylint: disable=too-many-instance-attributes
class FtReplay(Replicator):
    """Connector for ft-replay tool."""

    def __init__(
        self,
        executor: Executor,
        add_vlan: Optional[int] = None,
        verbose: bool = False,
        biflow_export: bool = False,
        mtu: int = 1522,
        cache_path: str = None,
        **kwargs: dict,
    ) -> None:
        """FtReplay initializer.

        Parameters
        ----------
        executor: Executor
            Executor for command execution.
        add_vlan : int, optional
            If specified, vlan header with given tag will be added to replayed packets.
        verbose : bool, optional
            If True, logs will collect all debug messages.
        biflow_export : bool, optional
            Used to process ft-generator report. When traffic originates from a profile.
            Flag indicating whether the tested probe exports biflows.
            If the probe exports biflows, the START_TIME resp. END_TIME in the generator
            report contains min(START_TIME,START_TIME_REV) resp. max(END_TIME,END_TIME_REV).
        mtu: int, optional
            Mtu size of interface on which traffic will be replayed.
            Default size is defined by standard MTU with ethernet and VLAN header.
        cache_path : str, optional
            Custom cache path for PCAPs generated by ft-generator on (remote) host.
        kwargs : dict
            Additional arguments used to set up ft-replay output plugin.

        Raises
        ------
        RuntimeError
            When ft-replay binary missing on host.
        """

        self._executor = executor
        self._vlan = add_vlan
        self._verbose = verbose
        self._output_plugin = FtReplayOutputPluginSettings(**kwargs)
        self._interface = None
        self._dst_mac = None
        self._process = None
        self._rsync = Rsync(executor)

        if self._output_plugin.output_plugin == "xdp" and mtu != 2048:
            logging.getLogger().warning("Xdp output plugin supports only MTU of value 2048. Parameter is ignored.")
            self._mtu = 2048
        else:
            self._mtu = mtu

        self._replication_units = []
        self._srcip_offset = None
        self._dstip_offset = None

        self._work_dir = tempfile.mkdtemp()
        self._config_file = path.join(self._work_dir, "config.yaml")

        self._ft_generator = FtGenerator(executor, cache_path, biflow_export, verbose)

        # use absolute binary path when run via sudo, needed if ft-replay was installed with cmake
        self._bin = assert_tool_is_installed("ft-replay", executor)

        self._log_file = path.join(self._work_dir, "ft-replay.log")
        self._generator_log_file = path.join(self._work_dir, "ft-generator.log")

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
        if dst_mac:
            self._dst_mac = [dst_mac]

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
            if len(config["units"]) % len(self._dst_mac) > 0:
                logging.getLogger().warning(
                    "Packets distribution to probe interfaces cannot be uniform. "
                    "Number of replication units %d is not divisible by %d.",
                    len(config["units"]),
                    len(self._dst_mac),
                )
            for idx, unit in enumerate(config["units"]):
                mac = self._dst_mac[idx % len(self._dst_mac)]
                unit["dstmac"] = mac

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
        remote_pcap: bool = False,
    ) -> None:
        """Start ft-replay with given command line options.

        ft-replay is started with root permissions.

        Parameters
        ----------
        pcap_path : str
            Path to pcap file to replay. When the remote_pcap parameter
            is True, the path is assumed to be remote. Otherwise is path
            local and method will synchronize pcap file on remote machine.
        speed : ReplaySpeed, optional
            Argument to modify packets replay speed.
        loop_count : int, optional
            Packets from pcap will be replayed loop_count times.
            Zero or negative value means infinite loop.
        remote_pcap: bool, optional
            True if PCAP file (pcap_path) is stored on remote machine
            and no synchronization is required.

        Raises
        ------
        GeneratorException
            Bad configuration or ft-replay process exited unexpectedly with an error.
        """

        if self._process is not None:
            if self._process.is_running():
                self.stop()

        if not self._interface:
            logging.getLogger().error("no output interface was specified")
            raise GeneratorException("no output interface was specified")

        # negative values mean infinite loop
        loop_count = max(loop_count, 0)

        self._save_config()

        cmd_args = ["-c", self._rsync.push_path(self._config_file)]
        cmd_args += ["-l", str(loop_count)]
        cmd_args += [self._get_speed_arg(speed)]
        if self._vlan:
            cmd_args += ["-v", str(self._vlan)]
        cmd_args += ["-o", self._output_plugin.get_cmd_args(self._interface, self._mtu)]

        pcap_path = pcap_path if remote_pcap else self._rsync.push_path(pcap_path)
        cmd_args += ["-i", pcap_path]

        if self._output_plugin.output_plugin in ["raw", "xdp"]:
            Tool(f"ip link set dev {self._interface} up", executor=self._executor, sudo=True).run()
            Tool(f"ip link set dev {self._interface} mtu {self._mtu}", executor=self._executor, sudo=True).run()

        self._process = AsyncTool(
            f"{self._bin} {' '.join(cmd_args)}",
            sudo=True,
            executor=self._executor,
        )
        self._process.set_outputs(self._log_file)

        try:
            self._process.run()
        except ExecutableProcessError as err:
            raise GeneratorException("ft-replay startup error") from err

    def start_profile(
        self,
        profile_path: str,
        report_path: str,
        speed: ReplaySpeed = MultiplierSpeed(1.0),
        loop_count: int = 1,
        generator_config: Optional[FtGeneratorConfig] = None,
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

        Raises
        ------
        GeneratorException
            When failed generating traffic from profile.
        """

        logging.getLogger().info("PCAP generator started, profile: %s", profile_path)
        start = time.time()
        pcap, report = self._ft_generator.generate(profile_path, generator_config, self._generator_log_file)
        end = time.time()
        logging.getLogger().info("PCAP generated in %.2f seconds.", (end - start))

        self.start(pcap, speed, loop_count, remote_pcap=True)

        cache_rsync = Rsync(self._executor, data_dir=path.dirname(report))
        report = cache_rsync.pull_path(report, self._work_dir)
        shutil.copy(report, report_path)

    def stop(self, timeout=None) -> None:
        """Stop current execution of ft-replay.

        Parameters
        ----------
        timeout : float, optional
            Kill process if it doesn't finish within
            specified timeout (in seconds).

        Raises
        ------
        GeneratorException
            ft-replay process exited unexpectedly with an error.
        """

        if self._process is None:
            return

        try:
            self._process.wait_or_kill(timeout)
        except ExecutableProcessError as err:
            raise GeneratorException("ft-replay runtime error") from err

    def download_logs(self, directory: str) -> None:
        """Download logs generated by ft-replay.

        Parameters
        ----------
        directory : str
            Path to a local directory where logs should be stored.
        """

        log_files = [self._log_file, self._generator_log_file]

        Path(directory).mkdir(parents=True, exist_ok=True)
        for file in log_files:
            if Path(file).exists():
                shutil.copy(file, directory)

        if self._verbose:
            shutil.copy(self._config_file, directory)

    def stats(self) -> GeneratorStats:
        """Get stats of last generator run.

        Returns
        -------
        GeneratorStats
            Class containing statistics of sent packets and bytes.
        """

        self._process.wait_or_kill()
        output = Path(self._log_file).read_text(encoding="utf-8")

        logging.getLogger().info("Ft-replay output:")
        logging.getLogger().info(output)

        pkts = int(re.findall(r"(\d+) packets", output)[-1])
        bts = int(re.findall(r"(\d+) bytes", output)[-1])

        start_time = int(re.findall(r"Start time:.*\[ms since epoch: (\d+)\]", output)[0])
        end_time = int(re.findall(r"End time:.*\[ms since epoch: (\d+)\]", output)[0])

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
            param_name = "multiplier"
        elif isinstance(speed, MbpsSpeed):
            param_name = "mbps"
        elif isinstance(speed, PpsSpeed):
            param_name = "pps"
        elif isinstance(speed, TopSpeed):
            param_name = "topspeed"
        else:
            raise TypeError("Unsupported speed type.")

        return f"--{param_name}{speed.speed and f'={speed.speed}'}"
