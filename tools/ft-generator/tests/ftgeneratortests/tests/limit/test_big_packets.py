"""
Author(s):  Matej Hulák <hulak@cesnet.cz>
            Michal Sedlak <sedlakm@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains big packets test for ft-generator module.
"""

import pytest
from ftgeneratortests.src import (
    MTU_SIZE,
    Flow,
    FlowCache,
    Generator,
    GeneratorConfig,
    parse_pcap,
    parse_report,
)
from scapy.utils import PcapReader


def create_vlan_config() -> GeneratorConfig:
    """Function used to create custom configuration for generator.

    Returns
    -------
    GeneratorConfig
        Created generator configuration.
    """

    config = GeneratorConfig(
        [
            GeneratorConfig.Encapsulation(
                [
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 1),
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 2),
                ],
                "100%",
            ),
        ],
        None,
        None,
    )
    return config


def create_mpls_config() -> GeneratorConfig:
    """Function used to create custom configuration for generator.

    Returns
    -------
    GeneratorConfig
        Created generator configuration.
    """

    config = GeneratorConfig(
        [
            GeneratorConfig.Encapsulation(
                [
                    GeneratorConfig.Encapsulation.Layer("mpls", 3, None),
                ],
                "100%",
            ),
        ],
        None,
        None,
    )
    return config


def create_profiles() -> FlowCache:
    """Function creates custom profiles.

    Returns
    -------
    FlowCache
        FlowCache with created profiles.
    """

    flow = Flow(
        src_port=58428,
        dst_port=1443,
        l3_proto=4,
        l4_proto=1,
        start_time=100,
        end_time=100,
        packets=1,
        packets_rev=0,
        bytes=3000,
        bytes_rev=0,
    )
    flow_cache = FlowCache()
    flow_cache.add(flow, key=1)

    return flow_cache


@pytest.mark.parametrize("config", [GeneratorConfig(), create_vlan_config(), create_mpls_config()])
def test_big_packets(ft_generator: Generator, config: GeneratorConfig):
    """Test verifies if too big packets can be generated.
    Ft-generator should fail or modify packet size.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    """

    config = GeneratorConfig()

    pcap_file, report_file = ft_generator.generate(config, create_profiles())

    for packet in PcapReader(pcap_file.as_posix()):
        assert len(packet) <= MTU_SIZE

    pcap = parse_pcap(pcap_file)
    report = parse_report(report_file)

    assert len(pcap) == len(report)

    for key, value in pcap.items():
        assert value.compare_content(report.get(key))
        assert report.get(key).packets == 1 and report.get(key).bytes <= MTU_SIZE
