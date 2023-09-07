"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains ip_range tests for ft-generator module.
"""

import ipaddress
from pathlib import Path
from typing import Optional

import pytest
from ftgeneratortests.src import (
    L3_PROTOCOL_IPV4,
    L3_PROTOCOL_IPV6,
    Generator,
    GeneratorConfig,
    parse_pcap,
)


def create_config(ip_ranges: list[str]) -> GeneratorConfig:
    """Function used to create custom configuration for generator.
    Function sorts ip ranges based on their version and creates GeneratorConfig instance.

    Parameters
    ----------
    ip_ranges : list[str]
        List of ip ranges.

    Returns
    -------
    GeneratorConfig
        Created generator configuration.

    Raises
    ------
    ValueError
        Raised when unexpected IP format found.
    """

    ipv4 = []
    ipv6 = []

    for network in ip_ranges:
        if isinstance(ipaddress.ip_network(network), ipaddress.IPv4Network):
            ipv4.append(network)
        elif isinstance(ipaddress.ip_network(network), ipaddress.IPv6Network):
            ipv6.append(network)
        else:
            raise ValueError("test_ip_range::create_config: Unexpected IP format found.")

    # If none or one entry in list, modify data type
    if len(ipv4) == 0:
        ipv4 = None
    elif len(ipv4) == 1:
        ipv4 = str(ipv4[0])

    if len(ipv6) == 0:
        ipv6 = None
    elif len(ipv6) == 1:
        ipv6 = str(ipv6[0])

    config = GeneratorConfig(
        None,
        GeneratorConfig.IP(None, None, ipv4),
        GeneratorConfig.IP(None, None, ipv6),
        None,
    )

    return config


def get_ip_range_from_config(config: GeneratorConfig) -> list[str]:
    """Function retrieves ip ranges from GeneratorConfig.

    Parameters
    ----------
    config : GeneratorConfig
        Ft-generator configuration.

    Returns
    -------
    list[str]
        List of ip ranges, from configuration file.

    Raises
    ------
    ValueError
        Raised when unexpected ip_range data type found.
    """

    ip_range = []

    if config.ipv4 and config.ipv4.ip_range:
        if isinstance(config.ipv4.ip_range, str):
            ip_range.append(config.ipv4.ip_range)
        elif isinstance(config.ipv4.ip_range, list):
            ip_range = ip_range + config.ipv4.ip_range
        else:
            raise ValueError("test_ip_range::get_ip_range_from_config: Unexpected ip_range data type.")

    if config.ipv6 and config.ipv6.ip_range:
        if isinstance(config.ipv6.ip_range, str):
            ip_range.append(config.ipv6.ip_range)
        elif isinstance(config.ipv6.ip_range, list):
            ip_range = ip_range + config.ipv6.ip_range
        else:
            raise ValueError("test_ip_range::get_ip_range_from_config: Unexpected ip_range data type.")

    return ip_range


def get_ip_range_versions(config: GeneratorConfig) -> list[bool, bool]:
    """Function verifies if ipv4 and ipv6 ip format is present in config.

    Parameters
    ----------
    config : GeneratorConfig
        Ft-generator configuration.

    Returns
    -------
    list[bool, bool]
        Presence of [ipv4, ipv6] protocol in configuration.
    """

    versions = [False, False]

    if config.ipv4 and config.ipv4.ip_range:
        versions[0] = True
    if config.ipv6 and config.ipv6.ip_range:
        versions[1] = True

    return versions


def in_range(ip_addr: str, ip_ranges: list[str]) -> bool:
    """Function checks if ip belongs to any ip range in list.

    Parameters
    ----------
    ip_addr : str
        Ip to check.
    ip_ranges : list[str]
        List of ip ranges.

    Returns
    -------
    bool
        True if ip belongs to any range in list, False otherwise.
    """

    for network in ip_ranges:
        if ipaddress.ip_address(ip_addr) in ipaddress.ip_network(network):
            return True

    return False


@pytest.mark.parametrize(
    "ip_range",
    [
        ["192.168.0.0/24"],
        ["192.168.1.0/24"],
        ["192.168.255.0/24"],
        ["fd4f:6591:77aa::/48"],
        ["fdde:3c9e:1b55::/64"],
        ["fdf9:3ac6:8341:ffff::/64"],
        ["192.168.0.0/24", "fd4f:4b50:1892::/48"],
        ["192.168.0.0/24", "192.168.255.0/24"],
        ["fd4f:4b50:1892::/48", "0120:4567:81ab:cdef::/64"],
        ["95.103.224.0/29"],
        ["2001:db8:85a3::8a2e:370:7334/127"],
    ],
)
def test_ip_range(
    ft_generator: Generator,
    ip_range: Optional[list[str]],
    custom_config: Optional[Path],
):
    """Test verifies range of IPv4/IPv6 IPs in pcap.
    If custom_config provided, ip_range is ignored.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    ip_range: Optional[list[str]]
        List of ip ranges to test.
    custom_config: Optional[Path]
        Path to custom configuration file for ft-generator.
        None if not provided.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
        ip_range = get_ip_range_from_config(config)
        if len(ip_range) == 0:
            pytest.skip()
    elif ip_range:
        config = create_config(ip_range)
    else:
        pytest.skip()

    ipv4, ipv6 = get_ip_range_versions(config)

    pcap_file, _ = ft_generator.generate(config)

    pcap = parse_pcap(pcap_file)

    for flow in pcap.values():
        if (flow.l3_proto == L3_PROTOCOL_IPV4 and ipv4) or (flow.l3_proto == L3_PROTOCOL_IPV6 and ipv6):
            assert in_range(flow.src_ip, ip_range)
            assert in_range(flow.dst_ip, ip_range)
