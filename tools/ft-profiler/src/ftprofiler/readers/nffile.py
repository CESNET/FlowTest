"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Nffile reader utilizes Nfdump binary to read specified nffiles from the disk.
"""

import argparse
import logging
import shutil
import subprocess

from ftprofiler.flow import Flow
from ftprofiler.readers.interface import InputException, InputInterface


class Nffile(InputInterface):
    """Nffile input reader continuously reads flows supplied by nfdump process.

    Attributes
    ----------
    _cmd : list of strings
        Arguments passed to the nfdump process on startup.
    _process : subprocess.Popen
        Nfdump process.
    _zero_time : int
        Timestamp of the first flow.
    """

    NAME = "nffile"
    ENCODING = "utf-8"
    NFDUMP_FORMAT = "fmt:%msec,%td,%pr,%sa,%da,%sp,%dp,%pkt,%byt"

    def __init__(self, args: argparse.Namespace) -> None:
        """Initialize the reader.

        Parameters
        ----------
        args : argparse.Namespace
            Startup arguments.

        Raises
        ------
        InputException
            Nfdump binary could not be located.
        """
        logging.getLogger().info("Initiating flow data reader: %s", self.NAME)
        nfdump_bin = shutil.which("nfdump")
        if nfdump_bin is None:
            raise InputException("Unable to locate or execute Nfdump binary")

        self._cmd = [nfdump_bin, "-qN", "-M", args.multidir, "-R", args.read, "-o", self.NFDUMP_FORMAT, "-6"]
        if args.count > 0:
            self._cmd += ["-c", str(args.count)]

        # Skip ARP communication and select data from only a single exporter process.
        self._cmd.append(f"not proto ARP and router ip {args.router} and in if {args.ifcid}")
        self._process = None
        self._zero_time = None

    def __iter__(self) -> "Nffile":
        """Basic iterator. Start nfdump process.

        Returns
        -------
        Nffile
            Iterable Object instance.

        Raises
        ------
        InputException
            Nfdump process exited unexpectedly with an error.
        """

        if self._process is not None:
            self.terminate()

        self._start_nfdump_process()

        return self

    def __next__(self) -> Flow:
        """Read next flow.

        Returns
        -------
        Flow
            Initialized flow object.

        Raises
        ------
        StopIteration
            No more flows for processing.
        InputException
            Nfdump process exited unexpectedly with an error.
        """

        if not self._process:
            logging.getLogger().error("nfdump process not started")
            raise InputException("nfdump process not started")

        while True:
            output = self._process.stdout.readline()
            if len(output) == 0:
                ret_code = self._process.poll()
                # Nfdump still running but empty line observed.
                if ret_code is None:
                    logging.getLogger().debug("nfdump process passed empty line")
                    continue

                # Nfdump exited with an error.
                if ret_code != 0:
                    logging.getLogger().warning("nfdump process exited with code=%d, data may be incomplete", ret_code)

                # Nfdump exited gracefully.
                raise StopIteration

            try:
                # %msec,%td,%pr,%sa,%da,%sp,%dp,%pkt,%byt
                start, dur, proto, s_addr, d_addr, s_port, d_port, pkts, bts = map(
                    str.strip, output.decode().split(sep=",")
                )
            except ValueError as err:
                logging.getLogger().error("processing line=%s error=%s", output.decode(), str(err))
                stderr = self._process.stderr.readline()
                if len(stderr) > 0:
                    logging.getLogger().error(stderr.decode())
                    raise InputException(f"Nfdump error: {stderr.decode()}") from err

                continue

            start = int(start)
            dur = int(float(dur) * 1000)

            if not self._zero_time:
                self._zero_time = start

            return Flow(
                start - self._zero_time,
                start + dur - self._zero_time,
                int(proto),
                s_addr,
                d_addr,
                int(s_port),
                int(d_port),
                int(pkts),
                int(bts),
            )

    def _start_nfdump_process(self) -> None:
        """Starts the nfdump process.

        Raises
        ------
        InputException
            Nfdump process exited unexpectedly with an error.
        """
        logging.getLogger().debug(self._cmd)
        # pylint: disable=R1732
        self._process = subprocess.Popen(self._cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Check if Nfdump has failed.
        try:
            self._process.wait(1)
        except subprocess.TimeoutExpired:
            # Probably not failed.
            pass
        else:
            ret_code = self._process.returncode
            if ret_code != 0:
                error = self._process.stderr.readline()
                logging.getLogger().error("Nfdump return code: %d, error: %s", ret_code, error.decode())
                raise InputException("Nfdump error")

    def terminate(self) -> None:
        """Terminate nfdump process."""
        self._process.terminate()
        try:
            self._process.wait(2)
        except TimeoutError:
            self._process.kill()

        self._process = None
        self._zero_time = None

    @classmethod
    def add_argument_parser(cls, parsers: argparse._SubParsersAction) -> None:
        """Add argument parser to existing argument parsers.

        Parameters
        ----------
        parsers : argparse._SubParsersAction
            Argument parsers which can be extended.
        """
        nffile_reader = parsers.add_parser(cls.NAME)
        nffile_reader.add_argument(
            "-c",
            "--count",
            type=int,
            required=False,
            default=0,
            help="limit the number of records read by nfdump (default: 0 - unlimited)",
        )
        nffile_reader.add_argument(
            "-i",
            "--ifcid",
            type=int,
            required=False,
            default=0,
            help="Interface ID of exporter process (default: 0).",
        )
        nffile_reader.add_argument(
            "-M",
            "--multidir",
            type=str,
            required=True,
            help=(
                "Specify directories where nffile should be searched for."
                "The actual files are provided in R argument. Nfdump -M argument (man nfdump)."
            ),
        )
        nffile_reader.add_argument(
            "-r",
            "--router",
            type=str,
            required=False,
            default="127.0.0.1",
            help="IP address where exporter process creates flow (default: 127.0.0.1)",
        )
        nffile_reader.add_argument(
            "-R",
            "--read",
            type=str,
            required=True,
            help="specify files to be read by nfdump. Nfdump -R argument (man nfdump)",
        )
