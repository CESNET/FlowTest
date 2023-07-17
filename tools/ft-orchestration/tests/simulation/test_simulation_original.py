"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause
"""

import logging
import os
import shutil
import time

import pytest
from ftanalyzer.models.sm_data_types import SMMetric, SMRule
from ftanalyzer.models.statistical_model import StatisticalModel
from lbr_testsuite.topology.topology import select_topologies
from src.collector.collector_builder import CollectorBuilder
from src.common.utils import collect_scenarios, download_logs, get_project_root
from src.config.scenario import SimulationScenario
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import PpsSpeed
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
SIMULATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/simulation")

select_topologies(["pcap_player"])


def validate(metrics: list[SMMetric], flows_file: str, ref_file: str, timeouts: tuple[int, int], start_time: int):
    """TODO"""
    model = StatisticalModel(flows_file, ref_file, timeouts, start_time)

    logging.getLogger().info("performing statistical model evaluation")
    return model.validate([SMRule(metrics=metrics)])


@pytest.mark.simulation
@pytest.mark.parametrize(
    "scenario, scenario_filename", collect_scenarios(SIMULATION_TESTS_DIR, SimulationScenario, name="sm_original")
)
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_simulation_original(
    request: pytest.FixtureRequest,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    scenario: SimulationScenario,
    log_dir: str,
    tmp_dir: str,
    check_requirements,
):
    """TODO"""
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
    probe_instance.start()

    generator_instance = generator.get()

    # file to save replication report from ft-generator (flows reference)
    ref_file = os.path.join(tmp_dir, "reference.csv")
    # timestamp milliseconds precision
    start_time = int(time.time_ns() * 10**-6)
    generator_instance.start_profile(
        os.path.join(SIMULATION_TESTS_DIR, scenario.profile),
        ref_file,
        speed=PpsSpeed(scenario.pps),
        loop_count=1,
        generator_config=scenario.generator,
    )

    time.sleep(0.1)
    probe_instance.stop()
    collector_instance.stop()

    flows_file = os.path.join(tmp_dir, "flows.csv")
    collector_instance.get_reader().save_csv(flows_file)

    report = validate(scenario.analysis, flows_file, ref_file, probe_instance.get_timeouts(), start_time)
    print("")
    report.print_results()
    if not report.is_passing():
        shutil.move(tmp_dir, os.path.join(log_dir, "data"))
        assert False, f"{request.function.__name__}[{scenario.name}] failed"
