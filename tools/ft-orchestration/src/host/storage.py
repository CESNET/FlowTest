"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Module implements RemoteStorage class providing access to remote storage.
"""

import logging
import os

import fabric
from src.host.common import get_real_user, ssh_agent_enabled


class RemoteStorage:
    """Class provides access to storage on remote host.

    Parameters
    ----------
    host : str
        Remote host.
    work_dir : str, optional
        Working directory on remote host. If provided, then directory
        must already exist on remote machine. If not provided, then
        temp directory is created.
    user : str, optional
        Login user for the remote connection.

    Raises
    ------
    EnvironmentError
        If SSH agent is not enabled.
    """

    # Session-based storage
    # For storage accessible between sessions, fixed work_dir must be used
    _TEMP_STORAGES = {}

    def __init__(self, host, work_dir=None, user=get_real_user()):

        if not ssh_agent_enabled():
            logging.getLogger().error("Missing SSH_AUTH_SOCK environment variable: SSH agent must be running.")
            raise EnvironmentError("Missing SSH_AUTH_SOCK environment variable: SSH agent must be running.")

        self._connection = fabric.Connection(host, user)
        self._connection.open()

        if work_dir is not None:
            self._remote_dir = work_dir
        elif host in self._TEMP_STORAGES:
            self._remote_dir = self._TEMP_STORAGES[host]
        else:
            self._TEMP_STORAGES[host] = self._connection.run("mktemp -d", hide=True).stdout.strip("\n")
            self._remote_dir = self._TEMP_STORAGES[host]

    def get_remote_directory(self):
        """Return path to storage directory on remote host.

        Returns
        -------
        str
            Storage directory.
        """

        return self._remote_dir

    def push(self, file_or_directory, checksum_diff=False):
        """Push file or directory to remote storage.

        Parameters
        ----------
        file_or_directory : str
            File or directory to push to remote storage.
        checksum_diff : bool, optional
            If file/directory already exists on remote storage, then
            it is replaced only if it differs. By default, difference is
            based on time modification and file size. If set to True, then
            difference is based on checksum (which is slower and more
            resource intensive).
        """

        checksum = "--checksum" if checksum_diff else ""

        # Use os.environ to preserve SSH agent
        self._connection.local(
            f"rsync {checksum} --recursive"
            f" {file_or_directory} {self._connection.user}@{self._connection.host}:{self._remote_dir}",
            env=os.environ,
        )

    def remove(self, file_or_directory):
        """Remove file or directory from remote storage.

        Parameters
        ----------
        file_or_directory : str
            File or directory to remove.
        """

        self._connection.run(f"rm -rf {self._remote_dir}/{file_or_directory}")

    def remove_all(self):
        """Remove everything from remote storage."""

        self._connection.run(f"rm -rf {self._remote_dir}/*")
