"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Functions with different purpose which can be utilized in testing scenarios.
"""

import ipaddress
import logging
import os
from typing import Optional, Union

import pytest
from dataclass_wizard.errors import MissingFields, ParseError
from src.config.scenario import ScenarioCfg, ScenarioCfgException
from yaml.scanner import ScannerError


def get_project_root() -> str:
    """General purpose function for getting the FlowTest root directory in ft-orchestration package.

    Returns
    -------
    str
        Project root directory.
    """

    return os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../../../../"))


def download_logs(dest: str, **kwargs) -> None:
    """Download logs from test components.

    Parameters
    ----------
    dest: str
        Path to a directory where logs should be downloaded to.
    *kwargs: Any
        Objects which logs should be downloaded.
        The argument name will be used to create the directory.
    """

    for name, device in kwargs.items():
        if device is None:
            continue

        logs_dir = os.path.join(dest, name)
        os.makedirs(logs_dir, exist_ok=True)
        device.download_logs(logs_dir)


def get_replicator_prefix(
    min_prefix: int,
    default_prefix: int,
    ipv4_range: Optional[Union[list[str], str]],
    ipv6_range: Optional[Union[list[str], str]],
) -> int:
    """Determine the value for replicator prefix so that it does not overlap with provided configuration.

    Parameters
    ----------
    min_prefix: int
        Minimum prefix value which is acceptable.
    default_prefix: int
        Default prefix value to be used if possible.
    ipv4_range: str or list[str], None
        IPv4 range settings for the generator.
    ipv6_range: str or list[str], None
        IPv6 range settings for the generator.

    Returns
    -------
    int
        Prefix which should be used in the replicator.
    """

    # range could be in form "<range> <probability>%", split by whitespace firstly
    if isinstance(ipv4_range, str):
        ipv4_prefix = int(ipv4_range.split()[0].split("/")[1])
    elif isinstance(ipv4_range, list):
        ipv4_prefix = min(int(r.split()[0].split("/")[1]) for r in ipv4_range)
    else:
        ipv4_prefix = 32

    if isinstance(ipv6_range, str):
        ipv6_prefix = int(ipv6_range.split()[0].split("/")[1])
    elif isinstance(ipv6_range, list):
        ipv6_prefix = min(int(r.split()[0].split("/")[1]) for r in ipv6_range)
    else:
        ipv6_prefix = 32

    configured_prefix = min([ipv4_prefix, ipv6_prefix])

    # check prefixes do not overlap
    assert configured_prefix >= min_prefix

    # ideally we want to use the default prefix so that we can reuse pcaps from a cache
    if configured_prefix >= default_prefix >= min_prefix:
        return default_prefix

    return min_prefix


def collect_scenarios(path: str, target: ScenarioCfg, name: Optional[str] = None) -> list["ParameterSet"]:
    """
    Collect all scenario files in the provided directory.
    The function provides created configuration object and name of the respective scenario file.
    To be used for parametrizing pytest test case to run the test case for every discovered scenario file.

    Parameters
    ----------
    path : str
        Path to a directory where test are situated.
    target: ScenarioCfg
        Configuration class which inherits from ScenarioCfg class.
    name: str, None
        Name of the test scenario. Used for selecting tests for the specific scenario.

    Returns
    -------
    list
        List of ParameterSet objects to parametrize pytest test case.
    """

    if not os.path.isdir(path):
        logging.getLogger().error("Path %s is not a directory", path)
        return []

    try:
        files = [f for f in os.listdir(path) if ".yml" in f]
    except OSError as err:
        logging.getLogger().error("Unable to read directory content: %s, error: %s", path, err)
        return []

    tests = []
    for file in files:
        abspath = os.path.join(path, file)
        logging.getLogger().info("Loading test scenario from file: %s", abspath)
        try:
            scenario = target.from_yaml_file(abspath)
            scenario.check()
            for test_cfg, test_marks, test_id in scenario.get_tests(file, name):
                marks = [getattr(pytest.mark, mark) for mark in test_marks]
                tests.append(pytest.param(test_cfg, test_id, marks=marks, id=test_id))
        except (
            OSError,
            TypeError,
            AttributeError,
            ParseError,
            MissingFields,
            ScenarioCfgException,
            ScannerError,
        ) as err:
            logging.getLogger().error("Loading test scenario from file: %s, error: %s", abspath, err)
            # We cannot mark test as failed at this point, therefore use skip.
            marks = pytest.mark.skip(reason=f"ERROR: {err}, file: {abspath}")
            tests.append(pytest.param(None, file, marks=marks, id=file.removesuffix(".yml")))

    return tests


def ip_network_add_offset(address: Union[ipaddress.IPv4Network, ipaddress.IPv6Network, str], offset: int):
    """Add offset to IP network.
    Respect ft-replay rules of replication. For IPv6 network constant is added to first 32 bits.

    Parameters
    ----------
    address: Union[ipaddress.IPv4Network, ipaddress.IPv6Network, str]
        Origin IP network. Can be string which is converted.
    offset: int
        Offset to add.

    Returns
    -------
    ipaddress.IPv4Network or ipaddress.IPv6Network
        Offset IP network.
    """

    if isinstance(address, str):
        address = ipaddress.ip_network(address)

    try:
        if address.version == 6:
            network_address = int(address.network_address) + 2**96 * offset
            # overflow address
            while network_address >= 2**128:
                network_address -= 2**128
        else:
            network_address = address.network_address + offset
        return ipaddress.ip_network((network_address, address.prefixlen))
    except ValueError as ex:
        logging.error("ip_network_add_offset error: %s", ex)
        return address
