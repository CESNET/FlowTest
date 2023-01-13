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
        Login user for the remote connection. If ``password``
        or ``key_filename`` is not set, use SSH agent for authentication.
    password : str, optional
        Password for ``user``. Has higher priority than ``key_filename``.
    key_filename : str, optional
        Path to private key(s) and/or certs. Key must *not* be
        encrypted (must be without password).

    Raises
    ------
    EnvironmentError
        SSH agent is not enabled (only if SSH key verification is used).
    """

    # Session-based storage
    # For storage accessible between sessions, fixed work_dir must be used
    _TEMP_STORAGES = {}

    def __init__(self, host, work_dir=None, user=get_real_user(), password=None, key_filename=None):

        if not ssh_agent_enabled() and password is None and key_filename is None:
            logging.getLogger().error("Missing SSH_AUTH_SOCK environment variable: SSH agent must be running.")
            raise EnvironmentError("Missing SSH_AUTH_SOCK environment variable: SSH agent must be running.")

        connect_kwargs = {}
        if key_filename is not None:
            connect_kwargs = {"key_filename": key_filename}
        if password is not None:
            connect_kwargs = {"password": password, "allow_agent": False, "look_for_keys": False}

        self._connection = fabric.Connection(host, user, connect_kwargs=connect_kwargs)
        self._connection.open()

        if work_dir is not None:
            self._remote_dir = work_dir
        else:
            if host not in self._TEMP_STORAGES:
                self._TEMP_STORAGES[host] = {}
                if user not in self._TEMP_STORAGES[host]:
                    self._TEMP_STORAGES[host][user] = self._connection.run("mktemp -d", hide=True).stdout.strip("\n")

            self._remote_dir = self._TEMP_STORAGES[host][user]

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

        Raises
        ------
        RuntimeError
            sshpass is not installed (only if password verification is used).
        """

        checksum = "--checksum" if checksum_diff else ""
        sshpass = ""
        rsh = ""
        connect_kwargs = self._connection.connect_kwargs

        # rsync doesn't support SSH login via password, thus
        # sshpass is used to provide password automatically
        if "password" in connect_kwargs:
            if self._connection.local("command -v sshpass", warn=True, hide=True).exited != 0:
                logging.getLogger().error("sshpass binary is missing")
                raise RuntimeError("sshpass binary is missing")

            sshpass = f"sshpass -p {connect_kwargs['password']}"

        # Provide SSH key to rsync via --rsh (remote-shell) parameter
        if "key_filename" in connect_kwargs and "password" not in connect_kwargs:
            key = connect_kwargs["key_filename"][0]
            rsh = f"--rsh='ssh -i {key}'"

        # Use os.environ to preserve SSH agent
        self._connection.local(
            f"{sshpass} rsync {checksum} --recursive {rsh}"
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
