"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains more reverse packets test for ft-generator module.
"""

from pathlib import Path
from typing import Optional

from ftgeneratortests.src import (
    Flow,
    FlowCache,
    Generator,
    GeneratorConfig,
    parse_pcap,
    parse_report,
)


def create_profiles() -> FlowCache:
    """Function creates custom profiles.

    Returns
    -------
    FlowCache
        FlowCache with created profiles.
    """

    flow_cache = FlowCache()
    flow = Flow(
        src_port=58428,
        dst_port=1443,
        l3_proto=4,
        l4_proto=1,
        start_time=0,
        end_time=28354,
        packets=1,
        packets_rev=1000,
        bytes=500,
        bytes_rev=20000,
    )
    flow_cache.add(flow, 1)

    return flow_cache


def test_more_rev_packet(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies if more reverse packets then forward packets can be generated.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    custom_config: Optional[Path]
        Path to custom configuration file for ft-generator.
        None if not provided.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
    else:
        config = GeneratorConfig()

    pcap_file, report_file = ft_generator.generate(config, create_profiles())

    pcap = parse_pcap(pcap_file)
    report = parse_report(report_file)

    assert len(pcap) == len(report)

    for key, value in pcap.items():
        assert value.compare_content(report.get(key))
