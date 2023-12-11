"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains no profiles test for ft-generator module.
"""

from pathlib import Path
from typing import Optional

from ftgeneratortests.src import (
    FlowCache,
    Generator,
    GeneratorConfig,
    parse_pcap,
    parse_report,
)


def test_no_profiles_file(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies if empty report and pcap can be generated.
    Test is using empty profiles file with header.

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

    pcap_file, report_file = ft_generator.generate(config, FlowCache())

    pcap = parse_pcap(pcap_file)
    report = parse_report(report_file)

    assert len(pcap) == len(report) == 0
