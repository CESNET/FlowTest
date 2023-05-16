"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Module implements TcpReplay class representing tcpreplay tool.
"""

import logging
import re
import shutil
import tempfile
from os import path
from pathlib import Path
from typing import Optional

import invoke
from src.generator.interface import (
    GeneratorException,
    MbpsSpeed,
    MultiplierSpeed,
    PcapPlayer,
    PcapPlayerStats,
    PpsSpeed,
    ReplaySpeed,
    TopSpeed,
)
from src.generator.scapy_rewriter import RewriteRules, rewrite_pcap


# pylint: disable=too-many-instance-attributes
class TcpReplay(PcapPlayer):
    """Class provides means to use tcpreplay tool.

    Parameters
    ----------
    host : Host
        Host class with established connection.
    add_vlan : int, optional
        If specified, vlan header with given tag will be added to replayed packets.
    mtu: int, optional
        Mtu size of interface on which traffic will be replayed.
    verbose : bool, optional
        If True, logs will collect all debug messages.
    """

    # pylint: disable=super-init-not-called
    def __init__(self, host, add_vlan: Optional[int] = None, verbose: bool = False, mtu: int = 2000):
        self._host = host
        self._vlan = add_vlan
        self._interface = None
        self._dst_mac = None
        self._verbose = verbose
        self._mtu = mtu
        self._process = None

        self._bin = "tcpreplay"
        if host.run(f"command -v {self._bin}", check_rc=False).exited != 0:
            logging.getLogger().error("%s is missing on host %s", self._bin, host.get_host())
            raise RuntimeError(f"{self._bin} is missing")

        if self._host.is_local():
            self._log_file = Path(tempfile.mkdtemp(), "tcpreplay.log")
        else:
            self._log_file = Path(self._host.get_storage().get_remote_directory(), "tcpreplay.log")

    def add_interface(self, ifc_name: str, dst_mac: Optional[str] = None):
        """Add interface on which traffic will be replayed.

        Parameters
        ----------
        ifc_name : str
            String name of interface, e.g. os name or pci address.
        dst_mac : str, optional
            If specified, destination mac address will be edited in packets which are replayed on interface.

        Raises
        ------
        RuntimeError
            If more than one interface is added.
        """

        if self._interface:
            raise RuntimeError("Tcpreplay generator supports only one replaying interface.")
        self._interface = ifc_name
        self._dst_mac = dst_mac

    # pylint: disable=arguments-differ
    def start(
        self,
        pcap_path: str,
        speed: ReplaySpeed = MultiplierSpeed(1.0),
        loop_count: int = 1,
        asynchronous=False,
        check_rc=True,
        timeout=None,
    ):
        """Start tcpreplay with given command line options.

        tcpreplay is started with root permissions.

        Parameters
        ----------
        pcap_path : str
            Path to pcap file to replay. Path to PCAP file must be local path.
            Method will synchronize pcap file on remote machine.
        speed : ReplaySpeed, optional
            Argument to modify packets replay speed.
        loop_count : int, optional
            Packets from pcap will be replayed loop_count times.
            Zero or negative value means infinite loop and must be started asynchronous.
        asynchronous : bool, optional
            If True, start tcpreplay in asynchronous (non-blocking) mode.
        check_rc : bool, optional
            If True, raise ``invoke.exceptions.UnexpectedExit`` when
            return code is nonzero. If False, do not raise any exception.
        timeout : float, optional
            Raise ``CommandTimedOut`` if command doesn't finish within
            specified timeout (in seconds).

        Raises
        ------
        RuntimeError
            If tcpreplay or tcpreplay-edit is missing or output interface was not specified.
        GeneratorException
            Tcpreplay process exited unexpectedly with an error.

        """
        if self._process is not None:
            self.stop()

        if not self._interface:
            logging.getLogger().error("no output interface was specified")
            raise RuntimeError("no output interface was specified")

        # negative values mean infinite loop
        loop_count = max(loop_count, 0)

        if loop_count == 0 and not asynchronous:
            logging.getLogger().error("tcpreplay can not be started synchronous in infinite loop")
            raise RuntimeError("tcpreplay can not be started synchronous in infinite loop")

        rewrite_rules = None
        if self._dst_mac or self._vlan is not None:
            rewrite_rules = RewriteRules()
            if self._dst_mac:
                rewrite_rules.edit_dst_mac = self._dst_mac
            if self._vlan is not None:
                rewrite_rules.add_vlan = self._vlan

        cmd_options = []
        cmd_options += [self._get_speed_arg(speed)]
        cmd_options += [f"--loop={loop_count}"]
        cmd_options += ["--preload-pcap"]  # always preload pcap file
        cmd_options += [f"--intf1={self._interface}"]
        if self._verbose:
            cmd_options += ["-v"]

        self._host.run(f"sudo ip link set dev {self._interface} up")
        self._host.run(f"sudo ip link set dev {self._interface} mtu {self._mtu}")

        with tempfile.TemporaryDirectory() as temp_dir:
            if rewrite_rules:
                pcap_path = rewrite_pcap(pcap_path, rewrite_rules, path.join(temp_dir, path.basename(pcap_path)))

            self._process = self._host.run(
                f"(set -o pipefail; sudo {self._bin} {' '.join(cmd_options)} {pcap_path} |& tee -i {self._log_file})",
                asynchronous,
                check_rc,
                timeout=timeout,
            )

        if asynchronous:
            runner = self._process.runner
            if runner.process_is_finished:
                res = self._process.join()
                if res.failed:
                    # If pty is true, stderr is redirected to stdout
                    err = res.stdout if runner.opts["pty"] else res.stderr
                    logging.getLogger().error("tcpreplay return code: %d, error: %s", res.return_code, err)
                    raise GeneratorException("tcpreplay startup error")

    def stats(self) -> PcapPlayerStats:
        """Get stats based on process from ``start`` method.

        This method will block if tcpreplay was started in asynchronous mode.

        Returns
        -------
        PcapPlayerStats
            Class containing statistics of sent packets and bytes.
        """

        process = self._process
        if not hasattr(process, "stdout"):
            process = self._host.wait_until_finished(process)

        pkts = int(re.findall(r"(\d+) packets", process.stdout)[0])
        bts = int(re.findall(r"(\d+) bytes", process.stdout)[0])
        return PcapPlayerStats(pkts, bts)

    def stop(self):
        """Stop current execution of tcpreplay.

        If tcpreplay was started in synchronous mode, do nothing.
        Otherwise stop the process.

        Raises
        ------
        GeneratorException
            Tcpreplay process exited unexpectedly with an error.
        """

        if self._process is None:
            return

        if not isinstance(self._process, invoke.runners.Promise) or not self._process.runner.opts["asynchronous"]:
            self._process = None
            return

        self._host.stop(self._process)
        res = self._host.wait_until_finished(self._process)
        if not isinstance(res, invoke.Result):
            raise TypeError("Host method wait_until_finished returned non result object.")
        runner = self._process.runner

        if res.failed:
            # If pty is true, stderr is redirected to stdout
            # Since stdout could be filled with normal output, print only last 1 line
            err = runner.stdout[-1] if runner.opts["pty"] else runner.stderr
            logging.getLogger().error("tcpreplay runtime error: %s, error: %s", res.return_code, err)
            raise GeneratorException("tcpreplay runtime error")

        self._process = None

    def cleanup(self) -> None:
        """Clean any artifacts which were created by the connector or the tcpreplay itself.
        Input pcap is copied to storage work_dir in Host.run(), so we need to remove everything.
        """
        if self._host.is_local():
            shutil.rmtree(Path.parent(self._log_file))
        else:
            self._host.get_storage().remove_all()

    def download_logs(self, directory: str):
        """Download logs from tcpreplay.

        Parameters
        ----------
        directory : str
            Path to a local directory where logs should be stored.
        """

        if self._host.is_local():
            Path(directory).mkdir(parents=True, exist_ok=True)
            shutil.copy(self._log_file, directory)
        else:
            try:
                self._host.get_storage().pull(self._log_file, directory)
            except RuntimeError as err:
                logging.getLogger().warning("%s", err)

    @staticmethod
    def _get_speed_arg(speed: ReplaySpeed) -> str:
        """Prepare tcpreplay argument according to given replay speed modifier.

        Parameters
        ----------
        speed : ReplaySpeed
            Replay speed modifier.

        Returns
        -------
        str
            Tcpreplay argument.

        Raises
        ------
        TypeError
            If speed parameter has unsupported type.
        """

        if isinstance(speed, MultiplierSpeed):
            return f"--multiplier={speed.multiplier}"
        if isinstance(speed, MbpsSpeed):
            return f"--mbps={speed.mbps}"
        if isinstance(speed, PpsSpeed):
            return f"--pps={speed.pps}"
        if isinstance(speed, TopSpeed):
            return "--topspeed"
        raise TypeError("Unsupported speed type.")
