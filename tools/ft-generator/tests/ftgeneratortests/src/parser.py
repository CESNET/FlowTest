"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains parse functions.
"""

import ipaddress as ip
from pathlib import Path
from typing import Optional, Union

import netaddr as naddr
import pandas as pd
from ftgeneratortests.src import (
    L3_PROTOCOL_IPV4,
    L3_PROTOCOL_IPV6,
    L4_PROTOCOL_ICMP,
    L4_PROTOCOL_ICMPV6,
    L4_PROTOCOL_TCP,
    L4_PROTOCOL_UDP,
    MTU_SIZE,
    ExtendedFlow,
    Flow,
    FlowCache,
)
from scapy.contrib.mpls import MPLS
from scapy.layers.inet import ICMP, IP, TCP, UDP
from scapy.layers.inet6 import (
    ICMPv6DestUnreach,
    ICMPv6EchoReply,
    ICMPv6EchoRequest,
    IPv6,
    IPv6ExtHdrFragment,
)
from scapy.layers.l2 import Dot1AD, Dot1Q, Ether
from scapy.packet import Packet
from scapy.utils import PcapReader


def parse_l2(pkt: Packet, tmp: ExtendedFlow):
    """Parses packet L2 header.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    tmp : ExtendedFlow
        Flow record, which should be updated.
    """

    if Ether in pkt:
        tmp.src_mac = naddr.EUI(pkt[Ether].src)
        tmp.dst_mac = naddr.EUI(pkt[Ether].dst)


def parse_vlan(pkt: Packet, tmp: ExtendedFlow):
    """Parses packet vlan header.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    tmp : ExtendedFlow
        Flow record, which should be updated.
    """

    if Dot1Q in pkt:
        tmp.vlan_id = pkt[Dot1Q].vlan
    if Dot1AD in pkt:
        tmp.vlan_id = pkt[Dot1AD].vlan


def parse_mpls(pkt: Packet, tmp: ExtendedFlow):
    """Parses packet mpls header.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    tmp : ExtendedFlow
        Flow record, which should be updated.
    """

    if MPLS in pkt:
        tmp.mpls_label = pkt[MPLS].label


def parse_l3(pkt: Packet, tmp: ExtendedFlow):
    """Parses packet L3 header.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    tmp : ExtendedFlow
        Flow record, which should be updated.

    Raises
    ------
    RuntimeError
        Raised when unsupported L3 protocol is found.
    """

    if IP in pkt:
        tmp.l3_proto = L3_PROTOCOL_IPV4
        tmp.bytes = len(pkt[IP])
        tmp.src_ip = ip.ip_address(pkt[IP].src)
        tmp.dst_ip = ip.ip_address(pkt[IP].dst)
    elif IPv6 in pkt:
        tmp.l3_proto = L3_PROTOCOL_IPV6
        tmp.bytes = len(pkt[IPv6])
        tmp.src_ip = ip.ip_address(pkt[IPv6].src)
        tmp.dst_ip = ip.ip_address(pkt[IPv6].dst)
    else:
        raise RuntimeError("parse_l3(): Unsupported L3 protocol found.")

    if tmp.bytes > MTU_SIZE:
        raise RuntimeError("parse_l3(): Size of packet is bigger than MTU.")


def parse_l4(pkt: Packet, tmp: ExtendedFlow):
    """Parses packet L4 layer.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    tmp : ExtendedFlow
        Flow record, which should be updated.
    """

    if TCP in pkt:
        tmp.l4_proto = L4_PROTOCOL_TCP
        tmp.src_port = pkt[TCP].sport
        tmp.dst_port = pkt[TCP].dport
    elif UDP in pkt:
        tmp.l4_proto = L4_PROTOCOL_UDP
        tmp.src_port = pkt[UDP].sport
        tmp.dst_port = pkt[UDP].dport
    elif ICMP in pkt:
        tmp.l4_proto = L4_PROTOCOL_ICMP
    elif ICMPv6EchoRequest in pkt or ICMPv6DestUnreach in pkt or ICMPv6EchoReply in pkt:
        tmp.l4_proto = L4_PROTOCOL_ICMPV6
    else:
        tmp.l4_proto = 0


def parse_fragmentation(
    pkt: Packet,
    tmp: ExtendedFlow,
    fragments: FlowCache,
    frag_min_size_packets: int,
) -> Union[ExtendedFlow, None]:
    """Parses fragmentation, if present.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    tmp : ExtendedFlow
        Flow record, which should be updated.
    fragments : FlowCache
        FlowCache with unparsed fragments.
    frag_min_size_packets: int
        Minimal size of packet to be considered for fragmentation.

    Returns
    -------
    Union[ExtendedFlow, None]
        Original tmp parameter, if fragmentation not found.
        ExtendedFlow if fragmented packet was assembled.
        None if incomplete packet (fragment) found.

    Raises
    ------
    RuntimeError
        Raised when invalid fragment found.
        Occurs when last packet fragment was found, but no fragment with same id was found before.
    """

    frag_id = -1
    is_last_frag = False

    # Detect fragmentation
    if IP in pkt:
        if pkt[IP].flags.MF:
            frag_id = pkt[IP].id
        elif pkt[IP].frag != 0:
            frag_id = pkt[IP].id
            is_last_frag = True
    elif IPv6 in pkt:
        if IPv6ExtHdrFragment in pkt:
            if pkt[IPv6ExtHdrFragment].m:
                frag_id = pkt[IPv6ExtHdrFragment].id
            elif pkt[IPv6ExtHdrFragment].offset != 0:
                frag_id = pkt[IPv6ExtHdrFragment].id
                is_last_frag = True

    # Hash frag_id with src and dst ip
    if frag_id != -1:
        frag_id = hash((tmp.src_ip, tmp.dst_ip, frag_id))

    # Last fragment found
    if is_last_frag:
        if frag_id not in fragments:
            raise RuntimeError("parse_fragmentation(): Invalid fragment found.")
        tmp.frag_pkt_count = 1
        tmp.frag_min_size_packets_count = 1
        fragments.update(tmp, frag_id)
        return fragments.pop(frag_id)

    # Fragment found
    if frag_id != -1:
        fragments.update(tmp, frag_id)
        return None

    # No fragmentation
    if tmp.bytes > frag_min_size_packets:
        tmp.frag_min_size_packets_count = 1

    return tmp


def parse_packet(pkt: Packet, fragments: FlowCache, frag_min_size_packets: int) -> Union[ExtendedFlow, None]:
    """Function parses packet and creates ExtendedFlow.
    Function is parsing L2, L3, L4, MPLS, VLAN layers and linking packet fragments.

    Parameters
    ----------
    pkt : scapy.Packet
        Packet to parse.
    fragments : FlowCache
        FlowCache with unparsed fragments.
    frag_min_size_packets: int
        Minimal size of packet to be considered for fragmentation.

    Returns
    -------
    Union[ExtendedFlow, None]
        ExtendedFlow created from single packet.
        None if incomplete packet found.

    Raises
    ------
    RuntimeError
        Raised when unsupported L4 protocol is found.
    """

    tmp = ExtendedFlow()
    tmp.packets = 1
    tmp.start_time = tmp.end_time = int(pkt.time * 1000)

    parse_l2(pkt, tmp)
    parse_vlan(pkt, tmp)
    parse_mpls(pkt, tmp)
    parse_l3(pkt, tmp)
    parse_l4(pkt, tmp)

    # Fragments
    tmp = parse_fragmentation(pkt, tmp, fragments, frag_min_size_packets)

    # Check for unsupported L4 protocols (must be checked after parse_fragmentation)
    if tmp is not None and tmp.l4_proto == 0:
        raise RuntimeError("parse_packet(): Unsupported L4 protocol found.")

    return tmp


def parse_pcap(pcap_file: Path, frag_min_size_packets: Optional[int] = 512) -> FlowCache:
    """Function to parse pcap file into dictionary with ExtendedFlow objects.

    Parameters
    ----------
    pcap_file : Path
        Path to pcap file.
    frag_min_size_packets: Optional[int]
        Minimal size of packet to be considered for fragmentation, by default 512.

    Returns
    -------
    FlowCache
        FlowCache with parsed ExtendedFlows.

    Raises
    ------
    FileNotFoundError
        Raised when pcap file cannot be found.
    RuntimeError
        Raised when unmatched packet fragments are left after reading pcap.
    """

    flows = FlowCache()
    fragments = FlowCache()

    # Check file existence
    if not pcap_file.exists():
        raise FileNotFoundError(pcap_file.as_posix())

    # Parse packets individually
    for pkt in PcapReader(pcap_file.as_posix()):
        tmp = parse_packet(pkt, fragments, frag_min_size_packets)
        if tmp is None:
            continue
        flows.update(tmp)

    # Check for unmatched fragments.
    if len(fragments) != 0:
        raise RuntimeError("parse_pcap(): Unmatched packet fragments are left.")

    return flows


def parse_report(file: Path) -> FlowCache:
    """Function parses CSV report file with flow records into FlowCache.

    Parameters
    ----------
    file : Path
        Path to CSV report file with flow records.

    Returns
    -------
    FlowCache
        FlowCache with parsed flows.

    Raises
    ------
    FileNotFoundError
        Raised when CSV file cannot be found.
    ValueError
        Raised when CSV does not contain required columns.
    """

    if not file.exists():
        raise FileNotFoundError("parser::parse_report(): File not found:", file.as_posix())

    cache = FlowCache()
    report = pd.read_csv(file.as_posix())

    required_columns = {
        "SRC_IP",
        "DST_IP",
        "BYTES",
        "BYTES_REV",
        "PACKETS",
        "PACKETS_REV",
        "L3_PROTO",
        "L4_PROTO",
        "SRC_PORT",
        "DST_PORT",
        "START_TIME",
        "START_TIME_REV",
        "END_TIME",
        "END_TIME_REV",
    }

    if not required_columns.issubset(report.columns):
        raise ValueError("parser::parse_report(): Required column is missing.")

    for _, row in report.iterrows():
        tmp = Flow()
        tmp.src_ip = ip.ip_address(row["SRC_IP"])
        tmp.dst_ip = ip.ip_address(row["DST_IP"])
        tmp.bytes = row["BYTES"]
        tmp.bytes_rev = row["BYTES_REV"]
        tmp.packets = row["PACKETS"]
        tmp.packets_rev = row["PACKETS_REV"]
        tmp.l3_proto = row["L3_PROTO"]
        tmp.l4_proto = row["L4_PROTO"]
        tmp.src_port = row["SRC_PORT"]
        tmp.dst_port = row["DST_PORT"]

        if row["PACKETS"] != 0 and row["PACKETS_REV"] != 0:
            tmp.start_time = min(row["START_TIME"], row["START_TIME_REV"])
            tmp.end_time = max(row["END_TIME"], row["END_TIME_REV"])
        elif row["PACKETS_REV"] != 0:
            tmp.start_time = row["START_TIME_REV"]
            tmp.end_time = row["END_TIME_REV"]
        else:
            tmp.start_time = row["START_TIME"]
            tmp.end_time = row["END_TIME"]

        cache.add(tmp)

    return cache


def parse_profiles(file: Path) -> FlowCache:
    """Function parses CSV file with profiles into FlowCache.

    As no IP addresses are provided in profiles file and no hash can be created,
    flows are stored in FlowCache with hash equal to their position in file.

    Parameters
    ----------
    file : Path
        Path to CSV profiles file.

    Returns
    -------
    FlowCache
        FlowCache with parsed profiles.

    Raises
    ------
    FileNotFoundError
        Raised when csv file cannot be found.
    ValueError
        Raised when CSV does not contain required columns.
    """

    if not file.exists():
        raise FileNotFoundError("parser::parse_profiles(): File not found:", file.as_posix())

    cache = FlowCache()
    profiles = pd.read_csv(file.as_posix())

    required_columns = {
        "BYTES",
        "BYTES_REV",
        "PACKETS",
        "PACKETS_REV",
        "L3_PROTO",
        "L4_PROTO",
        "SRC_PORT",
        "DST_PORT",
        "START_TIME",
        "END_TIME",
    }

    if not required_columns.issubset(profiles.columns):
        raise ValueError("parser::parse_profiles(): Required column is missing.")

    for index, row in profiles.iterrows():
        tmp = Flow()
        tmp.bytes = row["BYTES"]
        tmp.bytes_rev = row["BYTES_REV"]
        tmp.packets = row["PACKETS"]
        tmp.packets_rev = row["PACKETS_REV"]
        tmp.l3_proto = row["L3_PROTO"]
        tmp.l4_proto = row["L4_PROTO"]
        tmp.src_port = row["SRC_PORT"]
        tmp.dst_port = row["DST_PORT"]
        tmp.start_time = row["START_TIME"]
        tmp.end_time = row["END_TIME"]

        cache.add(tmp, index)

    return cache
