"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Interfaces for network traffic generators. PcapPlayer represents
generator which replays PCAP file on a network interface.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional


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
class PcapPlayerStats:
    """Class holding statistics of replay.

    Attributes
    ----------
    packets : int
        Sent packets.
    bytes : int
        Sent bytes.
    """

    packets: int
    bytes: int


class PcapPlayer(ABC):
    """Interface for generator which replays PCAP file on a network interface."""

    @abstractmethod
    def __init__(self, host, add_vlan: Optional[int] = None, **kwargs):
        """Init player.

        Parameters
        ----------
        host : Host
            Host class with established remote connection.
        add_vlan : int, optional
            If specified, vlan header with given tag will be added to replayed packets.
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
    def stop(self):
        """Stop sending traffic."""

        raise NotImplementedError

    @abstractmethod
    def stats(self) -> PcapPlayerStats:
        """Get stats of last generator run.

        Returns
        -------
        PcapPlayerStats
            Class containing statistics of sent packets and bytes.
        """

        raise NotImplementedError
