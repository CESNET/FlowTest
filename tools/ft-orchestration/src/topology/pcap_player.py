"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Implementation of topology with pcap-player generator, probe and collector.
"""

from copy import deepcopy
from typing import Optional

import pytest
import pytest_cases
from lbr_testsuite.topology import Topology, registration
from src.collector.collector_builder import CollectorBuilder
from src.config.config import Config
from src.generator.generator_builder import GeneratorBuilder
from src.probe.probe_builder import ProbeBuilder
from src.topology.common import (
    Option,
    check_time_synchronization,
    parse_generator_option,
)


def pytest_addoption(parser: pytest.Parser):
    """Add topology options."""

    # enable pcap-player topology
    parser.addoption(
        "--pcap-player",
        action="append",
        default=None,
        help=(
            "Enable wired pcap player topology -- main FlowTest topology. "
            "Tests with selected pcap player topology will be runnable. "
            "Use 'alias' to identify generator from configuration file generators.yml. "
            "Optionally, you can add generator parameters after alias. "
            "Standard options are 'vlan' for adding vlan tag to sent packets and "
            "'orig-mac' for disabling rewriting destination mac address. "
            "Any of connector-specific parameters can be appended after alias.\n"
            "E.g.: validation-gen:vlan=90;orig-mac;mtu=3500"
        ),
    )


@pytest_cases.fixture(scope="session")
def topology_pcap_player(
    request: pytest.FixtureRequest,
    config: Config,
    option_pcap_player: str,
    probe_option: Optional[Option],
    collector_option: Optional[Option],
) -> Topology:
    """Topology using pcap file player (interface PcapPlayer) as traffic generator.
    For better configuration inside a test, object builders are used.

    Parameters
    ----------
    request : FixtureRequest
        Pytest request object used to add finalizer.
    config : Config
        Global static configuration object. Description of probes, generators and collectors.
    option_pcap_player : str
        Generator alias and additional settings. E.g.: validation-gen:vlan=25;mtu=3500.
    probe_option : Optional[Option]
        Parsed probe option - probe alias and additional settings.
    collector_option : Optional[Option]
        Parsed collector option - collector alias and additional settings.

    Returns
    -------
    Topology
        3 components topology object.
    """

    # Append builder to the list for closing connection in test teardown phase.
    teardown_builders = []

    def finalizer():
        for builder in teardown_builders:
            builder.close_connection()

    request.addfinalizer(finalizer)

    assert probe_option, "Probe cmd argument is required."
    assert collector_option, "Collector cmd argument is required."

    generator_option = parse_generator_option(option_pcap_player)
    assert generator_option, "Generator cmd argument is required."

    probe_option, collector_option, generator_option = (
        deepcopy(probe_option),
        deepcopy(collector_option),
        deepcopy(generator_option),
    )

    disable_ansible = request.config.getoption("disable_ansible")
    extra_import_paths = request.config.getoption("extra_import_path")

    collector_builder = CollectorBuilder(
        config,
        disable_ansible,
        extra_import_paths,
        collector_option.alias,
        collector_option.arguments.pop("protocol"),
        collector_option.arguments.pop("port"),
        collector_option.arguments,
    )
    teardown_builders.append(collector_builder)

    probe_builder = ProbeBuilder(
        config,
        disable_ansible,
        extra_import_paths,
        probe_option.alias,
        collector_builder.get_probe_target(),
        probe_option.arguments.pop("ifc"),
        probe_option.arguments,
    )
    teardown_builders.append(probe_builder)

    generator_builder = GeneratorBuilder(
        config,
        disable_ansible,
        extra_import_paths,
        generator_option.alias,
        [ifc.mac for ifc in probe_builder.get_enabled_interfaces()],
        probe_builder.get_biflow_export(),
        add_vlan=generator_option.arguments.pop("vlan"),
        edit_dst_mac=not generator_option.arguments.pop("orig-mac"),
        cmd_connector_args=generator_option.arguments,
    )
    teardown_builders.append(generator_builder)

    check_time_synchronization(collector_builder, probe_builder, generator_builder)

    return Topology(device=probe_builder, generator=generator_builder, analyzer=collector_builder)


registration.topology_option_register("pcap_player")
