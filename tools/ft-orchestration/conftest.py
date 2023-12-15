"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Pytest conftest file.
"""

import logging
import os

import pytest


@pytest.fixture(scope="session")
def require_root():
    """Fixture checking whether a test is running under the root."""

    if os.geteuid() != 0:
        pytest.skip("requires root permissions")


def pytest_addhooks(pluginmanager: pytest.PytestPluginManager):
    """Include fixtures from components.

    Register only when plugins were not registered at top-level conftest,
    e.g.: when pytest was started from tools/ft-orchestration.
    """

    plugins = [
        "src.common.fixtures",
        "src.common.html_report_plugin",
        "src.topology.common",
        "src.topology.pcap_player",
        "src.topology.replicator",
    ]
    for plugin in plugins:
        if not pluginmanager.hasplugin(plugin):
            pluginmanager.import_plugin(plugin)


def pytest_configure():
    """Restrict certain loggers so they don't pollute
    output with useless messages.
    """

    logging.getLogger("paramiko.transport").setLevel(logging.WARNING)
