"""
Author(s): Tomas Jansky <tomas.jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Pytest fixtures for the use in testing scenarios.
"""

import datetime
import os
import tempfile

import pytest
from src.config.scenario import SimulationScenario
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
def log_dir(request: pytest.FixtureRequest, scenario_filename: str) -> str:
    """Create logging directory from test start time, test function name and name of the scenario file.

    Parameters
    ----------
    request: pytest.FixtureRequest
        Pytest fixture request object.
    scenario_filename: str
        Name of the testing scenario file.

    Returns
    -------
    str
        Path to a directory in which logs should be stored.
    """

    # pylint: disable=global-variable-not-assigned
    global START_TIME

    logs = os.path.join(
        os.getcwd(), f"logs/{START_TIME}/{request.function.__name__}/{scenario_filename.removesuffix('.yml')}"
    )
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
def check_requirements(scenario: SimulationScenario, device: ProbeBuilder, generator: GeneratorBuilder) -> None:
    """Fixture to check requirements given by the test scenario.
    Interface speed is checked on generator and probe. The protocols supported by the probe are also checked.

    Parameters
    ----------
    scenario : SimulationScenario
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
