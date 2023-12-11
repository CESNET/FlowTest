"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains flow content test for ft-generator module.
"""

from pathlib import Path
from typing import Optional

from ftgeneratortests.src import Generator, GeneratorConfig, parse_pcap, parse_report


def create_config() -> GeneratorConfig:
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
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 4),
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 2),
                ],
                "50%",
            ),
            GeneratorConfig.Encapsulation(
                [
                    GeneratorConfig.Encapsulation.Layer("mpls", 3),
                    GeneratorConfig.Encapsulation.Layer("mpls", 1),
                ],
                "20%",
            ),
        ],
        GeneratorConfig.IP(0.3, 512, "10.0.0.0/8"),
        GeneratorConfig.IP(0.3, 512, "0123:4567:89ab:cdef::/64"),
        GeneratorConfig.Mac("aa:aa:aa:00:00:00/24"),
    )
    return config


def test_content(ft_generator: Generator, custom_config: Optional[Path]):
    """Test compares content of parsed flows from pcap with flows from report.

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
        config = create_config()

    pcap_file, report_file = ft_generator.generate(config)

    pcap = parse_pcap(pcap_file)
    report = parse_report(report_file)

    assert len(pcap) == len(report)

    for key, value in pcap.items():
        assert value.compare_content(report.get(key))
