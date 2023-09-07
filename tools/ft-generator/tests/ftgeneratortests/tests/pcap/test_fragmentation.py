"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains fragmentation test for ft-generator module.
"""

import math
from pathlib import Path
from typing import Optional

import pytest
from ftgeneratortests.src import (
    FRAGMENTATION_ABS_TOLERANCE,
    L3_PROTOCOL_IPV4,
    L3_PROTOCOL_IPV6,
    FlowCache,
    Generator,
    GeneratorConfig,
    get_prob_from_str,
    parse_pcap,
)


def create_config(fragmentation_probability: float) -> GeneratorConfig:
    """Function used to create custom configuration for generator.

    Returns
    -------
    GeneratorConfig
        Created generator configuration.
    """

    config = GeneratorConfig(
        None,
        GeneratorConfig.IP(float(round(fragmentation_probability, 2)), 512, None),
        GeneratorConfig.IP(float(round(fragmentation_probability, 2)), 512, None),
        None,
    )

    return config


def get_fragment_ratio(pcap: FlowCache, l3_proto: int) -> float:
    """Calculate ration of fragmented packets for selected L3 layer.

    Parameters
    ----------
    pcap : dict
        Flows parsed from pcap.
    l3_proto : int
        L3 protocol for calculation.

    Returns
    -------
    float
        _description_
    """

    packets_count = 0
    fragments_count = 0

    for flow in pcap.values():
        if flow.l3_proto != l3_proto:
            continue
        packets_count += flow.frag_min_size_packets_count
        fragments_count += flow.frag_pkt_count

    return fragments_count / packets_count


@pytest.mark.parametrize(
    "probability",
    [0.1, 0.3, 0.5, 0.7, 0.9],
)
def test_ipv4_fragmentation(ft_generator: Generator, probability: Optional[int], custom_config: Optional[Path]):
    """Test verifies if ipv4 fragments are generated with desired probability.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    probability : Optional[int]
        Probability of fragmentation.
    custom_config : Optional[Path]
        Path to custom configuration file for ft-generator.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
        if not config.ipv4 or not config.ipv4.fragmentation_probability or not config.ipv4.min_packet_size_to_fragment:
            pytest.skip()
        probability = get_prob_from_str(config.ipv4.fragmentation_probability)
    elif probability:
        config = create_config(probability)
    else:
        pytest.skip()

    pcap_file, _ = ft_generator.generate(config)
    pcap = parse_pcap(pcap_file, config.ipv4.min_packet_size_to_fragment)

    calculated_ratio = get_fragment_ratio(pcap, L3_PROTOCOL_IPV4)

    assert math.isclose(probability, calculated_ratio, abs_tol=FRAGMENTATION_ABS_TOLERANCE)


@pytest.mark.parametrize(
    "probability",
    [0.1, 0.3, 0.5, 0.7, 0.9],
)
def test_ipv6_fragmentation(ft_generator: Generator, probability: Optional[int], custom_config: Optional[Path]):
    """Test verifies if ipv6 fragments are generated with desired probability.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    probability : Optional[int]
        Probability of fragmentation.
    custom_config : Optional[Path]
        Path to custom configuration file for ft-generator.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
        if not config.ipv6 or not config.ipv6.fragmentation_probability or not config.ipv6.min_packet_size_to_fragment:
            pytest.skip()
        probability = get_prob_from_str(config.ipv6.fragmentation_probability)
    elif probability:
        config = create_config(probability)
    else:
        pytest.skip()

    pcap_file, _ = ft_generator.generate(config)
    pcap = parse_pcap(pcap_file, config.ipv6.min_packet_size_to_fragment)

    calculated_ratio = get_fragment_ratio(pcap, L3_PROTOCOL_IPV6)

    assert math.isclose(probability, calculated_ratio, abs_tol=FRAGMENTATION_ABS_TOLERANCE)


def test_no_fragmentation(ft_generator: Generator):
    """Test verifies that no ipv4 or ipv6 fragmentation is present, when fragmentation is not enabled in configuration.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    """

    pcap_file, _ = ft_generator.generate()
    pcap = parse_pcap(pcap_file)

    for flow in pcap.values():
        assert flow.frag_pkt_count == 0
