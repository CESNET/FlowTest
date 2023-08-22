"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Interfaces for network traffic generators. PcapPlayer represents
generator which replays PCAP file on a network interface.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Iterable, Optional, Union

from src.generator.ft_generator import FtGeneratorConfig


class GeneratorException(Exception):
    """Basic exception raised by generator."""


@dataclass(frozen=True)
class ReplaySpeed(ABC):
    """Base class for replay speed modifier."""

    def __init__(self):
        raise NotImplementedError


@dataclass(frozen=True)
class MultiplierSpeed(ReplaySpeed):
    """Replay at given speed multiplier.

    E.g. 2.0 will replay traffic at twice the speed of provided PCAP.
    0.7 will replay traffic at 70% the speed of provided PCAP.

    Attributes
    ----------
    multiplier : float
        Multiplier constant.
    """

    multiplier: float


@dataclass(frozen=True)
class MbpsSpeed(ReplaySpeed):
    """Replay at constant speed in megabits per second.

    Attributes
    ----------
    mbps : float
        Megabits per second rate constant.
    """

    mbps: float


@dataclass(frozen=True)
class PpsSpeed(ReplaySpeed):
    """Replay at constant speed in packets per second.

    Attributes
    ----------
    pps : int
        Packets per second constant.
    """

    pps: int


@dataclass(frozen=True)
class TopSpeed(ReplaySpeed):
    """Replay at high possible wire speed."""


@dataclass(frozen=True)
class GeneratorStats:
    """Class holding statistics of replay.

    Attributes
    ----------
    packets : int
        Sent packets.
    bytes : int
        Sent bytes.
    start_time : int
        Timestamp of traffic sending start (milliseconds since epoch).
    end_time : int
        Timestamp of traffic sending end (milliseconds since epoch).
    """

    packets: int
    bytes: int
    start_time: int
    end_time: int


class PcapPlayer(ABC):
    """Interface for generator which replays PCAP file on a network interface."""

    @abstractmethod
    def __init__(
        self,
        host,
        add_vlan: Optional[int] = None,
        verbose: bool = False,
        biflow_export: bool = False,
        **kwargs,
    ):
        """Init player.

        Parameters
        ----------
        host : Host
            Host class with established remote connection.
        add_vlan : int, optional
            If specified, vlan header with given tag will be added to replayed packets.
        verbose : bool, optional
            If True, logs will collect all debug messages.
        biflow_export : bool, optional
            Used to process ft-generator report. When traffic originates from a profile.
            Flag indicating whether the tested probe exports biflows.
            If the probe exports biflows, the START_TIME resp. END_TIME in the generator
            report contains min(START_TIME,START_TIME_REV) resp. max(END_TIME,END_TIME_REV).
        kwargs : dict
            Additional arguments processed by implementation.
        """

        raise NotImplementedError

    @abstractmethod
    def add_interface(self, ifc_name: str, dst_mac: Optional[str] = None):
        """Add interface on which traffic will be replayed.

        Parameters
        ----------
        ifc_name : str
            String name of interface, e.g. os name or pci address.
        dst_mac : str, optional
            If specified, destination mac address will be edited in packets which are replayed on interface.
        """

        raise NotImplementedError

    @abstractmethod
    def start(self, pcap_path: str, speed: ReplaySpeed = MultiplierSpeed(1.0), loop_count: int = 1, **kwargs):
        """Start network traffic replaying.

        Parameters
        ----------
        pcap_path : str
            Path to pcap file to replay. Path to PCAP file must be local path.
            Method will synchronize pcap file on remote machine.
        speed : ReplaySpeed, optional
            Argument to modify packets replay speed.
        loop_count : int, optional
            Packets from pcap will be replayed loop_count times.
            Processing of zero or negative value depends on implementation.
        kwargs : dict
            Additional arguments processed by implementation.
        """

        raise NotImplementedError

    @abstractmethod
    def start_profile(
        self,
        profile_path: str,
        report_path: str,
        speed: ReplaySpeed = MultiplierSpeed(1.0),
        loop_count: int = 1,
        generator_config: Optional[FtGeneratorConfig] = None,
        **kwargs,
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
            Processing of zero or negative value depends on implementation.
        generator_config : FtGeneratorConfig, optional
            Configuration of PCAP generation from profile. Passed to ft-generator.
        kwargs : dict
            Additional arguments processed by implementation.
        """

        raise NotImplementedError

    @abstractmethod
    def stop(self):
        """Stop sending traffic."""

        raise NotImplementedError

    @abstractmethod
    def download_logs(self, directory: str):
        """Download logs to given directory.

        Parameters
        ----------
        directory : str
            Path to a local directory where logs should be stored.
        """
        raise NotImplementedError

    @abstractmethod
    def stats(self) -> GeneratorStats:
        """Get stats of last generator run.

        Returns
        -------
        GeneratorStats
            Class containing statistics of sent packets and bytes.
        """

        raise NotImplementedError


class Replicator(PcapPlayer):
    """Interface for generator which replicates network traffic from PCAP file according to a configuration."""

    @dataclass(frozen=True)
    class IpModifier:
        """Base class for ip address modifier used in replication unit."""

        def __init__(self) -> None:
            raise NotImplementedError

    @dataclass(frozen=True)
    class AddConstant(IpModifier):
        """Add constant number to the IP address in replication unit.

        Attributes
        ----------
        value: int
            Constant added to each IP address.
        """

        value: int

        def __str__(self) -> str:
            return f"addConstant({self.value})"

    @dataclass(frozen=True)
    class AddCounter(IpModifier):
        """Add counter value to the ip address, counter initial value is <start>
        and counter is incremented by <step> on every call.

        Attributes
        ----------
        start: int
            Counter initial value.
        step: int
            Increment by step each call.
        """

        start: int
        step: int

        def __str__(self) -> str:
            return f"addCounter({self.start},{self.step})"

    @abstractmethod
    def add_replication_unit(
        self,
        srcip: Optional[IpModifier] = None,
        dstip: Optional[IpModifier] = None,
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
            Modifier for source MAC address. Original MAC address will be overwritten by given value.
        dstmac : str, optional
            Modifier for destination MAC address. Original MAC address will be overwritten by given value.
            Warning: dstmac could be changed to real interface MAC when sending packets over a switch.
        loop_only : int or Iterable or None, optional
            Specifies the replication loops in which the replication unit will be applied.
            Index of loop or list of indices.
        """

        raise NotImplementedError

    @abstractmethod
    def set_loop_modifiers(self, srcip_offset: Optional[int] = None, dstip_offset: Optional[int] = None) -> None:
        """Define how to rewrite IP addresses in each loop. Used to distinguish flows in individual loops.

        Parameters
        ----------
        srcip_offset : int, optional
            Add offset to the source IP address. Offset is increased by integer value on each replication loop.
        dstip_offset : int, optional
            Add offset to the destination IP address. Offset is increased by integer value on each replication loop.
        """

        raise NotImplementedError

    @abstractmethod
    def get_replicator_config(self) -> dict:
        """Get replicator configuration in form of a dict.

        Returns
        -------
        dict
            Replicator configuration.
        """

        raise NotImplementedError
