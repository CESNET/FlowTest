"""
Author(s): Michal Sedlak <sedlakm@cesnet.cz>

SPDX-License-Identifier: BSD-3-Clause

Fdsfile reader utilizes Fdsdump binary to read specified FDS files from the disk.
"""

import argparse
import json
import logging
import shutil
import subprocess
from datetime import datetime

from ftprofiler.flow import Flow
from ftprofiler.readers.interface import InputException, InputInterface


class Fdsfile(InputInterface):
    """Fdsfile input reader continuously reads flows supplied by fdsdump process.

    Attributes
    ----------
    _cmd : list of strings
        Arguments passed to the fdsdump process on startup.
    _process : subprocess.Popen
        Fdsdump process.
    _zero_time : int
        Timestamp of the first flow.
    """

    NAME = "fdsfile"
    ENCODING = "utf-8"

    def __init__(self, args: argparse.Namespace) -> None:
        """Initialize the reader.

        Parameters
        ----------
        args : argparse.Namespace
            Startup arguments.

        Raises
        ------
        InputException
            Fdsdump binary could not be located.
        """
        logging.getLogger().info("Initiating flow data reader: %s", self.NAME)
        fdsdump_bin = shutil.which("fdsdump")
        if fdsdump_bin is None:
            raise InputException("Unable to locate or execute Fdsdump binary")

        self._cmd = [fdsdump_bin, "-o", "json:srcip,dstip,srcport,dstport,proto,pkts,bytes,flowstart,flowend,tcpflags"]

        for read_pattern in args.read:
            self._cmd += ["-r", read_pattern]

        if args.count > 0:
            self._cmd += ["-c", str(args.count)]

        if args.filter is not None:
            self._cmd += ["-F", args.filter]

        self._process = None
        self._zero_time = None

    def __iter__(self) -> "Fdsfile":
        """Basic iterator. Start fdsdump process.

        Returns
        -------
        Fdsfile
            Iterable Object instance.

        Raises
        ------
        InputException
            Fdsdump process exited unexpectedly with an error.
        """

        if self._process is not None:
            self.terminate()

        self._start_fdsdump_process()

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
            Fdsdump process exited unexpectedly with an error.
        """

        if not self._process:
            logging.getLogger().error("fdsdump process not started")
            raise InputException("fdsdump process not started")

        while True:
            output = self._process.stdout.readline()
            if len(output) == 0:
                ret_code = self._process.poll()
                # Fdsdump still running but empty line observed.
                if ret_code is None:
                    logging.getLogger().debug("fdsdump process passed empty line")
                    continue

                # Fdsdump exited with an error.
                if ret_code != 0:
                    logging.getLogger().warning("fdsdump process exited with code=%d, data may be incomplete", ret_code)

                # Fdsdump exited gracefully.
                raise StopIteration

            try:
                rec = output.decode("utf-8").strip()
                if rec in ("[", "]"):
                    # Remove beginning/trailing bracket as the output is a JSON array
                    continue

                if rec.endswith(","):
                    # Remove the trailing comma so the record can be parsed one at a time
                    rec = rec[:-1]

                rec = json.loads(rec)

                s_addr = rec["srcip"]
                d_addr = rec["dstip"]
                s_port = rec["srcport"]
                d_port = rec["dstport"]
                proto = rec["proto"]
                pkts = rec["pkts"]
                bts = rec["bytes"]
                start = rec["flowstart"]
                end = rec["flowend"]

            except (json.decoder.JSONDecodeError, KeyError) as err:
                logging.getLogger().error("processing line=%s error=%s", output.decode("utf-8"), str(err))
                stderr = self._process.stderr.readline()
                if len(stderr) > 0:
                    logging.getLogger().error(stderr.decode())
                    raise InputException(f"Fdsdump error: {stderr.decode()}") from err

                continue

            start = datetime.strptime(start, "%Y-%m-%dT%H:%M:%S.%fZ")
            end = datetime.strptime(end, "%Y-%m-%dT%H:%M:%S.%fZ")

            if not self._zero_time:
                self._zero_time = start

            return Flow(
                int((start - self._zero_time).total_seconds() * 1000),
                int((end - self._zero_time).total_seconds() * 1000),
                int(proto),
                s_addr,
                d_addr,
                int(s_port),
                int(d_port),
                int(pkts),
                int(bts),
            )

    def _start_fdsdump_process(self) -> None:
        """Starts the fdsdump process.

        Raises
        ------
        InputException
            Fdsdump process exited unexpectedly with an error.
        """
        logging.getLogger().debug(self._cmd)
        # pylint: disable=R1732
        self._process = subprocess.Popen(self._cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Check if Fdsdump has failed.
        try:
            self._process.wait(1)
        except subprocess.TimeoutExpired:
            # Probably not failed.
            pass
        else:
            ret_code = self._process.returncode
            if ret_code != 0:
                error = self._process.stderr.readline()
                logging.getLogger().error("Fdsdump return code: %d, error: %s", ret_code, error.decode())
                raise InputException("Fdsdump error")

    def terminate(self) -> None:
        """Terminate fdsdump process."""
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
        fdsfile_reader = parsers.add_parser(cls.NAME)
        fdsfile_reader.add_argument(
            "-c",
            "--count",
            type=int,
            required=False,
            default=0,
            help="limit the number of records read by fdsdump (default: 0 - unlimited)",
        )
        fdsfile_reader.add_argument(
            "-F",
            "--filter",
            type=str,
            required=False,
            help="filter records, specified in bpf-like syntax (see fdsdump -F switch documentation)",
        )
        fdsfile_reader.add_argument(
            "-R",
            "--read",
            type=str,
            required=True,
            action="append",
            help="specify files to be read by fdsdump. Supports glob patterns.",
        )
