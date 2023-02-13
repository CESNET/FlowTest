"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

General purpose function for getting the FlowTest root directory in ft-orchestration package.
"""

from os import path


def get_project_root() -> str:
    """General purpose function for getting the FlowTest root directory in ft-orchestration package.

    Returns
    -------
    str
        Project root directory.
    """

    return path.join(path.dirname(path.realpath(__file__)), "../../../../")
