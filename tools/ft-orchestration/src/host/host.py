"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Module implements Host class providing remote command
execution and file synchronization.
"""

import logging
import pathlib

import fabric
from src.host.common import get_real_user, ssh_agent_enabled
from src.host.storage import RemoteStorage


class Host:
    """Class provides means for remote command execution and
    file synchronization.

    Parameters
    ----------
    host : str, optional
        Host where commands will be executed. Can be hostname or
        IP address. If remote execution on different machine is
        not needed, use ``localhost``.
    storage : storage.RemoteStorage, optional
        Storage object for file synchronization on remote machine.
        If ``host`` is ``localhost``, storage can be empty.

    Raises
    ------
    EnvironmentError
        If SSH agent is not enabled.
    TypeError
        When storage is not set or is incorrect type.
    """

    def __init__(self, host="localhost", storage=None):

        self._connection = fabric.Connection(host, user=get_real_user())

        if host == "localhost":
            self._local = True
        else:
            if not isinstance(storage, RemoteStorage):
                logging.getLogger().error("Storage must be set when host isn't localhost.")
                raise TypeError("Storage must be set when host isn't localhost.")

            if not ssh_agent_enabled():
                logging.getLogger().error("Missing SSH_AUTH_SOCK environment variable: SSH agent must be running.")
                raise EnvironmentError("Missing SSH_AUTH_SOCK environment variable: SSH agent must be running.")

            self._local = False
            self._connection.open()
            self._storage = storage

    def run(self, command, asynchronous=False, check_rc=True, timeout=None):
        """Run command.

        Method automatically synchronizes files/directories if
        command is executed on remote machine.

        Note: wildcards (*) are not supported. Use directory instead.

        Parameters
        ----------
        command : str
            Command to run/execute.
        asynchronous : bool, optional
            If True, start command in asynchronous (non-blocking) mode.
        check_rc : bool, optional
            If True, raise ``invoke.exceptions.UnexpectedExit`` when
            return code is nonzero. If False, do not raise any exception.
        timeout : float, optional
            Raise ``CommandTimedOut`` if command doesn't finish within
            specified timeout (in seconds).

        Returns
        -------
        invoke.runners.Result or invoke.runners.Promise
            Execution result. If ``asynchronous``, Promise is returned.
            Then ``join()`` must be called on Promise to get Result.
        """

        if asynchronous:
            # 'pty' must be True so that kill() command can
            # reach remote shell. Otherwise, kill() will terminate
            # local process but remote process will not be terminated.
            asynchronous_args = {"asynchronous": True, "pty": True}
        else:
            asynchronous_args = {}

        if self._local:
            # 'pty' must be True otherwise termination via timeout won't work properly
            asynchronous_args["pty"] = True
            return self._connection.local(command, hide=True, **asynchronous_args, warn=(not check_rc), timeout=timeout)

        for item in command.split():
            path = pathlib.Path(item)
            if path.exists() and (path.is_dir() or path.is_file()):
                storage_dir = self._storage.get_remote_directory()
                self._storage.push(str(path))
                command = command.replace(str(path), f"{storage_dir}/{path.name}")

        return self._connection.run(command, hide=True, warn=(not check_rc), timeout=timeout, **asynchronous_args)

    @staticmethod
    def stop(promise):
        """Stop current execution of command.

        This method is valid only if command was started in
        asynchronous mode. Otherwise, it won't have any effect.

        User must manually call ``join`` or ``wait_until_finished`` to get
        execution result (return code will be probably nonzero as command
        was forcefully killed).

        Parameters
        ----------
        promise : invoke.runners.Promise
            Promise from ``run`` method.
        """

        if promise.runner.opts["asynchronous"] and promise.runner.opts["pty"]:
            promise.runner.kill()

    @staticmethod
    def wait_until_finished(promise):
        """Wait until execution of command is finished
        and update the promise into result.

        This method is valid only if command was started in
        asynchronous mode. Otherwise, it won't have any effect.

        Parameters
        ----------
        promise : invoke.runners.Promise
            Promise from ``run`` method.

        Returns
        -------
        invoke.runners.Result
            Updated result.
        """

        if hasattr(promise, "join"):
            return promise.join()

        return None
