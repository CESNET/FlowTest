"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Integration tests for ftprofiler.
"""

import filecmp
import os
import sys
from unittest.mock import patch

from ftprofiler import core

INTEGRATION_BASE = "tools/ft-profiler/tests/ftprofiler/integration/data"


def assert_reference_output(output_csv: str, reference_csv: str):
    """Assert False if output CSV differs from reference CSV."""
    if not filecmp.cmp(output_csv, reference_csv, shallow=False):
        with open(output_csv, "r", encoding="ascii") as output_file:
            content = output_file.readlines()
        assert False, f"Output profile is different from reference. Content of output profile:\n{content}"


def test_integration_csv_basic():
    """Read flows from csv_basic_input.csv using csvfile reader and write them to output_profile.csv.
    Then compare it with reference csv_basic_profile.csv."""
    input_csv = os.path.join(INTEGRATION_BASE, "csv_basic_input.csv")
    output_csv = os.path.join(INTEGRATION_BASE, "csv_basic_output.csv")
    reference_csv = os.path.join(INTEGRATION_BASE, "csv_basic_profile.csv")
    with patch.object(sys, "argv", [None, "-o", output_csv, "csvfile", "-f", input_csv]):
        assert core.main() == 0
        assert_reference_output(output_csv, reference_csv)
