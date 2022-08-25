"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Module implements Host class providing remote command
execution and file synchronization.
"""

import glob
import logging
import pathlib

import fabric
import invoke
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
        self._storage = storage
        self._host = host

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

    def is_local(self):
        """Return True if host is local or False if host is remote.

        Returns
        -------
        bool
            Return True if host is local or False if host is remote.
        """

        return self._local

    def get_storage(self):
        """Get remote storage class.

        Returns
        -------
        None or storage.RemoteStorage
            Storage if host is remote. None if host is localhost.
        """

        return self._storage

    def get_host(self):
        """Get hostname/IP address.

        Returns
        -------
        string
            Hostname/IP address.
        """

        return self._host

    def run(self, command, asynchronous=False, check_rc=True, timeout=None):
        """Run command.

        Method automatically synchronizes files/directories if
        command is executed on remote machine.

        If path with wildcards is present in command, it is replaced
        with list of all matched paths. For example, "ls *.txt sample.md"
        can be replaced with "ls a.txt b.txt c.txt sample.md".

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

        storage_dir = self._storage.get_remote_directory()
        for local_path in command.split():
            remote_path = ""
            for path in glob.glob(local_path):
                self._storage.push(str(pathlib.Path(path)))
                remote_path += str(pathlib.Path(storage_dir, pathlib.Path(path).name))
                remote_path += " "
            if len(remote_path) > 0:
                command = command.replace(local_path, remote_path)

        return self._connection.run(command, hide=True, warn=(not check_rc), timeout=timeout, **asynchronous_args)

    @staticmethod
    def stop(promise):
        """Stop current execution of command.

        This method is valid only if command was started in
        asynchronous mode. Otherwise, it won't have any effect.

        User must manually call ``join`` or ``wait_until_finished`` to get
        execution result. Method tries to gracefully terminate
        process (via CTRL+C). If it fails, process is killed after 5 seconds.
        In that case return code can be non-zero.

        Parameters
        ----------
        promise : invoke.runners.Promise
            Promise from ``run`` method.
        """

        if isinstance(promise, invoke.runners.Promise) and promise.runner.opts["asynchronous"]:
            if promise.runner.process_is_finished:
                return

            # Try to gracefully terminate process
            promise.runner.send_interrupt(KeyboardInterrupt)
            # If it won't terminate within 5 seconds, kill it
            promise.runner.start_timer(5)
            promise.runner.wait()

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

        if isinstance(promise, invoke.runners.Promise):
            return promise.join()

        return None
