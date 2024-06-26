"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Pytest conftest file.
"""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), "tools/ft-orchestration"))
sys.setrecursionlimit(5000)

# Include fixtures from components. Plugin registration must be defined in the top-level conftest.
pytest_plugins = [
    "src.common.fixtures",
    "src.common.html_report_plugin",
    "src.topology.common",
    "src.topology.pcap_player",
    "src.topology.replicator",
]
