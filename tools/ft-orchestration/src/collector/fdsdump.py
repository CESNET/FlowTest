"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Fdsdump iterates over entries in FDS file and return them as JSON/dict.
"""

import io
import json
import logging
import shutil
import time
from os import path

from src.collector.interface import (
    CollectorOutputReaderException,
    CollectorOutputReaderInterface,
)

FDSDUMP_CSV_FIELDS = "flowstart,flowend,proto,srcip,dstip,srcport,dstport,packets,bytes"
ANALYZER_CSV_FIELDS = "START_TIME,END_TIME,PROTOCOL,SRC_IP,DST_IP,SRC_PORT,DST_PORT,PACKETS,BYTES"


class Fdsdump(CollectorOutputReaderInterface):
    """Iterate over entries in FDS file and return them as JSON/dict.

    Attributes
    ----------
    _host : Host
        Host class with established connection.
    _file : str
        Input FDS file.
    _cmd : string
        Fdsdump command for startup.
    _process : invoke.runners.Promise
        Representation of Fdsdump process.
    _buf : io.StringIO
        Stream representation of fdsdump output.
    _idx : int
        Current index in fdsdump output.
    """

    def __init__(self, host, file, work_dir):  # pylint: disable=super-init-not-called
        """Initialize the collector output reader.

        Parameters
        ----------
        host : Host
            Host class with established connection.
        file : str
            FDS file to read. If running on remote machine, it should be
            path on remote machine.
        work_dir: str
            Temp (remote) directory used to save helper files when exporting to csv.

        Raises
        ------
        CollectorOutputReaderException
            fdsdump binary could not be located.
            File cannot be accessed.
            Two or more FDS files found.
        """

        if host.run("command -v fdsdump", check_rc=False).exited != 0:
            logging.getLogger().error("fdsdump is missing")
            raise CollectorOutputReaderException("Unable to locate or execute fdsdump binary")

        command = host.run(f"ls {file} -1", check_rc=False, path_replacement=False)
        if command.exited != 0:
            raise CollectorOutputReaderException(f"Cannot access file '{file}'")
        if command.stdout.count("\n") != 1:
            raise CollectorOutputReaderException("More than one file found.")

        self._host = host
        self._file = file
        self._work_dir = work_dir
        self._cmd_json = f"fdsdump -r {file} -o json-raw:no-biflow-split"
        self._cmd_csv = f"fdsdump -r {file} -o 'csv:{FDSDUMP_CSV_FIELDS};timestamp=unix'"
        self._process = None
        self._buf = None
        self._idx = 0

    def __iter__(self):
        """Basic iterator. Start fdsdump process.

        Returns
        -------
        Fdsdump
            Iterable object instance.

        Raises
        ------
        CollectorOutputReaderException
            Fdsdump process exited unexpectedly with an error.
        """

        if self._process is not None:
            self._stop()

        self._start()

        return self

    def _start(self):
        """Starts the fdsdump process.

        Raises
        ------
        CollectorOutputReaderException
            Fdsdump process exited unexpectedly with an error.
        """

        self._process = self._host.run(self._cmd_json, asynchronous=True, check_rc=False)
        # Wait until fdsdump produces some output or fails on startup
        time.sleep(1)

        runner = self._process.runner
        if runner.process_is_finished:
            res = self._process.join()
            if res.failed:
                # If pty is true, stderr is redirected to stdout
                err = res.stdout if runner.opts["pty"] else res.stderr
                logging.getLogger().error("fdsdump return code: %d, error: %s", res.return_code, err)
                raise CollectorOutputReaderException("fdsdump startup error")

    def _stop(self):
        """Stop fdsdump process.

        Raises
        ------
        CollectorOutputReaderException
            Fdsdump process exited unexpectedly with an error.
        """

        self._host.stop(self._process)
        res = self._host.wait_until_finished(self._process)
        runner = self._process.runner

        if res.failed:
            # If pty is true, stderr is redirected to stdout
            # Since stdout could be filled with normal output, print only last line
            err = runner.stdout[-1] if runner.opts["pty"] else runner.stderr
            logging.getLogger().error("fdsdump runtime error: %s, error: %s", res.return_code, err)
            raise CollectorOutputReaderException("fdsdump runtime error")

        self._process = None
        self._buf = None
        self._idx = 0

    def __next__(self):
        """Read next flow entry from FDS file.

        Returns
        -------
        dict
            JSON flow entry in form of dict.

        Raises
        ------
        CollectorOutputReaderException
            Fdsdump process not started.
        StopIteration
            No more flow entries for processing.
        """

        if self._process is None:
            logging.getLogger().error("fdsdump process not started")
            raise CollectorOutputReaderException("fdsdump process not started")

        # Since we can run fdsdump on remote machine, we cannot use
        # subprocess module and it's stream output.
        # Full output is available when fdsdump finishes, but it can
        # take too long when processing a lot of data.
        # Thus we should process output while fdsdump is still processing input.
        # Problem is that output is then stored as ever-expanding list, where
        # each element contains ~1000 characters of output.
        # We must concatenate one flow (separated by \n) when it is spread
        # between 2 or more list elements.

        runner = self._process.runner

        if self._buf is None:
            # In case fdsfile is empty
            if len(runner.stdout) == 0 and runner.process_is_finished:
                self._stop()
                raise StopIteration

            # Convert string into stream, then we can use readline()
            self._buf = io.StringIO(runner.stdout[self._idx])
            # Keep current position in list
            self._idx += 1

        output = self._buf.readline()

        while (
            # In case we read output faster than fdsdump processes input
            (not runner.process_is_finished)
            or
            # We did not process all elements from list
            self._idx < len(runner.stdout)
            or
            # Even if fdsdump finished and we read all list elements,
            # last element can still contain 2 or more flows.
            len(output) > 0
        ):
            # If flow is not complete, read next bulk of stdout to complete it
            if len(output) == 0 or (len(output) > 0 and output[-1] != "\n"):
                next_output = self._buf.readline()
                if len(next_output) == 0:
                    # If we read output too fast, we can exceed runner.stdout[] index range
                    # In that case just wait until more flows are available
                    try:
                        self._buf = io.StringIO(runner.stdout[self._idx])
                        self._idx += 1
                    except IndexError:
                        pass
                output += next_output
                continue

            try:
                json_output = json.loads(output)
            except json.JSONDecodeError as err:
                logging.getLogger().error("processing line=%s error=%s", output, str(err))
                output = self._buf.readline()
                continue
            return json_output

        self._stop()
        raise StopIteration

    def save_csv(self, csv_file: str):
        """Convert flows from FDS format to CSV file.
        Used for significant amount of flows in performance testing.

        Parameters
        ----------
        csv_file: str
            Path to CSV file. Local file, CSV will be downloaded when collector running on remote.
        """

        filename = path.basename(csv_file)
        tmp_file = path.join(self._work_dir, filename)

        logging.getLogger().info("Preparing CSV output by calling fdsdump command...")
        start = time.time()
        # use internal (analyzer) column names
        self._host.run(f"echo '{ANALYZER_CSV_FIELDS}' > {tmp_file}")
        # discard row with column names from fdsdump
        self._host.run(f"{self._cmd_csv} | tail -n +2 >> {tmp_file}")
        end = time.time()
        logging.getLogger().info("CSV output saved in %.2f seconds.", (end - start))

        if self._host.is_local():
            shutil.move(tmp_file, csv_file)
        else:
            logging.getLogger().info("Downloading CSV output to local directory...")
            start = time.time()
            self._host.get_storage().pull(tmp_file, path.dirname(csv_file))
            end = time.time()
            logging.getLogger().info("CSV output downloaded in %.2f seconds.", (end - start))
            self._host.get_storage().remove(tmp_file)
