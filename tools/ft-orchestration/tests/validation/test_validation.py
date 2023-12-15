"""
Author(s):  Dominik Tran <tran@cesnet.cz>
            Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Execute validation tests and report results.

Test executes following steps:
- Read "flowtest/testing/validation" directory to obtain tests,
- Read configuration files and get generator, probe and collector configuration.
- Launch tested probe.
- Send test-defined packets to probe via generator of type PcapPlayer.
- Stop probe to flush all flows from cache.
- Stop collector.
- Read flows received by collector and remap them.
- Compare received flows to reference flows.
- If flows match, report success. Otherwise report failure.
"""

import logging
import os
import time
from typing import List, Optional

import pytest
from ftanalyzer.fields import FieldDatabase
from ftanalyzer.models import ValidationModel
from ftanalyzer.normalizer import Normalizer
from ftanalyzer.reports import ValidationReport
from lbr_testsuite.topology.topology import select_topologies
from scapy.utils import rdpcap
from src.collector.collector_builder import CollectorBuilder
from src.collector.interface import CollectorInterface
from src.collector.mapper import CollectorOutputMapper
from src.common.html_report_plugin import HTMLReportData
from src.common.utils import collect_scenarios, download_logs, get_project_root
from src.config.scenario import ValidationScenario
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import PcapPlayer, PpsSpeed
from src.probe.interface import ProbeInterface
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
VALIDATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/validation")
PCAP_DIR = os.path.join(PROJECT_ROOT, "pcap")
MAPPER_CONF = os.path.join(PROJECT_ROOT, "conf/ipfixcol2/mapping.yaml")
FIELD_DATABASE_CONF = os.path.join(PROJECT_ROOT, "conf/fields.yml")

WARN_CLR = "\033[33m"
RST_CLR = "\033[0m"


def receive_flows(collector: CollectorInterface) -> List[dict]:
    """Receive flows from collector and remap items according to mapping configuration.

    Parameters
    ----------
    collector: CollectorInterface
        Collector to be stopped.

    Returns
    -------
    list
        List of remapped flows.
    """

    mapper = CollectorOutputMapper(collector.get_reader(), MAPPER_CONF)
    flows = []
    for item in mapper:
        flows.append(item[0])
        HTMLReportData.validation_summary_report.update_unmapped_fields(item[2])

    return flows


def validate_flows(scenario: ValidationScenario, probe: ProbeInterface, received_flows: List[dict]) -> ValidationReport:
    """Validate received flows with reference flows.

    Parameters
    ----------
    scenario: ValidationScenario
        Test configuration.
    probe: ProbeInterface
        Probe object.
    received_flows: list
        List of received flows that will be validated.

    Returns
    -------
    ValidationModelReport
        Validation report.
    """

    ref_flows = scenario.flows
    supported_fields = probe.supported_fields()
    special_fields = probe.get_special_fields()

    fdb = FieldDatabase(FIELD_DATABASE_CONF)
    norm = Normalizer(fdb)
    key = scenario.key or fdb.get_key_formats()[0]
    norm.set_key_fmt(key)

    try:
        ref_flows = norm.normalize(ref_flows, True)
        received_flows = norm.normalize(received_flows)

    # Normalization can raise some exceptions
    # Log these exceptions to output so it's clear what caused it
    # This can be removed once all problems are fixed
    except (KeyError, ValueError) as err:
        logging.error("Error in flow normalization: %s", err)
        logging.error(received_flows)
        raise

    HTMLReportData.validation_summary_report.update_unknown_fields(norm.pop_skipped_fields())
    val_model = ValidationModel(key, ref_flows)
    report = val_model.validate(received_flows, supported_fields, special_fields)

    return report


def check_generator_stats(generator_instance: PcapPlayer, pcap_file: str, vlan: Optional[int]):
    """Ensure generator sends expected number of packets and bytes.

    Parameters
    ----------
    generator_instance: PcapPlayer
        tcpreplay that generated traffic.
    pcap_file: str
        Path to PCAP file that was generated.
    vlan: int, optional
        Vlan tag if added to sent packets.
    """

    pcap_stats = rdpcap(os.path.join(PCAP_DIR, pcap_file))
    pcap_packets = len(pcap_stats)
    pcap_bytes = sum(len(pkt) for pkt in pcap_stats)

    stats = generator_instance.stats()
    assert stats.packets == pcap_packets
    # tcpreplay includes Ethernet FSC field in bytes stats, Scapy does not
    expected_bytes = pcap_bytes
    if vlan is not None:
        expected_bytes += 4 * pcap_packets
    assert stats.bytes == expected_bytes


def check_required_fields(at_least_one: list[str], report: ValidationReport):
    """Check that at least one of the fields marked as required has been checked.

    Parameters
    ----------
    at_least_one: list[str]
        List of fields, at least one of which must be checked.
    report: ValidationReport
        Report with stats of checked fields.
    """

    if not at_least_one:
        return

    found = False

    for field in at_least_one:
        if field in report.fields_stats and (
            report.fields_stats[field].ok > 0
            or report.fields_stats[field].error > 0
            or report.fields_stats[field].missing > 0
        ):
            found = True
            break

    if not found:
        print(
            f"{WARN_CLR}None of the required fields have been checked (at least one of: {at_least_one}).\n"
            f"Test will be skipped...{RST_CLR}"
        )
        pytest.skip(f"None of the required fields have been checked (at least one of: {at_least_one}).")


def stop_components(probe: ProbeInterface, collector: CollectorInterface):
    """Stop probe and collector to ensure data are flushed from cache.

    Parameters
    ----------
    probe: ProbeInterface
        Probe to be stopped.
    collector: CollectorInterface
        Collector to be stopped.
    """

    probe.stop()
    collector.stop()


select_topologies(["pcap_player"])


@pytest.mark.validation
@pytest.mark.parametrize("scenario, test_id", collect_scenarios(VALIDATION_TESTS_DIR, ValidationScenario))
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_validation(
    request: pytest.FixtureRequest,
    scenario: ValidationScenario,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    log_dir: str,
    test_id: str,
    xfail_by_probe,
    check_requirements,
):
    """Execute validation tests."""

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

    probe_instance = device.get(**scenario.probe.get_args(device.get_instance_type()))
    objects_to_cleanup.append(probe_instance)
    probe_instance.start()

    generator_instance = generator.get()
    generator_instance.start(os.path.join(PCAP_DIR, scenario.pcap), PpsSpeed(10))
    time.sleep(0.1)

    check_generator_stats(generator_instance, scenario.pcap, generator.get_vlan())
    stop_components(probe_instance, collector_instance)

    received_flows = receive_flows(collector_instance)
    report = validate_flows(scenario, probe_instance, received_flows)

    print("")
    report.print_results()
    report.print_flows_stats()
    report.print_fields_stats()

    HTMLReportData.validation_summary_report.update_fields_stats(report.fields_stats)
    HTMLReportData.validation_summary_report.update_flows_stats(report.flows_stats)

    if not report.is_passing():
        assert False, f"validation test: {request.function.__name__}[{test_id}] failed"

    check_required_fields(scenario.at_least_one, report)
