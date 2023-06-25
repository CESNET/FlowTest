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
