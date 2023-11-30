"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause
"""

import logging
import os
import time

import pytest
from ftanalyzer.models.sm_data_types import SMMetric, SMRule
from ftanalyzer.models.statistical_model import StatisticalModel
from ftanalyzer.replicator.flow_replicator import FlowReplicator
from lbr_testsuite.topology.topology import select_topologies
from src.collector.collector_builder import CollectorBuilder
from src.common.utils import collect_scenarios, download_logs, get_project_root
from src.config.scenario import SimulationScenario
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import PpsSpeed, Replicator
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
SIMULATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/simulation")

LOOPS = 2
BASE_IPV4 = "64.0.0.0"
BASE_IPV6 = "4000::"
UNIT_SUBNET_BITS = 24
LOOP_SUBNET_BITS = 30


select_topologies(["replicator"])


def validate(
    metrics: list[SMMetric],
    flows_file: str,
    ref_file: str,
    start_time: int,
    replicator_config: dict,
    tmp_dir: str,
):
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

    flows_replicator = FlowReplicator(replicator_config)
    replicated_ref_file = os.path.join(tmp_dir, "replicated_ref.csv")
    flows_replicator.replicate(ref_file, replicated_ref_file, loops=LOOPS)

    model = StatisticalModel(flows_file, replicated_ref_file, start_time)
    return model.validate([SMRule(metrics)])


def setup_replicator(generator_instance: Replicator, sampling: float):
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

    for unit_n in range(units_count):
        generator_instance.add_replication_unit(
            srcip=Replicator.AddConstant(unit_n * 2**UNIT_SUBNET_BITS),
            dstip=Replicator.AddConstant(unit_n * 2**UNIT_SUBNET_BITS),
        )

    assert LOOPS <= 2 ** (32 - LOOP_SUBNET_BITS)

    generator_instance.set_loop_modifiers(srcip_offset=2**LOOP_SUBNET_BITS, dstip_offset=2**LOOP_SUBNET_BITS)

    return generator_instance.get_replicator_config()


@pytest.mark.simulation
@pytest.mark.parametrize(
    "scenario, scenario_filename", collect_scenarios(SIMULATION_TESTS_DIR, SimulationScenario, name="sm_replication")
)
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_simulation_replicator(
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
    """Test with ability of replication original traffic from network profile.
    Replication units are used for sampled profiles to achieve original amount
    of traffic. Traffic could be replayed in more non-overlapping loops.
    Statistical model is used for evaluation of the test.

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
