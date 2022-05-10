"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Common functions used by modules in ``host`` directory.
"""

import os


def get_real_user():
    """Get real user name (even when running under root).

    Returns
    -------
    str
        User name.
    """

    if os.getenv("USER") == "root":
        return os.getenv("SUDO_USER")

    return os.getenv("USER")


def ssh_agent_enabled():
    """Check if SSH agent is running (``SSH_AUTH_SOCK`` environment variable exists).

    Returns
    -------
    bool
        True if ``SSH_AUTH_SOCK`` environment variable exists.
    """

    if os.getenv("SSH_AUTH_SOCK") is not None:
        return True

    return False
