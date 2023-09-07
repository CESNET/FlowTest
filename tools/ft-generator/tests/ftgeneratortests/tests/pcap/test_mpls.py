"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains MPLS tests for ft-generator module.
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
                    GeneratorConfig.Encapsulation.Layer("mpls", 1, None),
                    GeneratorConfig.Encapsulation.Layer("mpls", 2, None),
                ],
                "30%",
            ),
            GeneratorConfig.Encapsulation(
                [
                    GeneratorConfig.Encapsulation.Layer("mpls", 3, None),
                ],
                "40%",
            ),
        ],
        None,
        None,
    )
    return config


def test_mpls(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies ratio of mpls flows in pcap.

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
    pcap_labels = {}
    for flow in pcap.values():
        if flow.mpls_label:
            tmp_label = flow.mpls_label
            if tmp_label in pcap_labels:
                pcap_labels[tmp_label] += 1
            else:
                pcap_labels[tmp_label] = 1

    # Convert config to GeneratorConfig, if it is Path
    if not isinstance(config, GeneratorConfig):
        config = GeneratorConfig(config)

    # Go through GeneratorConfig and count occurrences of ids in pcap
    for encapsulation in config.encapsulation:
        mpls_present = False
        encapsulation_count = 0

        for layer in encapsulation.layers:
            if layer.type == "mpls":
                mpls_present = True
                # Count occurrences of mpls_label in pcap
                if layer.label in pcap_labels:
                    encapsulation_count += pcap_labels[layer.label]

        if not mpls_present:
            continue

        calculated_ratio = encapsulation_count / len(pcap)
        defined_ratio = get_prob_from_str(encapsulation.probability)

        assert math.isclose(defined_ratio, calculated_ratio, abs_tol=ENCAPSULATION_ABS_TOLERANCE)


def test_no_mpls(ft_generator: Generator):
    """Test verifies that no mpls is present, when not enabled in configuration.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    """

    pcap_file, _ = ft_generator.generate()

    pcap = parse_pcap(pcap_file)

    for flow in pcap.values():
        assert not flow.mpls_label
