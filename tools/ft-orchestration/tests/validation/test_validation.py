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

# pylint: disable=no-name-in-module
import pytest
import yaml
from _pytest.mark import ParameterSet
from ftanalyzer.fields import FieldDatabase
from ftanalyzer.models import ValidationModel
from ftanalyzer.normalizer import Normalizer
from ftanalyzer.reports import ValidationReport, ValidationReportSummary
from lbr_testsuite.topology.topology import select_topologies
from py.xml import html
from scapy.utils import rdpcap
from src.collector.collector_builder import CollectorBuilder
from src.collector.interface import CollectorInterface
from src.collector.mapper import CollectorOutputMapper
from src.common.get_project_root import get_project_root
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import PcapPlayer, PpsSpeed
from src.probe.interface import ProbeInterface
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
VALIDATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/validation")
PCAP_DIR = os.path.join(PROJECT_ROOT, "pcap")
MAPPER_CONF = os.path.join(PROJECT_ROOT, "conf/ipfixcol2/mapping.yaml")
FIELD_DATABASE_CONF = os.path.join(PROJECT_ROOT, "conf/fields.yml")
LOGS_DIR = os.path.join(os.getcwd(), "logs/validation")

WARN_CLR = "\033[33m"
RST_CLR = "\033[0m"


def pytest_html_report_title(report: "HTMLReport") -> None:
    """Set pytest HTML report title.

    Parameters
    ----------
    report: HTMLReport
        HTML report object from pytest-html plugin.
    """
    report.title = "Validation Test Scenarios"


def pytest_configure(config: pytest.Config) -> None:
    """Modify the table pytest environment. Add summary report to the pytest.

    Parameters
    ----------
    config: pytest.Config
        Pytest configuration object.
    """
    config_path = config.getoption("--config-path")
    config_path = os.path.realpath(config_path) if config_path is not None else ""

    meta = {
        "generator": config.getoption("--pcap-player"),
        "collector": config.getoption("--collector"),
        "probe": config.getoption("--probe"),
        "configuration": config_path,
        "markers": config.getoption("-m"),
        "name filter": config.getoption("-k"),
    }

    # pylint: disable=protected-access
    config._metadata = meta
    pytest.summary_report = ValidationReportSummary()


@pytest.hookimpl(optionalhook=True)
def pytest_html_results_summary(summary: list) -> None:
    """Append additional summary information into the HTML report.

    Parameters
    ----------
    summary: list
        HTML summary section.
    """
    # FLOWS
    summary.append(html.h3("Flows"))
    thead = [html.th("OK"), html.th("ERROR"), html.th("MISSING"), html.th("UNEXPECTED")]

    stats = pytest.summary_report.flows
    thead = html.thead(html.tr(thead))
    tbody = html.tbody(
        html.tr(html.td(stats.ok), html.td(stats.error), html.td(stats.missing), html.td(stats.unexpected))
    )
    summary.append(html.table([thead, tbody], id="environment"))

    # FIELDS
    summary.append(html.h3("Flow Fields"))
    thead = [
        html.th("FIELD"),
        html.th("OK"),
        html.th("ERROR"),
        html.th("MISSING"),
        html.th("UNEXPECTED"),
        html.th("UNCHECKED"),
    ]

    rows = []
    fields_summary = pytest.summary_report.get_fields_summary()
    for name, stats in pytest.summary_report.fields.items():
        rows.append(
            html.tr(
                [
                    html.td(name),
                    html.td(stats.ok),
                    html.td(stats.error),
                    html.td(stats.missing),
                    html.td(stats.unexpected),
                    html.td(stats.unchecked),
                ]
            )
        )

    rows.append(
        html.tr(
            [
                html.td("SUMMARY"),
                html.td(fields_summary.ok),
                html.td(fields_summary.error),
                html.td(fields_summary.missing),
                html.td(fields_summary.unexpected),
                html.td(fields_summary.unchecked),
            ]
        )
    )
    thead = html.thead(html.tr(thead))
    tbody = html.tbody(rows)
    summary.append(html.table([thead, tbody], id="environment"))

    summary.append(html.h4("Fields unrecognized by Mapper"))
    summary.append(html.p(", ".join(pytest.summary_report.unmapped_fields)))
    summary.append(html.h4("Fields unrecognized by Normalizer"))
    summary.append(html.p(", ".join(pytest.summary_report.unknown_fields)))


def pytest_html_results_table_header(cells: list) -> None:
    """Add description column to the HTML report results table.

    Parameters
    ----------
    cells: list
        HTML report results table column names.
    """
    cells.insert(2, html.th("Description"))


def pytest_html_results_table_row(report: "HTMLReport", cells: list) -> None:
    """Modify name and description cells of a test result in the HTML report results table.

    Parameters
    ----------
    report: HTMLReport
        HTML report object from pytest-html plugin.
    cells: list
        HTML report results table row cells.
    """
    if len(report.test_name) > 0:
        cells[1] = html.td(report.test_name)

    cells.insert(2, html.td(report.test_description))


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item) -> None:
    """Add test name and description after each validation test finishes.

    Parameters
    ----------
    item: pytest.Item
        Test item.
    """
    outcome = yield
    report = outcome.get_result()
    if report.when == "teardown":
        report.test_name = getattr(item.function, "test_name", "")
        report.test_description = getattr(item.function, "test_description", "")


def collect_validation_tests() -> list[ParameterSet]:
    """Collect validation tests.

    Returns
    -------
    list
        Collected tests.
    """

    files = os.listdir(VALIDATION_TESTS_DIR)
    files = [f for f in files if ".yml" in f]

    tests = []
    for file in files:
        with open(f"{VALIDATION_TESTS_DIR}/{file}", "r", encoding="utf-8") as test_file:
            validation_test = yaml.safe_load(test_file)
            marks = [getattr(pytest.mark, prot) for prot in validation_test["test"]["required_protocols"]]
            tests.append(pytest.param(validation_test, file, marks=marks, id=file))

    return tests


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
        pytest.summary_report.update_unmapped_fields(item[2])

    return flows


def validate_flows(validation_test: dict, probe: ProbeInterface, received_flows: List[dict]) -> ValidationReport:
    """Validate received flows with reference flows.

    Parameters
    ----------
    validation_test: dict
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

    ref_flows = validation_test.get("flows", None)
    supported_fields = probe.supported_fields()
    special_fields = probe.get_special_fields()

    fdb = FieldDatabase(FIELD_DATABASE_CONF)
    norm = Normalizer(fdb)
    key = validation_test.get("key", fdb.get_key_formats()[0])
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

    pytest.summary_report.update_unknown_fields(norm.pop_skipped_fields())
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


def check_required_fields(test: dict, report: ValidationReport):
    """Check that at least one of the fields marked as required has been checked.

    Parameters
    ----------
    test: dict
        Test definition from yaml.
    report: ValidationReport
        Report with stats of checked fields.
    """

    if not "at_least_one" in test:
        return
    required = test["at_least_one"]
    found = False

    for field in required:
        if field in report.fields_stats and (
            report.fields_stats[field].ok > 0
            or report.fields_stats[field].error > 0
            or report.fields_stats[field].missing > 0
        ):
            found = True
            break

    if not found:
        print(
            f"{WARN_CLR}None of the required fields have been checked (at least one of: {required}).\n"
            f"Test will be skipped...{RST_CLR}"
        )
        pytest.skip(f"None of the required fields have been checked (at least one of: {required}).")


def stop_components(probe: ProbeInterface, collector: CollectorInterface):
    """Stop probe and collector to ensure data are flushed from cache.

    Parameters
    ----------
    probe: ProbeInterface
        Probe to be stopped.
    collector: CollectorInterface
        Collector to be stopped.
    """

    logging.info("\t- Stop probe.")
    probe.stop()

    logging.info("\t- Stop collector.")
    collector.stop()


def download_logs(probe: ProbeInterface, collector: CollectorInterface, generator: PcapPlayer, test_name: str):
    """Download logs from component instances and save for further analysis.

    Parameters
    ----------
    probe: ProbeInterface
        Probe instance.
    collector: CollectorInterface
        Collector instance.
    generator: PcapPlayer
        Generator instance.
    test_name: str
        Test name used for log directory naming.
    """

    # for rsync to work correctly, the path must not contain the character ;
    test_name = test_name.replace(";", "_")
    logs_path = os.path.join(LOGS_DIR, test_name)

    logs_dir = os.path.join(logs_path, "probe")
    os.makedirs(logs_dir, exist_ok=True)
    probe.download_logs(logs_dir)

    logs_dir = os.path.join(logs_path, "collector")
    os.makedirs(logs_dir, exist_ok=True)
    collector.download_logs(logs_dir)

    logs_dir = os.path.join(logs_path, "generator")
    os.makedirs(logs_dir, exist_ok=True)
    generator.download_logs(logs_dir)


select_topologies(["pcap_player"])


@pytest.fixture(name="xfail_by_probe")
def fixture_xfail_by_probe(request: pytest.FixtureRequest, test_filename: str, device: ProbeBuilder):
    """The fixture_xfail_by_probe function is a pytest fixture that marks
    the test as xfail if it's in the whitelist. Whitelist must be
    associated in probe static configuration.

    Parameters
    ----------
    request: pytest.FixtureRequest
        Request from pytest.
    test_filename: str
        Get the name of the test file.
    device: ProbeBuilder
        Get the whitelist for the device.
    """

    whitelist = device.get_tests_whitelist()
    if whitelist:
        validation_whitelist = whitelist.get_items("validation")
        if test_filename in validation_whitelist:
            request.applymarker(pytest.mark.xfail(run=True, reason=validation_whitelist[test_filename] or ""))


@pytest.mark.validation
@pytest.mark.parametrize("test, test_filename", collect_validation_tests())
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_validation(
    request: pytest.FixtureRequest,
    test: dict,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    xfail_by_probe,
):
    """Execute validation tests."""

    objects_to_cleanup = []

    def cleanup():
        for obj in objects_to_cleanup:
            obj.stop()
            obj.cleanup()

    request.addfinalizer(cleanup)
    test_validation.test_name = test["test"].get("name", "")
    test_validation.test_description = test["test"].get("description", "No description provided.")

    # every validation test uses only 1 pcap
    pcap_file = test["test"]["pcaps"][0]
    required_protocols = test["test"]["required_protocols"]

    logging.info("\t- Starting collector...")
    collector_instance = analyzer.get()
    objects_to_cleanup.append(collector_instance)
    collector_instance.start()

    logging.info("\t- Starting probe...")
    probe_instance = device.get(required_protocols)
    objects_to_cleanup.append(probe_instance)
    probe_instance.start()

    logging.info("\t- Sending packets via generator to probe...")
    generator_instance = generator.get()
    generator_instance.start(os.path.join(PCAP_DIR, pcap_file), PpsSpeed(10))
    time.sleep(0.1)

    check_generator_stats(generator_instance, pcap_file, generator.get_vlan())
    stop_components(probe_instance, collector_instance)

    received_flows = receive_flows(collector_instance)
    report = validate_flows(test, probe_instance, received_flows)

    download_logs(probe_instance, collector_instance, generator_instance, request.node.name)

    print("")
    report.print_results()
    report.print_flows_stats()
    report.print_fields_stats()

    pytest.summary_report.update_fields_stats(report.fields_stats)
    pytest.summary_report.update_flows_stats(report.flows_stats)

    if not report.is_passing():
        assert False, f"Validation of test {request.node.name} failed"

    check_required_fields(test, report)
