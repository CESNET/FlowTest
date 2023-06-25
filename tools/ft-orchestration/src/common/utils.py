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
