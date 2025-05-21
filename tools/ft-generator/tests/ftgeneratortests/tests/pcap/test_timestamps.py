"""
Author(s): Michal Sedlak <sedlakm@cesnet.cz>

Copyright: (C) 2024 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains flow timestamp test for ft-generator module.
"""

from ftgeneratortests.src import Generator, GeneratorConfig
from scapy.packet import Packet
from scapy.utils import PcapReader


def calc_min_gap_nanos(packet_size, link_speed_gbps, min_packet_gap_ns):
    """
    Calculate minimal gap between two packets depending on the provided configuration options.

    Attributes
    ----------
    packet_size
        The packet size (including L2+ headers).
    link_speed_gbps
        The link speed in gbps.
    min_packet_gap_ns
        Additional gap between packets in nanoseconds.
    """

    frame_with_crc = max(packet_size, 60) + 4
    frame_aligned = (frame_with_crc + 7) / 8 * 8  # Round up to multiple of 8
    frame_total = frame_aligned + 8 + 12

    transfer_nanos = frame_total * 1_000_000_000 / (link_speed_gbps * 1_000_000_000)

    return min(transfer_nanos, min_packet_gap_ns)


def test_timestamp(ft_generator: Generator):
    """Test compares timing of parsed flows from pcap with timing of flows from report.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    """

    min_packet_gap_ns = 10
    link_speed_gbps = 100

    config = GeneratorConfig(
        timestamps=GeneratorConfig.Timestamps(
            min_packet_gap=f"{min_packet_gap_ns} ns",
            link_speed=f"{link_speed_gbps} gbps",
        )
    )

    pcap_file, _ = ft_generator.generate(config)

    prev_pkt = None
    for pkt in PcapReader(pcap_file.as_posix()):
        pkt: Packet = pkt

        if not prev_pkt:
            prev_pkt = pkt
            continue

        min_gap_nanos = calc_min_gap_nanos(len(prev_pkt), link_speed_gbps, min_packet_gap_ns)
        gap_nanos = (pkt.time - prev_pkt.time) * 1_000_000_000
        assert gap_nanos >= min_gap_nanos

        prev_pkt = pkt
