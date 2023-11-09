"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Check that tool is installed/available on machine.
"""

import logging
from typing import Any

from lbr_testsuite.executable import Tool
from lbr_testsuite.executable.executor import Executor


def assert_tool_is_installed(tool: Any, executor: Executor) -> None:
    """Assert tool is installed/available on machine.

    Parameters
    ----------
    tool : str
        Name of tool/binary to check.
    executor : Executor
        Executor for command execution.

    Returns
    -------
    str
        Absolute path to ``tool``.

    Raises
    ------
    RuntimeError
        If tool is not installed on machine.
    """

    cmd = Tool(f"command -v {tool}", failure_verbosity="no-exception", executor=executor)
    stdout, _ = cmd.run()

    if cmd.returncode() != 0:
        logging.getLogger().error("%s is missing on host %s", tool, executor.get_host())
        raise RuntimeError(f"{tool} is missing on host {executor.get_host()}")

    return stdout.strip()
