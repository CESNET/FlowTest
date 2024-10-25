"""
Author(s):
    Matej Hulák <hulak@cesnet.cz>
    Michal Sedlak <sedlakm@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains Flow class.
"""

import ipaddress as ip
from dataclasses import dataclass
from typing import Optional

from netaddr import EUI


@dataclass
class Flow:
    """Class represents biflow record.

    Attributes
    ----------
    src_ip : ip.ip_address
        Source IP address.
    dst_ip : ip.ip_address
        Destination IP address.
    src_mac : EUI
        Source MAC address.
    dst_mac : EUI
        Destination MAC address.
    src_port : int
        Source port number.
    dst_port : int
        Destination port number.
    l3_proto : int
        L3 protocol number.
    l4_proto : int
        L4 protocol number.
    start_time : int
        Time of the first observed packet.
    end_time : int
        Time of the last observed packet.
    packets : int
        Number of packets observed in the forward direction.
    packets_rev : int
        Number of packets observed in the opposite direction.
    bytes: int
        Number of bytes observed in the forward direction.
    bytes_rev : int
        Number of bytes observed in the opposite direction.
    """

    # pylint: disable=too-many-instance-attributes
    src_ip: ip.ip_address = ip.ip_address("0.0.0.0")
    dst_ip: ip.ip_address = ip.ip_address("0.0.0.0")
    src_mac: EUI = EUI("0-0-0-0-0-0")
    dst_mac: EUI = EUI("0-0-0-0-0-0")
    src_port: int = 0
    dst_port: int = 0
    l3_proto: int = 0
    l4_proto: int = 0
    start_time: int = 0
    end_time: int = 0

    packets: int = 0
    packets_rev: int = 0
    bytes: int = 0
    bytes_rev: int = 0

    @property
    def normalized_key(self) -> tuple:
        """Returns a 'normalized' flow key based on the src/dst ip address,
        src/dst port, l3 protocol, and l4 protocol, which is
        direction-indifferent. If two flows have the same key and only the
        src/dst direction is flipped, they will have the same key.

        Returns
        -------
        tuple
            normalized key of the flow
        """
        if self.src_ip < self.dst_ip or (self.src_ip == self.dst_ip and self.src_port < self.dst_port):
            return (
                self.src_ip,
                self.dst_ip,
                self.src_port,
                self.dst_port,
                self.l3_proto,
                self.l4_proto,
            )
        return (
            self.dst_ip,
            self.src_ip,
            self.dst_port,
            self.src_port,
            self.l3_proto,
            self.l4_proto,
        )

    def __hash__(self) -> int:
        """Function creates hash of Flow, which is derived from src/dst ip, src/dst ports, l3/l4 protocols.
        Class Flow represent biflow record and its direction is based on comparison of source and destination IP.
        If src/dst ip is the same, order is based on src/dst port comparison.

        Returns
        -------
        int
            Hash value of Flow.
        """
        return hash(self.normalized_key)

    def __eq__(self, other) -> bool:
        """Compare 2 flows.
        Flows are equal if src/dst ip, src/dst port, l3 protocol and l4 protocol are the same.

        Parameters
        ----------
        other : Flow
            Flow to be compared with.

        Returns
        -------
        bool
            True if flows are equal, False otherwise.
        """
        return self.normalized_key == other.normalized_key

    def check_flow_direction(self, other) -> bool:
        """Functions checks direction of flows.

        Parameters
        ----------
        other : Flow
            Flow to be compared with.

        Returns
        -------
        bool
            True if flows have same direction, False otherwise.

        Raises
        ------
        RuntimeError
            Raised when flow IPs dont match.
        """

        if self.src_ip == self.dst_ip == other.src_ip == other.dst_ip:
            return self.src_port == other.src_port
        if self.src_ip == other.src_ip and self.dst_ip == other.dst_ip:
            return True
        if self.src_ip == other.dst_ip and self.dst_ip == other.src_ip:
            return False
        raise RuntimeError("Flow::check_flow_direction(): Flows dont match")

    def get_profile(self) -> list[str]:
        """Function return profile like list with its attributes.

        Returns
        -------
        list[str]
            List of values, formatted as profile.
        """

        return [
            str(self.start_time),
            str(self.end_time),
            str(self.l3_proto),
            str(self.l4_proto),
            str(self.src_port),
            str(self.dst_port),
            str(self.packets),
            str(self.bytes),
            str(self.packets_rev),
            str(self.bytes_rev),
        ]

    def compare_content(self, other) -> bool:
        """Compare content of 2 flows.
        Works as __eq__ but in addition compares also bytes, bytes_rev, packets, packets_rev.

        Parameters
        ----------
        other : Flow
            Flow to be compared with.

        Returns
        -------
        bool
            True if flows are equal, False otherwise.
        """

        if self != other:
            return False

        if self.check_flow_direction(other):
            return (self.packets, self.packets_rev, self.bytes, self.bytes_rev) == (
                other.packets,
                other.packets_rev,
                other.bytes,
                other.bytes_rev,
            )

        return (self.packets, self.packets_rev, self.bytes, self.bytes_rev) == (
            other.packets_rev,
            other.packets,
            other.bytes_rev,
            other.bytes,
        )


@dataclass
class ExtendedFlow(Flow):
    """Child class of Flow, contains extra attributes, used for testing purposes.

    Attributes
    ----------
    vlan_id : Optional[int]
        ID number of VLAN layer.
    mpls_label : Optional[int]
        Label of MPLS layer.
    frag_pkt_count : int
        Fragmented packets count.
    frag_min_size_packets_count: int
        Count of packets which are bigger then minimal size of packet to be considered for fragmentation.
    """

    vlan_id: Optional[int] = None
    mpls_label: Optional[int] = None
    frag_pkt_count: int = 0
    frag_min_size_packets_count = 0

    __hash__ = Flow.__hash__
    __eq__ = Flow.__eq__

    def aggregate(self, other):
        """Updates Flow with attributes from another Flow.
        Used to aggregate Flow attributes from multiple partial Flows.

        Parameters
        ----------
        other : Flow
            Flow to update from.

        Raises
        ------
        RuntimeError
            Raised when trying to aggregate flows with different IPs.
        ValueError
            Raised when mpls or vlan ids are different in one flow.
        """

        if self.vlan_id != other.vlan_id:
            raise ValueError("Flow::aggregate(): Packets from one flow have different vlan ids.")

        if self.mpls_label != other.mpls_label:
            raise ValueError("Flow::aggregate(): Packets from one flow have different mpls labels.")

        self.start_time = min(self.start_time, other.start_time)
        self.end_time = max(self.end_time, other.end_time)

        if self.check_flow_direction(other):
            self.packets += other.packets
            self.packets_rev += other.packets_rev
            self.bytes += other.bytes
            self.bytes_rev += other.bytes_rev
        else:
            self.packets += other.packets_rev
            self.packets_rev += other.packets
            self.bytes += other.bytes_rev
            self.bytes_rev += other.bytes

        self.frag_pkt_count += other.frag_pkt_count
        self.frag_min_size_packets_count += other.frag_min_size_packets_count
