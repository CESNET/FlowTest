"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Simulation scenario focused on replaying traffic which should be processed by the probe without any data loss.
Statistical and precise models are used for evaluating the results. Difference in metrics is not acceptable.
"""

import logging
import os
import shutil
import time

import pytest
from ftanalyzer.models.precise_model import PreciseModel
from ftanalyzer.models.sm_data_types import SMMetric, SMMetricType, SMRule
from ftanalyzer.reports.precise_report import PreciseReport
from ftanalyzer.reports.statistical_report import StatisticalReport
from lbr_testsuite.topology.topology import select_topologies
from src.collector.collector_builder import CollectorBuilder
from src.common.utils import collect_scenarios, download_logs, get_project_root
from src.config.scenario import SimulationScenario
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import MultiplierSpeed
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
SIMULATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/simulation")

select_topologies(["pcap_player"])


def validate(
    flows_file: str, ref_file: str, timeouts: tuple[int, int], start_time: int
) -> tuple[StatisticalReport, PreciseReport]:
    """
    Perform statistical and precise model evaluation of the test scenario.
    Difference in metrics is not acceptable.

    Parameters
    ----------
    flows_file: str
        Path to a file with flows from collector.
    ref_file: str
        Path to a file with reference flows.
    timeouts: tuple
        Active and inactive timeout which used during flow creation process on a probe.
    start_time: int
        Timestamp of the first packet.

    Returns
    -------
    StatisticalReport, PreciseReport
        Evaluation reports.
    """

    model = PreciseModel(flows_file, ref_file, timeouts, start_time)
    logging.getLogger().info("performing precise model evaluation")
    precise_report = model.validate_precise()

    logging.getLogger().info("performing statistical model evaluation")
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]
    stats_report = model.validate([SMRule(metrics=metrics)])

    return stats_report, precise_report


@pytest.mark.simulation
@pytest.mark.parametrize(
    "scenario, scenario_filename", collect_scenarios(SIMULATION_TESTS_DIR, SimulationScenario, name="sm_dropless")
)
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_simulation_dropless(
    request: pytest.FixtureRequest,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    scenario: SimulationScenario,
    scenario_filename: str,
    log_dir: str,
    tmp_dir: str,
    check_requirements,
) -> None:
    """
    Simulation scenario focused on replaying traffic which should be processed by the probe without any data loss.
    Statistical and precise models are used for evaluating the results. Difference in metrics is not acceptable.

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

    probe_instance, collector_instance, generator_instance = (None, None, None)
    objects_to_cleanup = []

    def cleanup():
        for obj in objects_to_cleanup:
            obj.stop()
            obj.cleanup()

    def finalizer_download_logs():
        download_logs(log_dir, collector=collector_instance, generator=generator_instance, probe=probe_instance)

    request.addfinalizer(cleanup)
    request.addfinalizer(finalizer_download_logs)

    collector_instance = analyzer.get()
    objects_to_cleanup.append(collector_instance)
    collector_instance.start()

    probe_instance = device.get(**scenario.probe.get_args(device.get_instance_type()))
    objects_to_cleanup.append(probe_instance)
    probe_timeouts = probe_instance.get_timeouts()
    probe_instance.start()

    generator_instance = generator.get()

    # file to save replication report from ft-generator (flows reference)
    ref_file = os.path.join(tmp_dir, "reference.csv")

    scenario.generator.max_flow_inter_packet_gap = probe_timeouts[1]
    generator_instance.start_profile(
        scenario.get_profile(scenario_filename, SIMULATION_TESTS_DIR),
        ref_file,
        speed=MultiplierSpeed(1.0),
        loop_count=1,
        generator_config=scenario.generator,
    )

    time.sleep(0.1)
    probe_instance.stop()
    collector_instance.stop()

    flows_file = os.path.join(tmp_dir, "flows.csv")
    collector_instance.get_reader().save_csv(flows_file)
    start_time = generator_instance.stats().start_time

    stats_report, precise_report = validate(flows_file, ref_file, probe_timeouts, start_time)
    print("")
    stats_report.print_results()
    precise_report.print_results()
    if not stats_report.is_passing() or not precise_report.is_passing():
        shutil.move(tmp_dir, os.path.join(log_dir, "data"))
        assert False, f"{request.function.__name__}[{scenario.name}] failed"
