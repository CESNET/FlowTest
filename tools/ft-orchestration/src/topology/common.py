"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Common options and fixtures used by FlowTest topologies.
"""

from collections import namedtuple
from typing import Optional

import pytest
from src.common.builder_base import BuilderBase
from src.config.config import Config

DEFAULT_EXPORT_PROTOCOL = "tcp"
DEFAULT_EXPORT_PORT = 4739
MAX_HOST_TIMESTAMPS_DIFF = 500

Option = namedtuple("Option", ["alias", "arguments"])


def pytest_addoption(parser: pytest.Parser):
    """Add common options for all FlowTest topologies."""

    parser.addoption(
        "--config-path",
        type=str,
        help=("Redefine path to static config yaml files (probes.yaml, authentications.yaml, ...)."),
    )

    parser.addoption(
        "--probe",
        type=str,
        help=(
            "Probe from configuration file probes.yml. "
            "Use 'alias' to identify probe. "
            "Optionally, you can select interfaces to use by appending :ifc=if1,if2... to alias. "
            "If not specified, all available interfaces will be used. "
            "Any of connector-specific parameters can be appended after alias.\n"
            "E.g.: "
            "flowmonexp-10G:ifc=eth2 or "
            "ipfixprobe-10G:ifc=eth2,eth3;cache_size=64"
        ),
    )

    parser.addoption(
        "--collector",
        type=str,
        help=(
            "Collector from configuration file collectors.yml. Use 'alias' to identify collector. "
            "Optionally, you can override default export port (4739) and export protocol (tcp) by appending "
            ":port={any_port};protocol={tcp/udp} to alias. "
            "Any of connector-specific parameters can be appended.\n"
            "E.g.: validation-col:port=9999;protocol=udp;specific-param=1"
        ),
    )


@pytest.fixture(scope="session")
def config(request: pytest.FixtureRequest) -> Config:
    """Fixture providing static configuration object.

    Parameters
    ----------
    request : FixtureRequest
        Pytest request.

    Returns
    -------
    Config
        Static configuration.
    """

    assert request.config.getoption(
        "config_path"
    ), "Path to static configuration files is required option (--config-path)."

    config_dir = request.config.getoption("config_path")

    return Config(
        f"{config_dir}/authentications.yml",
        f"{config_dir}/generators.yml",
        f"{config_dir}/collectors.yml",
        f"{config_dir}/probes.yml",
        f"{config_dir}/whitelists.yml",
    )


def parse_opt(option: str) -> Option:
    """Parse probe, generator or collector cmd argument.
    Object argument is in form <alias>:<key>=<value>;<key>=<value>;...

    Parameters
    ----------
    option : str
        Argument string.

    Returns
    -------
    Option
        Object alias and additional arguments.
    """

    splitted = option.split(":", 1)
    alias = splitted[0]
    arguments = {}
    if len(splitted) == 2:
        for arg in [_arg.split("=", 1) for _arg in splitted[1].split(";")]:
            if len(arg) == 2:
                # store value
                arguments.update({arg[0]: arg[1]})
            else:
                # store true
                arguments.update({arg[0]: True})

    return Option(alias, arguments)


@pytest.fixture(scope="session")
def probe_option(request: pytest.FixtureRequest) -> Optional[Option]:
    """Option to select probe to use.

    Parameters
    ----------
    request : FixtureRequest
        Pytest request.

    Returns
    -------
    Optional[Option]
        Named tuple holding alias and additional arguments.
    """

    opt = request.config.getoption("probe")
    if opt:
        parsed = parse_opt(opt)
        # If ifc not specified, all available interfaces will be used.
        if "ifc" not in parsed.arguments:
            parsed.arguments["ifc"] = []
        else:
            # custom parsing ifc option
            parsed.arguments["ifc"] = parsed.arguments["ifc"].split(",")
        return parsed
    return None


@pytest.fixture(scope="session")
def collector_option(request: pytest.FixtureRequest) -> Optional[Option]:
    """Option to select collector to use.

    Parameters
    ----------
    request : FixtureRequest
        Pytest request.

    Returns
    -------
    Optional[Option]
        Named tuple holding alias and additional arguments.
    """

    opt = request.config.getoption("collector")
    if opt:
        parsed = parse_opt(opt)
        if "port" in parsed.arguments:
            # receiving port should be an integer
            parsed.arguments["port"] = int(parsed.arguments["port"])
        else:
            parsed.arguments["port"] = DEFAULT_EXPORT_PORT
        if "protocol" not in parsed.arguments:
            parsed.arguments["protocol"] = DEFAULT_EXPORT_PROTOCOL
        return parsed
    return None


def parse_generator_option(option: str) -> Optional[Option]:
    """Parse option to select generator to use.

    Parameters
    ----------
    option : str
        Generator option before parsing.

    Returns
    -------
    Optional[Option]
        Named tuple holding alias and additional arguments.
    """

    if option:
        parsed = parse_opt(option)
        if "vlan" in parsed.arguments:
            # vlan id should be an integer
            parsed.arguments["vlan"] = int(parsed.arguments["vlan"])
        else:
            # by default vlan is not added
            parsed.arguments["vlan"] = None
        if "orig-mac" not in parsed.arguments:
            # by default is destination mac rewritten in packets
            parsed.arguments["orig-mac"] = False
        return parsed
    return None


def check_time_synchronization(*args: list[BuilderBase]) -> None:
    """Check the time differences between the machines running collector, probe and generator.

    Parameters
    ----------
    args : list[BuilderBase]
        Builders to check for time differences.
    """

    assert len(args) == 3

    for builder in args:
        builder.host_timestamp_async()
    timestamps = [builder.host_timestamp_result() for builder in args]

    assert (
        max(timestamps) - min(timestamps) <= MAX_HOST_TIMESTAMPS_DIFF
    ), f"Timestamps from remote hosts differ by more than {MAX_HOST_TIMESTAMPS_DIFF}ms."
