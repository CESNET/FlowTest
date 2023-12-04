"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause
"""

import ipaddress
import logging
import os
import time

import pytest
from ftanalyzer.models.sm_data_types import SMMetric, SMRule, SMSubnetSegment
from ftanalyzer.models.statistical_model import StatisticalModel
from ftanalyzer.replicator.flow_replicator import FlowReplicator
from ftanalyzer.reports.statistical_report import StatisticalReport
from lbr_testsuite.topology.topology import select_topologies
from src.collector.collector_builder import CollectorBuilder
from src.common.utils import collect_scenarios, download_logs, get_project_root
from src.config.scenario import SimulationScenario
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import PpsSpeed, Replicator
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
SIMULATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/simulation")

BASE_IPV4 = "64.0.0.0"
BASE_IPV6 = "4000::"
UNIT_SUBNET_BITS = 24
LOOP_SUBNET_BITS = 30
OVERLOAD_MULTIPLIER = 1

# first loop warm up, second probe overload, third back to normal function
# first and third is evaluated
LOOPS = 3

select_topologies(["replicator"])


def prepare_validation_rules(metrics: list[SMMetric]) -> list[SMRule]:
    """Create rules to evaluate exported flows.
    Calculate subnets for loops in which is evaluation is performed.

    Parameters
    ----------
    metrics : list[SMMetric]
        Metrics for statistical evaluation.

    Returns
    -------
    list[SMRule]
        Evaluation rules.
    """

    network_prefix = 32 - LOOP_SUBNET_BITS
    network_ipv4 = ipaddress.ip_address(BASE_IPV4)
    network_ipv6 = ipaddress.ip_address(BASE_IPV6)

    segment_first_v4 = SMSubnetSegment(f"{network_ipv4}/{network_prefix}", f"{network_ipv4}/{network_prefix}")
    segment_third_v4 = SMSubnetSegment(
        f"{network_ipv4+2*2**LOOP_SUBNET_BITS}/{network_prefix}",
        f"{network_ipv4+2*2**LOOP_SUBNET_BITS}/{network_prefix}",
    )

    segment_first_v6 = SMSubnetSegment(f"{network_ipv6}/{network_prefix}", f"{network_ipv6}/{network_prefix}")
    segment_third_v6 = SMSubnetSegment(
        f"{network_ipv6+2*2**(LOOP_SUBNET_BITS+96)}/{network_prefix}",
        f"{network_ipv6+2*2**(LOOP_SUBNET_BITS+96)}/{network_prefix}",
    )

    return [
        SMRule(metrics, segment_first_v4),
        SMRule(metrics, segment_third_v4),
        SMRule(metrics, segment_first_v6),
        SMRule(metrics, segment_third_v6),
    ]


def validate(
    metrics: list[SMMetric],
    flows_file: str,
    ref_file: str,
    start_time: int,
    replicator_config: dict,
    tmp_dir: str,
) -> StatisticalReport:
    """Perform statistical model evaluation of the test scenario with provided metrics.

    Parameters
    ----------
    metrics : list
        Metrics for statistical evaluation.
    flows_file: str
        Path to a file with flows from collector.
    ref_file: str
        Path to a file with reference flows.
    start_time: int
        Timestamp of the first packet.
    replicator_config : dict
        Config used to replicate flows with FlowReplicator.
        Probably the same as ft-replay configuration.
    tmp_dir : str
        Temporary directory used to save replicated flows CSV.

    Returns
    -------
    StatisticalReport
        Evaluation report.
    """

    flows_replicator = FlowReplicator(replicator_config, ignore_loops=[1])
    replicated_ref_file = os.path.join(tmp_dir, "replicated_ref.csv")
    flows_replicator.replicate(ref_file, replicated_ref_file, loops=LOOPS)

    model = StatisticalModel(flows_file, replicated_ref_file, start_time)
    return model.validate(prepare_validation_rules(metrics))


def setup_replicator(generator_instance: Replicator, sampling: float) -> dict:
    """Setup replication units and loops offsets.

    Parameters
    ----------
    generator_instance : Replicator
        Object to setup (replicator connector).
    sampling : float
        Sampling rate (interval 0-1). Used to find out number of
        replication units to simulate original traffic.

    Returns
    -------
    dict
        Used replicator configuration.
    """

    units_count = int(1 / sampling)

    assert units_count <= 2 ** (LOOP_SUBNET_BITS - UNIT_SUBNET_BITS)

    # normal traffic replication units
    for unit_n in range(units_count):
        generator_instance.add_replication_unit(
            srcip=Replicator.AddConstant(unit_n * 2**UNIT_SUBNET_BITS),
            dstip=Replicator.AddConstant(unit_n * 2**UNIT_SUBNET_BITS),
            loop_only=[0, 2],
        )

    # overload replication units
    for unit_n in range(OVERLOAD_MULTIPLIER * units_count):
        generator_instance.add_replication_unit(
            srcip=Replicator.AddCounter(unit_n * 2**UNIT_SUBNET_BITS, 1),
            dstip=Replicator.AddCounter(unit_n * 2**UNIT_SUBNET_BITS, 1),
            loop_only=1,
        )

    assert LOOPS <= 2 ** (32 - LOOP_SUBNET_BITS)

    generator_instance.set_loop_modifiers(srcip_offset=2**LOOP_SUBNET_BITS, dstip_offset=2**LOOP_SUBNET_BITS)

    return generator_instance.get_replicator_config()


@pytest.mark.simulation
@pytest.mark.parametrize(
    "scenario, scenario_filename", collect_scenarios(SIMULATION_TESTS_DIR, SimulationScenario, name="sm_overload")
)
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_simulation_overload(
    request: pytest.FixtureRequest,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    scenario: SimulationScenario,
    scenario_filename: str,
    log_dir: str,
    tmp_dir: str,
    check_requirements,
):
    """Test of probe recovery after overload with big amount of flows.
    Statistical model is used for evaluation of the test.

    Overload is realized by "addCounter" replication units.
    Test has 3 traffic loops. In first probe getting warmed up, in second
    is attempt to overload probe. Second loop is not evaluated.
    Third loop is most important for evaluation, traffic is getting back to normal.

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
    scenario_filename: str
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

    logging.info("\t- Starting collector...")
    collector_instance = analyzer.get()
    objects_to_cleanup.append(collector_instance)
    collector_instance.start()

    logging.info("\t- Starting probe...")
    probe_instance = device.get(mtu=scenario.mtu, **scenario.probe.get_args(device.get_instance_type()))
    objects_to_cleanup.append(probe_instance)
    probe_timeouts = probe_instance.get_timeouts()
    probe_instance.start()

    logging.info("\t- Sending packets via generator to probe...")
    generator_instance = generator.get(scenario.mtu)
    replicator_config = setup_replicator(generator_instance, scenario.sampling)
    # file to save replication report from ft-generator (flows reference)
    ref_file = os.path.join(tmp_dir, "report.csv")

    scenario.generator.ipv4.ip_range = f"{BASE_IPV4}/{32-UNIT_SUBNET_BITS}"
    # ft-replay can edit only first 32 bits from IPv6 address
    scenario.generator.ipv6.ip_range = f"{BASE_IPV6}/{32-UNIT_SUBNET_BITS}"
    scenario.generator.max_flow_inter_packet_gap = probe_timeouts[1]

    generator_instance.start_profile(
        scenario.get_profile(scenario_filename, SIMULATION_TESTS_DIR),
        ref_file,
        speed=PpsSpeed(scenario.pps),
        loop_count=LOOPS,
        generator_config=scenario.generator,
    )
    # start_profile is asynchronous, method stats blocks until traffic is send
    start_time = generator_instance.stats().start_time

    time.sleep(0.1)

    logging.info("\t- Stop probe.")
    probe_instance.stop()

    logging.info("\t- Stop collector.")
    collector_instance.stop()

    flows_file = os.path.join(tmp_dir, "flows.csv")
    collector_instance.get_reader().save_csv(flows_file)

    report = validate(
        scenario.analysis,
        flows_file,
        ref_file,
        start_time,
        replicator_config,
        tmp_dir,
    )

    print("")
    report.print_results()

    if not report.is_passing():
        assert False, f"Validation of test {request.node.name} failed"
