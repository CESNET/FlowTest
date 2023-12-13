"""
Author(s): Tomas Jansky <tomas.jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Threshold simulation scenario focuses on finding the maximum link speed that a profile
can be replayed so that the number of lost packets and bytes is below specified threshold.
"""

import ipaddress
import logging
import os
import shutil
from typing import Optional

import pytest
from ftanalyzer.models.sm_data_types import SMRule
from ftanalyzer.models.statistical_model import StatisticalModel
from ftanalyzer.replicator.flow_replicator import FlowReplicator
from lbr_testsuite.topology.topology import select_topologies
from src.collector.collector_builder import CollectorBuilder
from src.common.utils import collect_scenarios, download_logs, get_project_root
from src.config.scenario import AnalysisCfg, SimulationScenario
from src.generator.ft_generator import FtGeneratorConfig
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import MbpsSpeed, Replicator
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
SIMULATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/simulation")
select_topologies(["replicator"])

DEFAULT_REPLICATOR_PREFIX = 8


def validate(
    analysis: AnalysisCfg,
    flows_file: str,
    ref_file: str,
    start_time: int,
) -> bool:
    """Perform statistical and/or precise model evaluation of the test scenario.

    Parameters
    ----------
    analysis: AnalysisCfg
        Object describing how the test results should be evaluated.
    flows_file: str
        Path to a file with flows from collector.
    ref_file: str
        Path to a file with reference flows.
    start_time: int
        Timestamp of the first packet.

    Returns
    -------
    tuple
        Reports from the evaluation. Statistical report is always present,
        precise report is present only if a precise model is selected.
    """

    model = StatisticalModel(flows_file, ref_file, start_time)
    stats_report = model.validate([SMRule(analysis.metrics)])
    stats_report.print_results()
    print("")

    return stats_report.is_passing()


def get_replicator_prefix(min_prefix: int, ipv4_range: Optional[str], ipv6_range: Optional[str]) -> int:
    """Determine the value for replicator prefix so that it does not overlap with provided configuration.

    Parameters
    ----------
    min_prefix: int
        Minimum prefix value which is acceptable.
    ipv4_range: str, None
        IPv4 range settings for the generator.
    ipv6_range: str, None
        IPv6 range settings for the generator.

    Returns
    -------
    int
        Prefix which should be used in the replicator.
    """

    ipv4_prefix = 32 if ipv4_range is None else int(ipv4_range.split("/")[1])
    ipv6_prefix = 32 if ipv6_range is None else int(ipv6_range.split("/")[1])
    configured_prefix = min([ipv4_prefix, ipv6_prefix])

    # check prefixes do not overlap
    assert configured_prefix >= min_prefix

    # ideally we want to use the default prefix so that we can reuse pcaps from a cache
    if configured_prefix >= DEFAULT_REPLICATOR_PREFIX >= min_prefix:
        return DEFAULT_REPLICATOR_PREFIX

    return min_prefix


def setup_replicator(generator: Replicator, conf: FtGeneratorConfig, loop_cnt: int, unit_cnt: int) -> FlowReplicator:
    """
    Setup replicator units and loops so that there is enough bits in an IP prefix
    to perform replication in a way that IP subnets do not overlap.

    Parameters
    ----------
    generator: Replicator
        Generator instance.
    conf: FtGeneratorConfig
        Generator configuration.
    loop_cnt: int
        Number of traffic replay loops.
    unit_cnt: int
        Number of replication units.

    Returns
    -------
    FlowReplicator
        Flow replicator object to adjust reference flows report form the packet player.
    """

    assert loop_cnt > 0 and unit_cnt > 0

    prefix = get_replicator_prefix((loop_cnt * unit_cnt - 1).bit_length(), conf.ipv4.ip_range, conf.ipv6.ip_range)
    if conf.ipv4.ip_range is None:
        conf.ipv4.ip_range = f"{ipaddress.IPv4Address(2 ** (32 - prefix)):s}/{prefix}"

    if conf.ipv6.ip_range is None:
        conf.ipv6.ip_range = f"{ipaddress.IPv6Address(2 ** (128 - prefix)):s}/{prefix}"

    for unit_n in range(unit_cnt):
        generator.add_replication_unit(
            srcip=Replicator.AddConstant(unit_n * 2 ** (32 - prefix)),
            dstip=Replicator.AddConstant(unit_n * 2 ** (32 - prefix)),
        )

    generator.set_loop_modifiers(srcip_offset=unit_cnt * 2 ** (32 - prefix), dstip_offset=unit_cnt * 2 ** (32 - prefix))
    logging.getLogger().info("Generator - ipv4 range: %s, ipv6 range: %s", conf.ipv4.ip_range, conf.ipv6.ip_range)
    logging.getLogger().info("Replicator - units: %d, loops: %d, prefix: %d", unit_cnt, loop_cnt, prefix)

    return FlowReplicator(generator.get_replicator_config())


@pytest.mark.simulation
@pytest.mark.parametrize(
    "scenario, test_id", collect_scenarios(SIMULATION_TESTS_DIR, SimulationScenario, name="sim_threshold")
)
# pylint: disable=too-many-locals
# pylint: disable=too-many-statements
# pylint: disable=unused-argument
def test_simulation_threshold(
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
    """Threshold simulation scenario focuses on finding the maximum link speed that a profile
    can be replayed so that the number of lost packets and bytes is below specified threshold.
    Statistical model is used for evaluation.

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

    current_log_dir = log_dir
    objects_to_cleanup = []
    probe_instance, collector_instance, generator_instance = (None, None, None)

    def cleanup():
        for obj in objects_to_cleanup:
            obj.stop()
            obj.cleanup()

    def finalizer_download_logs():
        download_logs(current_log_dir, collector=collector_instance, generator=generator_instance, probe=probe_instance)

    request.addfinalizer(cleanup)
    request.addfinalizer(finalizer_download_logs)

    def run_single_test(loops: int, speed: MbpsSpeed):
        logging.getLogger().info("running test with speed: %s Mbps (loops: %s)", speed.speed, loops)
        collector_instance.start()
        probe_instance.start()

        flow_replicator = setup_replicator(generator_instance, generator_conf, loops, replicator_units)
        generator_instance.start_profile(
            profile_path, ref_file, speed=speed, loop_count=loops, generator_config=generator_conf
        )

        # method stats blocks until traffic is sent
        stats = generator_instance.stats()
        logging.getLogger().info("stats: %s", stats)

        probe_instance.stop()
        collector_instance.stop()

        collector_instance.get_reader().save_csv(flows_file)
        flow_replicator.replicate(input_file=ref_file, output_file=replicated_ref_file, loops=loops)

        ret = validate(
            analysis=scenario.test.analysis,
            flows_file=flows_file,
            ref_file=replicated_ref_file,
            start_time=stats.start_time,
        )

        return ret

    # general configuration
    probe_conf = scenario.test.get_probe_conf(device.get_instance_type(), scenario.default.probe)
    generator_conf = scenario.test.get_generator_conf(scenario.default.generator)
    profile_path = scenario.get_profile(scenario.filename, SIMULATION_TESTS_DIR)
    replicator_units = int(1 / scenario.sampling)
    ref_file = os.path.join(tmp_dir, "report.csv")
    flows_file = os.path.join(tmp_dir, "flows.csv")
    replicated_ref_file = os.path.join(tmp_dir, "replicated_ref.csv")

    # first run is the original configuration
    speed_min = scenario.test.speed_min
    speed_max = scenario.test.speed_max if scenario.test.speed_max is not None else scenario.requirements.speed * 1000
    speed_current = speed_max
    while True:
        # setup log path
        current_log_dir = os.path.join(log_dir, str(speed_current))

        # create devices
        collector_instance = analyzer.get()
        objects_to_cleanup.append(collector_instance)

        probe_instance = device.get(mtu=scenario.mtu, **probe_conf)
        objects_to_cleanup.append(probe_instance)
        _, inactive_t = probe_instance.get_timeouts()

        generator_instance = generator.get(scenario.mtu)
        generator_conf.max_flow_inter_packet_gap = inactive_t - 1

        # run test
        result = run_single_test(max(1, int(speed_current / scenario.default.mbps)), MbpsSpeed(speed_current))
        if result:
            speed_min = speed_current
        else:
            speed_max = speed_current

        if speed_max - speed_min <= scenario.test.mbps_accuracy:
            break

        # adjust speed
        speed_current = int((speed_max + speed_min) / 2)
        speed_current = speed_current + (scenario.test.mbps_accuracy - speed_current % scenario.test.mbps_accuracy)

        # cleanup devices
        finalizer_download_logs()
        shutil.copytree(tmp_dir, os.path.join(current_log_dir, "data"))
        cleanup()
        objects_to_cleanup = []

    logging.getLogger().info("maximum throughput: %s Mbps (accuracy: %s Mbps)", speed_min, scenario.test.mbps_accuracy)
    assert scenario.test.mbps_required <= speed_min
