"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Functions with different purpose which can be utilized in testing scenarios.
"""

import logging
import os
from typing import Optional

import pytest
from dataclass_wizard.errors import MissingFields, ParseError
from src.config.scenario import ScenarioCfg, ScenarioCfgException


def get_project_root() -> str:
    """General purpose function for getting the FlowTest root directory in ft-orchestration package.

    Returns
    -------
    str
        Project root directory.
    """

    return os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../../../../"))


def download_logs(dest: str, **kwargs) -> None:
    """Download logs from test components.

    Parameters
    ----------
    dest: str
        Path to a directory where logs should be downloaded to.
    *kwargs: Any
        Objects which logs should be downloaded.
        The argument name will be used to create the directory.
    """

    for name, device in kwargs.items():
        if device is None:
            continue

        logs_dir = os.path.join(dest, name)
        os.makedirs(logs_dir, exist_ok=True)
        device.download_logs(logs_dir)


def collect_scenarios(path: str, target: ScenarioCfg, name: Optional[str] = None) -> list["ParameterSet"]:
    """
    Collect all scenario files in the provided directory.
    The function provides created configuration object and name of the respective scenario file.
    To be used for parametrizing pytest test case to run the test case for every discovered scenario file.

    Parameters
    ----------
    path : str
        Path to a directory where test are situated.
    target: ScenarioCfg
        Configuration class which inherits from ScenarioCfg class.
    name: str, None
        Name of the current test type to be collected. Used to determine
        if a scenario should be excluded from this test (based on the 'exclude'
        list in the scenario configuration). If None, no scenario will be excluded.

    Returns
    -------
    list
        List of ParameterSet objects to parametrize pytest test case.
    """

    if not os.path.isdir(path):
        logging.getLogger().error("Path %s is not a directory", path)
        return []

    try:
        files = [f for f in os.listdir(path) if ".yml" in f]
    except OSError as err:
        logging.getLogger().error("Unable to read directory content: %s, error: %s", path, err)
        return []

    tests = []
    for file in files:
        abspath = os.path.join(path, file)
        logging.getLogger().info("Loading test scenario from file: %s", abspath)
        try:
            test = target.from_yaml_file(abspath)
            test.check()
            marks = []
            if name is not None:
                if name in test.exclude:
                    continue
                marks.append(getattr(pytest.mark, name))
            marks += [getattr(pytest.mark, mark) for mark in test.marks]
            tests.append(pytest.param(test, file, marks=marks, id=file))
        except OSError as err:
            logging.getLogger().error("Error reading scenario file: %s, error: %s", abspath, err)
        except (TypeError, AttributeError, ParseError, MissingFields, ScenarioCfgException) as err:
            logging.getLogger().error("Parsing error of scenario file: %s, error: %s", abspath, err)

    return tests
