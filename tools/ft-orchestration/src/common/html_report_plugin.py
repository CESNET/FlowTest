"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Pytest plugin responsible for customizing the HTML report (pytest-html).
"""

import os

# pylint: disable=no-name-in-module
import pytest
import pytest_html
from ftanalyzer.reports import StatisticalReportSummary, ValidationReportSummary
from py.xml import html
from src.config.scenario import ScenarioCfg


# pylint: disable=too-few-public-methods
class HTMLReportData:
    """Global (static) class holding test results summaries.

    Attributes
    ----------
    validation_summary_report: ValidationReportSummary
        Summary of validation tests.
    simulation_summary_report: StatisticalReportSummary
        Summary of simulation tests (statistical metrics).
    """

    validation_summary_report: ValidationReportSummary
    simulation_summary_report: StatisticalReportSummary


def pytest_html_report_title(report: "HTMLReport") -> None:
    """Set pytest HTML report title.

    Parameters
    ----------
    report: HTMLReport
        HTML report object from pytest-html plugin.
    """
    report.title = "FlowTest Scenarios"


def pytest_configure(config: pytest.Config) -> None:
    """Modify the table pytest environment. Add summary report to the pytest.

    Parameters
    ----------
    config: pytest.Config
        Pytest configuration object.
    """
    config_path = config.getoption("--config-path")
    assert isinstance(config_path, (str, type(None)))
    config_path = os.path.realpath(config_path) if config_path is not None else ""

    meta = {
        "collector": config.getoption("--collector"),
        "probe": config.getoption("--probe"),
        "configuration": config_path,
        "markers": config.getoption("-m"),
        "name filter": config.getoption("-k"),
    }
    pcap_player = config.getoption("--pcap-player")
    if pcap_player:
        meta["generator (pcap-player)"] = pcap_player
    replicator = config.getoption("--replicator")
    if replicator:
        meta["generator (replicator)"] = replicator

    # pylint: disable=protected-access
    config._metadata = meta
    HTMLReportData.validation_summary_report = ValidationReportSummary()
    HTMLReportData.simulation_summary_report = StatisticalReportSummary()


def validation_summary(summary: list) -> None:
    """Append summary information for validation tests into the HTML report.

    Parameters
    ----------
    summary: list
        HTML summary section.
    """

    summary.append(html.h2("Validation Tests Summary"))

    # FLOWS
    summary.append(html.h3("Flows"))
    thead = [html.th("OK"), html.th("ERROR"), html.th("MISSING"), html.th("UNEXPECTED")]

    stats = HTMLReportData.validation_summary_report.flows
    thead = html.thead(html.tr(thead))
    tbody = html.tbody(
        html.tr(
            html.td(str(stats.ok)),
            html.td(str(stats.error)),
            html.td(str(stats.missing)),
            html.td(str(stats.unexpected)),
        )
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
    fields_summary = HTMLReportData.validation_summary_report.get_fields_summary()
    for name, stats in HTMLReportData.validation_summary_report.fields.items():
        rows.append(
            html.tr(
                [
                    html.td(name),
                    html.td(str(stats.ok)),
                    html.td(str(stats.error)),
                    html.td(str(stats.missing)),
                    html.td(str(stats.unexpected)),
                    html.td(str(stats.unchecked)),
                ]
            )
        )

    rows.append(
        html.tr(
            [
                html.td("SUMMARY"),
                html.td(str(fields_summary.ok)),
                html.td(str(fields_summary.error)),
                html.td(str(fields_summary.missing)),
                html.td(str(fields_summary.unexpected)),
                html.td(str(fields_summary.unchecked)),
            ]
        )
    )
    thead = html.thead(html.tr(thead))
    tbody = html.tbody(rows)
    summary.append(html.table([thead, tbody], id="environment"))

    summary.append(html.h4("Fields unrecognized by Mapper"))
    summary.append(html.p(", ".join(HTMLReportData.validation_summary_report.unmapped_fields)))
    summary.append(html.h4("Fields unrecognized by Normalizer"))
    summary.append(html.p(", ".join(HTMLReportData.validation_summary_report.unknown_fields)))


def simulation_summary(summary: list) -> None:
    """Append summary information for simulation tests into the HTML report.

    Parameters
    ----------
    summary: list
        HTML summary section.
    """

    summary.append(html.h2("Simulation Tests Summary"))

    thead = [html.th("METRIC"), html.th("AVG DIFF")]

    stats = HTMLReportData.simulation_summary_report.get_summary()
    thead = html.thead(html.tr(thead))
    rows = [html.tr(html.td(metric.value), html.td(f"{value:.4f}")) for metric, value in stats.items()]
    tbody = html.tbody(rows)
    summary.append(html.table([thead, tbody], id="environment"))


@pytest.hookimpl(optionalhook=True)
def pytest_html_results_summary(summary: list) -> None:
    """Append additional summary information into the HTML report.

    Parameters
    ----------
    summary: list
        HTML summary section.
    """

    if not HTMLReportData.validation_summary_report.is_empty():
        validation_summary(summary)

    if not HTMLReportData.simulation_summary_report.is_empty():
        simulation_summary(summary)


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

    if hasattr(report, "test_name") and len(report.test_name) > 0:
        cells[1] = html.td(report.test_name)

    if hasattr(report, "test_description"):
        cells.insert(2, html.td(report.test_description))
    else:
        cells.insert(2, html.td(""))


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
    ci_project_dir = os.getenv("CI_PROJECT_DIR")
    relative_logs_path = os.path.relpath(os.path.realpath(relative_logs_path), ci_project_dir)
    if ci_job_url:
        return f"{ci_job_url}/artifacts/browse/{relative_logs_path}"
    return relative_logs_path


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Function) -> None:
    """Add test name and description after each validation test finishes.

    Parameters
    ----------
    item: pytest.Item
        Test item.
    """

    outcome = yield
    report = outcome.get_result()

    if report.when == "teardown":
        # assign test name and description to report (shown in result table)
        try:
            scenario = item.callspec.params.get("scenario")
            if not isinstance(scenario, ScenarioCfg):
                raise ValueError
            report.test_name = scenario.name
            report.test_description = scenario.description
        except (AttributeError, ValueError):
            report.test_name = ""
            report.test_description = ""

        # assign logs url to test report (shown in extra in result table)
        logs_path = item.funcargs.get("log_dir", None)
        if logs_path and isinstance(logs_path, str):
            extra = getattr(report, "extra", [])
            html_path = item.config.getoption("htmlpath")
            if html_path:
                assert isinstance(html_path, str)
                relative_logs_path = os.path.relpath(logs_path, os.path.dirname(html_path))
            else:
                relative_logs_path = logs_path
            extra.append(pytest_html.extras.url(get_logs_url(relative_logs_path), name="Logs"))
            report.extra = extra
