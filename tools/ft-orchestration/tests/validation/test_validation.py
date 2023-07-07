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
import pytest_html
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


def get_logs_url(relative_logs_path: str) -> str:
    """Add url prefix for browsing artifacts in CI.

    Parameters
    ----------
    relative_logs_path: str
        Path to test log directory relative to html report file.

    Returns
    -------
    str
        Url to log directory.
    """

    # tests are running in GitLab CI pipeline
    ci_job_url = os.getenv("CI_JOB_URL")
    if ci_job_url:
        return f"{ci_job_url}/artifacts/browse/{relative_logs_path}"
    return relative_logs_path


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

        extra = getattr(report, "extra", [])
        logs_path = getattr(item.function, "logs_path", None)
        if logs_path:
            html_path = item.config.getoption("htmlpath")
            if html_path:
                relative_logs_path = os.path.relpath(logs_path, os.path.dirname(html_path))
            else:
                relative_logs_path = logs_path
            extra.append(pytest_html.extras.url(get_logs_url(relative_logs_path), name="Logs"))
            report.extra = extra


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

    logging.info("\t- Stop probe.")
    probe.stop()

    logging.info("\t- Stop collector.")
    collector.stop()


select_topologies(["pcap_player"])


@pytest.fixture(name="xfail_by_probe")
def fixture_xfail_by_probe(request: pytest.FixtureRequest, scenario_filename: str, device: ProbeBuilder):
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
        if scenario_filename in validation_whitelist:
            request.applymarker(pytest.mark.xfail(run=True, reason=validation_whitelist[scenario_filename] or ""))


@pytest.mark.validation
@pytest.mark.parametrize("scenario, scenario_filename", collect_scenarios(VALIDATION_TESTS_DIR, ValidationScenario))
# pylint: disable=too-many-locals
# pylint: disable=unused-argument
def test_validation(
    request: pytest.FixtureRequest,
    scenario: ValidationScenario,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
    log_dir: str,
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

    test_validation.test_name = scenario.name
    test_validation.test_description = scenario.description
    test_validation.logs_path = log_dir

    logging.info("\t- Starting collector...")
    collector_instance = analyzer.get()
    objects_to_cleanup.append(collector_instance)
    collector_instance.start()

    logging.info("\t- Starting probe...")
    probe_instance = device.get(**scenario.probe.get_args(device.get_instance_type()))
    objects_to_cleanup.append(probe_instance)
    probe_instance.start()

    logging.info("\t- Sending packets via generator to probe...")
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

    pytest.summary_report.update_fields_stats(report.fields_stats)
    pytest.summary_report.update_flows_stats(report.flows_stats)

    if not report.is_passing():
        assert False, f"Validation of test {request.node.name} failed"

    check_required_fields(scenario.at_least_one, report)
