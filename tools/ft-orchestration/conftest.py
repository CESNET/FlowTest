"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Pytest conftest file.
"""

import os

import pytest


def pytest_addoption(parser):  # pylint: disable=unused-argument
    """Add pytest options. Currently empty."""


@pytest.fixture(scope="session")
def require_root():
    """Fixture checking whether a test is running under the root."""

    if os.geteuid() != 0:
        pytest.skip("requires root permissions")
