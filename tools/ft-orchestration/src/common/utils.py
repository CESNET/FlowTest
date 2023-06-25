"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Functions with different purpose which can be utilized in testing scenarios.
"""

import os


def get_project_root() -> str:
    """General purpose function for getting the FlowTest root directory in ft-orchestration package.

    Returns
    -------
    str
        Project root directory.
    """

    return os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../../../../"))
