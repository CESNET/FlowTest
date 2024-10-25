"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains mac_range tests for ft-generator module.
"""

from pathlib import Path
from typing import Optional

import pytest
from ftgeneratortests.src import MAC_MASK_SIZE, Generator, GeneratorConfig, parse_pcap
from netaddr import EUI


def create_config(mac_ranges: list[str]) -> GeneratorConfig:
    """Function used to create custom configuration for generator.

    Parameters
    ----------
    mac_ranges : list[str]
        List of mac ranges.

    Returns
    -------
    GeneratorConfig
        Created generator configuration.
    """

    if len(mac_ranges) == 1:
        mac_ranges = str(mac_ranges[0])

    config = GeneratorConfig(
        None,
        None,
        None,
        GeneratorConfig.Mac(mac_ranges),
    )

    return config


def get_mac_range_from_config(config: GeneratorConfig) -> list[str]:
    """Function retrieves mac ranges from GeneratorConfig.

    Parameters
    ----------
    config : GeneratorConfig
        Ft-generator configuration.

    Returns
    -------
    list[str]
        List of mac ranges, from configuration file.

    Raises
    ------
    ValueError
        Raised when unexpected mac_range data type found.
    """

    if config.mac and config.mac.mac_range:
        if isinstance(config.mac.mac_range, str):
            return [config.mac.mac_range]
        if isinstance(config.mac.mac_range, list):
            return config.mac.mac_range
        raise ValueError("test_mac_range::get_mac_range_from_config: Unexpected mac_range data type.")

    return []


def in_range(mac: EUI, mac_ranges: list[str]) -> bool:
    """Function checks if mac belongs to any mac range in list.

    Parameters
    ----------
    mac : str
        Mac to check.
    mac_ranges : list[str]
        List of mac ranges.

    Returns
    -------
    bool
        True if mac belongs to any range in list, False otherwise.
    """

    mac = mac.bin

    for tmp_range in mac_ranges:
        tmp, mask = tmp_range.split("/")
        tmp = EUI(tmp).bin
        mask = MAC_MASK_SIZE - int(mask)
        if mac[:-mask] == tmp[:-mask]:
            return True

    return False


@pytest.mark.parametrize(
    "mac_range",
    [
        ["c4:6f:f7:00:00:00/24"],
        ["aa:b0:2c:00:00:00/24"],
        ["ae:18:e5:00:00:00/24"],
        ["26:d7:d1:8a:00:00/32"],
        ["23:51:11:74:00:00/32"],
        ["11:5b:e4:79:00:00/32"],
        ["3d:f3:da:5d:27:00/40"],
        ["ed:09:46:08:86:00/40"],
        ["43:2d:6c:b9:e1:00/40"],
        ["43:2d:6c:b9:e1:10/44", "43:2d:6c:b9:e1:20/44", "43:2d:6c:b9:e1:30/44"],
        ["aa:aa:aa:aa:bb:cc/46"],
    ],
)
def test_mac_range(
    ft_generator: Generator,
    mac_range: Optional[list[str]],
    custom_config: Optional[Path],
):
    """Test verifies range of mac addresses in pcap.
    If custom_config provided, mac_range is ignored.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    mac_range: Optional[list[str]]
        List of mac ranges to test.
    custom_config: Optional[Path]
        Path to custom configuration file for ft-generator.
        None if not provided.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
        mac_range = get_mac_range_from_config(config)
        if len(mac_range) == 0:
            pytest.skip()
    elif mac_range:
        config = create_config(mac_range)
    else:
        pytest.skip()

    pcap_file, _ = ft_generator.generate(config)

    pcap = parse_pcap(pcap_file)

    for flow in pcap.values():
        assert in_range(flow.src_mac, mac_range)
        assert in_range(flow.dst_mac, mac_range)
