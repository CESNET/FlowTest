"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Overload simulation scenario focuses on flooding tested probe with a large amount of network flows
and evaluating the number of processed packets before and after the flooding period.
"""

import ipaddress
import logging
import os
import shutil

import pytest
from ftanalyzer.models.sm_data_types import SMRule, SMSubnetSegment
from ftanalyzer.models.statistical_model import StatisticalModel
from ftanalyzer.replicator.flow_replicator import FlowReplicator
from lbr_testsuite.topology.topology import select_topologies
from src.collector.collector_builder import CollectorBuilder
from src.common.html_report_plugin import HTMLReportData
from src.common.utils import (
    collect_scenarios,
    download_logs,
    get_project_root,
    get_replicator_prefix,
)
from src.config.scenario import SimulationScenario
from src.generator.ft_generator import FtGeneratorConfig
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import MultiplierSpeed, Replicator
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
SIMULATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/simulation")
DEFAULT_REPLICATOR_PREFIX = 8
select_topologies(["replicator"])

# first loop warm up, second probe overload, third back to normal function
# first and third are evaluated
LOOPS = 3


def create_evaluation_segments(
    ipv4_range: str, ipv6_range: str, prefix: int, unit_cnt: int, loop_step: int
) -> list[SMSubnetSegment]:
    """Create evaluation segments based on the replicator configuration.

    Parameters
    ----------
    ipv4_range: str
        PCAP generator IPv4 range.
    ipv6_range: str
        PCAP generator IPv6 range.
    prefix: int
        Prefix used for replication units.
    unit_cnt: int
        Number of replication units for normal loops.
    loop_step: int
        Maximum number of subnets in a single loop.

    Returns
    -------
    list[SMSubnetSegment]
        Evaluation segments.
    """

    ipv4_addr = ipaddress.ip_address(ipv4_range.split("/")[0])
    ipv6_addr = ipaddress.ip_address(ipv6_range.split("/")[0])
    segments = []

    # only first and third loops with normal traffic are evaluated
    for loop_index in [0, 2]:
        for unit_n in range(unit_cnt):
            ipv4_offset = unit_n * 2 ** (32 - prefix) + loop_index * loop_step * 2 ** (32 - prefix)
            ipv6_offset = unit_n * 2 ** (128 - prefix) + loop_index * loop_step * 2 ** (128 - prefix)

            segments.append(
                SMSubnetSegment(f"{ipv4_addr + ipv4_offset}/{prefix}", f"{ipv4_addr + ipv4_offset}/{prefix}")
            )
            segments.append(
                SMSubnetSegment(f"{ipv6_addr + ipv6_offset}/{prefix}", f"{ipv6_addr + ipv6_offset}/{prefix}")
            )

    return segments


def setup_replicator(
    generator: Replicator, conf: FtGeneratorConfig, multiplier: float, unit_cnt: int
) -> tuple[FlowReplicator, list[SMSubnetSegment]]:
    """
    Setup replicator units and loops so that there is enough bits in an IP prefix
    to perform replication in a way that IP subnets do not overlap.

    Parameters
    ----------
    generator: Replicator
        Generator instance.
    conf: FtGeneratorConfig
        Generator configuration.
    multiplier: float
        Multiply the number of replication units during overload phase by specified constant.
    unit_cnt: int
        Number of replication units.

    Returns
    -------
    tuple
        Flow replicator object to adjust reference flows report form the packet player.
    """

    assert unit_cnt > 0
    assert multiplier >= 1.0

    # maximum of replication units running in parallel across all loops (which is the second loop)
    loop_units = int(multiplier * unit_cnt)
    prefix = get_replicator_prefix(
        (loop_units * LOOPS).bit_length(), DEFAULT_REPLICATOR_PREFIX, conf.ipv4.ip_range, conf.ipv6.ip_range
    )
    if conf.ipv4.ip_range is None:
        conf.ipv4.ip_range = f"{ipaddress.IPv4Address(2 ** (32 - prefix)):s}/{prefix}"

    if conf.ipv6.ip_range is None:
        conf.ipv6.ip_range = f"{ipaddress.IPv6Address(2 ** (128 - prefix)):s}/{prefix}"

    # normal traffic replication units
    for unit_n in range(unit_cnt):
        generator.add_replication_unit(
            srcip=Replicator.AddConstant(unit_n * 2 ** (32 - prefix)),
            dstip=Replicator.AddConstant(unit_n * 2 ** (32 - prefix)),
            loop_only=[0, 2],
        )

    # overload replication units
    for unit_n in range(loop_units):
        generator.add_replication_unit(
            srcip=Replicator.AddCounter(unit_n * 2 ** (32 - prefix), 1),
            dstip=Replicator.AddConstant(unit_n * 2 ** (32 - prefix)),
            loop_only=1,
        )

    # loop offset
    generator.set_loop_modifiers(
        srcip_offset=loop_units * 2 ** (32 - prefix), dstip_offset=loop_units * 2 ** (32 - prefix)
    )
    segments = create_evaluation_segments(conf.ipv4.ip_range, conf.ipv6.ip_range, prefix, unit_cnt, loop_units)
    logging.getLogger().info("Generator - ipv4 range: %s, ipv6 range: %s", conf.ipv4.ip_range, conf.ipv6.ip_range)
    logging.getLogger().info("Replicator - units: %d, overload units: %d, prefix: %d", unit_cnt, loop_units, prefix)

    return FlowReplicator(generator.get_replicator_config(), ignore_loops=[1]), segments


@pytest.mark.simulation
@pytest.mark.parametrize(
    "scenario, test_id", collect_scenarios(SIMULATION_TESTS_DIR, SimulationScenario, name="sim_overload")
)
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_simulation_overload(
    request: pytest.FixtureRequest,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    scenario: SimulationScenario,
    test_id: str,
    log_dir: str,
    tmp_dir: str,
    check_requirements,
):
    """Test of probe recovery after overload with big amount of flows.
    Statistical model is used for evaluation of the test.

    Overload is realized by "addCounter" replication units.
    Test has 3 traffic loops. In first probe getting warmed up, in second
    is attempt to overload probe. Second loop is not evaluated.
    Third loop is the most important for evaluation, since the traffic is getting back to normal.

    Parameters
    ----------
    request : pytest.FixtureRequest
        Pytest request object.
    generator: GeneratorBuilder
        Traffic generator builder.
    device: ProbeBuilder
        Tested probe builder.
    analyzer: CollectorBuilder
        Collector builder.
    scenario: SimulationScenario
        Scenario configration.
    test_id: str
        Path to scenario filename.
    log_dir: str
        Directory for storing logs.
    tmp_dir: str
        Temporary directory which can be used for the duration of the testing scenario.
        Removed after the scenario test is concluded.
    check_requirements: fixture
        Function which automatically checks whether the scenario can be started
        with selected generator / probe configuration.
    """

    objects_to_cleanup = []

    def cleanup():
        for obj in objects_to_cleanup:
            obj.stop()
            obj.cleanup()

    probe_instance, collector_instance, generator_instance = (None, None, None)

    def finalizer_download_logs():
        download_logs(log_dir, collector=collector_instance, generator=generator_instance, probe=probe_instance)

    request.addfinalizer(cleanup)
    request.addfinalizer(finalizer_download_logs)

    collector_instance = analyzer.get()
    objects_to_cleanup.append(collector_instance)
    collector_instance.start()

    probe_conf = scenario.test.get_probe_conf(device.get_instance_type(), scenario.default.probe)
    probe_instance = device.get(mtu=scenario.mtu, **probe_conf)
    objects_to_cleanup.append(probe_instance)
    _, inactive_t = probe_instance.get_timeouts()
    probe_instance.start()

    # initialize generator
    generator_conf = scenario.test.get_generator_conf(scenario.default.generator)
    generator_instance = generator.get(scenario.mtu)
    # file to save replication report from ft-generator (flows reference)
    ref_file = os.path.join(tmp_dir, "report.csv")

    # set max inter packet gap in a profile slightly below configured probe's inactive timeout
    generator_conf.max_flow_inter_packet_gap = inactive_t - 1

    flow_replicator, segments = setup_replicator(
        generator_instance, generator_conf, scenario.test.multiplier, int(1 / scenario.sampling)
    )
    speed = scenario.test.speed_multiplier if scenario.test.speed_multiplier is not None else MultiplierSpeed(1.0)
    assert scenario.test.analysis.model == "statistical"

    generator_instance.start_profile(
        scenario.get_profile(scenario.filename, SIMULATION_TESTS_DIR),
        ref_file,
        speed=speed,
        loop_count=LOOPS,
        generator_config=generator_conf,
    )

    # method stats blocks until traffic is sent
    stats = generator_instance.stats()

    probe_instance.stop()
    collector_instance.stop()

    flows_file = os.path.join(tmp_dir, "flows.csv")
    collector_instance.get_reader().save_csv(flows_file)

    replicated_ref = flow_replicator.replicate(
        input_file=ref_file,
        loops=LOOPS,
        speed_multiplier=speed.speed if isinstance(speed, MultiplierSpeed) else 1.0,
    )

    model = StatisticalModel(flows_file, replicated_ref, stats.start_time)
    report = model.validate([SMRule(scenario.test.analysis.metrics, segment) for segment in segments])
    report.print_results()

    HTMLReportData.simulation_summary_report.update_stats("sim_overload", report.is_passing())

    if not report.is_passing():
        shutil.move(tmp_dir, os.path.join(log_dir, "data"))
        assert False, f"evaluation of test: {request.function.__name__}[{test_id}] failed"
