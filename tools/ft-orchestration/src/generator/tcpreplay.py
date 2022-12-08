"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Module implements TcpReplay class representing tcpreplay tool.
"""

import logging
import re


class TcpReplay:
    """Class provides means to use tcpreplay tool.

    Parameters
    ----------
    host : Host
        Host class with established connection.
    tcp_edit : bool, optional
        If True, use ``tcpreplay-edit`` instead of ``tcpreplay``.

    Raises
    ------
    RuntimeError
        If tcpreplay or tcpreplay-edit is missing.
    """

    def __init__(self, host, tcp_edit=False):

        if tcp_edit:
            self._bin = "tcpreplay-edit"
        else:
            self._bin = "tcpreplay"

        if host.run(f"command -v {self._bin}", check_rc=False).exited != 0:
            logging.getLogger().error("%s is missing on host %s", self._bin, host.get_host())
            raise RuntimeError(f"{self._bin} is missing")

        self._host = host

    def start(self, cmd_options, asynchronous=False, check_rc=True, timeout=None):
        """Start tcpreplay with given command line options.

        tcpreplay is started with root permissions.

        Parameters
        ----------
        cmd_options : str
            Command line options for tcpreplay. Make sure that correct
            output interface is supplied if tcpreplay is executed on
            remote machine. Paths to PCAP files must be local paths.
            Method will synchronize these files on remote machine.
        asynchronous : bool, optional
            If True, start tcpreplay in asynchronous (non-blocking) mode.
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
        """

        return self._host.run(f"sudo {self._bin} {cmd_options}", asynchronous, check_rc, timeout=timeout)

    def stats(self, result):
        """Get stats based on result from ``start`` method.

        This method will block if tcpreplay was started in asynchronous mode.

        Parameters
        ----------
        result : invoke.runners.Result
            Result from ``start`` method.

        Returns
        -------
        tuple(int, int)
            First element contains number of sent packets.
            Second element contains number of sent bytes.
        """

        if not hasattr(result, "stdout"):
            result = self._host.wait_until_finished(result)

        pkts = int(re.findall(r"(\d+) packets", result.stdout)[0])
        bts = int(re.findall(r"(\d+) bytes", result.stdout)[0])
        return (pkts, bts)

    def stop(self, result):
        """Stop current execution of tcpreplay.

        This method is valid only if tcpreplay was started in
        asynchronous mode. Otherwise it won't have any effect.

        Parameters
        ----------
        result : invoke.runners.Result
            Result from ``start`` method.
        """

        self._host.stop(result)
