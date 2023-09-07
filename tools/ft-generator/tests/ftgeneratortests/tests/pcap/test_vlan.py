"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains VLAN tests for ft-generator module.
"""

import math
from pathlib import Path
from typing import Optional

from ftgeneratortests.src import (
    ENCAPSULATION_ABS_TOLERANCE,
    Generator,
    GeneratorConfig,
    get_prob_from_str,
    parse_pcap,
)


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
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 1),
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 2),
                ],
                "30%",
            ),
            GeneratorConfig.Encapsulation(
                [
                    GeneratorConfig.Encapsulation.Layer("vlan", None, 3),
                ],
                "40%",
            ),
        ],
        None,
        None,
    )
    return config


def test_vlan(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies ratio of vlan flows in pcap.

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

    pcap_file, _ = ft_generator.generate(config)

    pcap = parse_pcap(pcap_file)

    # Count vlan_ids in pcap
    pcap_ids = {}
    for flow in pcap.values():
        if flow.vlan_id:
            tmp_id = flow.vlan_id
            if tmp_id in pcap_ids:
                pcap_ids[tmp_id] += 1
            else:
                pcap_ids[tmp_id] = 1

    # Go through GeneratorConfig and count occurrences of ids in pcap
    for encapsulation in config.encapsulation:
        vlan_present = False
        encapsulation_count = 0

        for layer in encapsulation.layers:
            if layer.type == "vlan":
                vlan_present = True
                # Count occurrences of vlan_id in pcap
                if layer.id in pcap_ids:
                    encapsulation_count += pcap_ids[layer.id]

        if not vlan_present:
            continue

        calculated_ratio = encapsulation_count / len(pcap)
        defined_ratio = get_prob_from_str(encapsulation.probability)

        assert math.isclose(defined_ratio, calculated_ratio, abs_tol=ENCAPSULATION_ABS_TOLERANCE)


def test_no_vlan(ft_generator: Generator):
    """Test verifies that no vlan is present, when not enabled in configuration.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    """

    pcap_file, _ = ft_generator.generate()

    pcap = parse_pcap(pcap_file)

    for flow in pcap.values():
        assert not flow.vlan_id
