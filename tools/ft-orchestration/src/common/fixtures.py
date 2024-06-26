"""
Author(s): Tomas Jansky <tomas.jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Pytest fixtures for the use in testing scenarios.
"""

import datetime
import os
import shutil
import tempfile

import pytest
from src.config.scenario import ScenarioCfg
from src.generator.generator_builder import GeneratorBuilder
from src.probe.probe_builder import ProbeBuilder

# pylint: disable=global-statement
# store the pytest start time globally
START_TIME = None


# pylint: disable=unused-argument
def pytest_configure(config: pytest.Config) -> None:
    """Set test start time.

    Parameters
    ----------
    config: pytest.Config
        Pytest Config object.
    """

    global START_TIME
    START_TIME = datetime.datetime.now().strftime("%Y%m%d%H%M%S")


@pytest.fixture(scope="function")
def log_dir(request: pytest.FixtureRequest, test_id: str) -> str:
    """Create logging directory from test start time, test function name and test id.

    Parameters
    ----------
    request: pytest.FixtureRequest
        Pytest fixture request object.
    test_id: str
        ID of the performed test.

    Returns
    -------
    str
        Path to a directory in which logs should be stored.
    """

    # pylint: disable=global-variable-not-assigned
    global START_TIME

    logs = os.path.join(os.getcwd(), f"logs/{START_TIME}/{request.function.__name__}/{test_id}")
    os.makedirs(logs, exist_ok=True)

    yield logs


@pytest.fixture(scope="function")
def tmp_dir() -> str:
    """Create a temporary working directory for a testing scenario.
    The temporary directory is removed once a scenario is concluded.

    Returns
    -------
    str
        Path to the temporary directory.
    """

    with tempfile.TemporaryDirectory() as tmp:
        yield tmp


@pytest.fixture(scope="function")
def check_requirements(scenario: ScenarioCfg, device: ProbeBuilder, generator: GeneratorBuilder) -> None:
    """Fixture to check requirements given by the test scenario.
    Interface speed is checked on generator and probe. The protocols supported by the probe are also checked.

    Parameters
    ----------
    scenario : ScenarioCfg
        Test scenario dataclass.
    device : ProbeBuilder
        Probe builder to gather interfaces speed and supported protocols.
    generator : GeneratorBuilder
        Generator builder used to gather interfaces speed.
    """

    if scenario.requirements.speed is not None:
        if not all(ifc.speed >= scenario.requirements.speed for ifc in device.get_enabled_interfaces()):
            pytest.skip(
                "Some of the enabled PROBE interfaces do not meet speed requirement defined by test scenario "
                f"({scenario.requirements.speed} Gbps)."
            )
        if not all(ifc.speed >= scenario.requirements.speed for ifc in generator.get_enabled_interfaces()):
            pytest.skip(
                "Some of the enabled GENERATOR interfaces do not meet speed requirement defined by test scenario "
                f"({scenario.requirements.speed} Gbps)."
            )

    if scenario.requirements.protocols and len(scenario.requirements.protocols) > 0:
        if not set(scenario.requirements.protocols).issubset(set(device.get_supported_protocols())):
            pytest.skip(
                "Probe does not support all of the protocols required by test scenario "
                f"({', '.join(scenario.requirements.protocols)})."
            )


@pytest.fixture(name="xfail_by_probe")
def fixture_xfail_by_probe(request: pytest.FixtureRequest, test_id: str, device: ProbeBuilder):
    """The fixture_xfail_by_probe function is a pytest fixture that marks
    the test as xfail if it's in the whitelist. Whitelist must be
    associated in probe static configuration.

    Parameters
    ----------
    request: pytest.FixtureRequest
        Request from pytest.
    test_id: str
        Test id.
    device: ProbeBuilder
        Get the whitelist for the device.
    """

    whitelist = device.get_tests_whitelist()
    if whitelist:
        for mark in request.node.iter_markers():
            if mark.name == "parametrize":
                continue
            whitelist_segment = whitelist.get_items(mark.name)
            if whitelist_segment and test_id in whitelist_segment:
                request.applymarker(pytest.mark.xfail(run=True, reason=whitelist_segment[test_id] or ""))


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Function):
    """Hook to save CSV data when archive-test-data argument is specified.

    Parameters
    ----------
    item: pytest.Function
        Test item.
    """

    outcome = yield
    report = outcome.get_result()

    if report.when == "call":
        archive_data = item.config.getoption("archive_test_data")
        if (archive_data == "failed" and report.outcome == "failed") or archive_data == "always":
            _tmp_dir = item.funcargs.get("tmp_dir")
            _log_dir = item.funcargs.get("log_dir")
            if isinstance(_tmp_dir, str) and isinstance(_log_dir, str):
                shutil.move(_tmp_dir, os.path.join(_log_dir, "data"))
