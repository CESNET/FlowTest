"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tool for editing packets in PCAP file. Similar to tcpreplay-edit or tcprewrite, but with limited editing options.
"""

from dataclasses import dataclass
from typing import Optional

from scapy.layers.all import (  # pylint: disable=no-name-in-module
    ETHER_TYPES,
    Dot1Q,
    Ether,
)
from scapy.main import load_contrib
from scapy.packet import Packet
from scapy.utils import rdpcap, wrpcap

load_contrib("mpls")


@dataclass
class RewriteRules:
    """Rules for packet rewriting.

    Attributes
    ----------
    add_vlan : int, optional
        Insert vlan (Dot1Q) header after first layer (Ether) with given tag.
    edit_dst_mac : str, optional
        Replace destination mac address with given in ethernet layer.
    """

    add_vlan: Optional[int] = None
    edit_dst_mac: Optional[str] = None


def rewrite_pcap(pcap_path: str, rules: RewriteRules, output_pcap_path: str) -> str:
    """Rewrite packets from the PCAP file according to the rules and save to the new PCAP file.

    Parameters
    ----------
    pcap_path : str
        Path to original PCAP file.
    rules : RewriteRules
        Packet rewriting rules.
    output_pcap_path : str
        Path to save new PCAP.

    Returns
    -------
    str
        Path to new PCAP file.
    """

    res_packets = []
    for packet in rdpcap(pcap_path):
        res_packets.append(edit_packet(packet, rules))

    wrpcap(output_pcap_path, res_packets)

    return output_pcap_path


def edit_packet(packet: Packet, rules: RewriteRules) -> Packet:
    """Rewrite single packet.

    Parameters
    ----------
    packet : Packet
        Packet loaded by Scapy.
    rules : RewriteRules
        Packet rewriting rules.

    Returns
    -------
    Packet
        Edited packet.

    Raises
    ------
    RuntimeError
        When packet does not have supported structure.
        Packet must start with ethernet layer.
    """

    if not isinstance(packet, Ether):
        raise RuntimeError("Scapy rewriter supports only packets with ethernet at first layer.")

    if rules.edit_dst_mac:
        packet[Ether].dst = rules.edit_dst_mac

    if rules.add_vlan is not None:
        payload = packet.payload
        packet.remove_payload()
        packet.type = ETHER_TYPES.n_802_1Q
        packet = packet / Dot1Q(vlan=rules.add_vlan) / payload

    return packet
