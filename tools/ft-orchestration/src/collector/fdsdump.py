"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Fdsdump iterates over entries in FDS file and return them as JSON/dict.
"""

import json
import logging
import time
from os import path

from lbr_testsuite.executable import (
    AsyncTool,
    ExecutableProcessError,
    Executor,
    Rsync,
    Tool,
)
from src.collector.interface import (
    CollectorOutputReaderException,
    CollectorOutputReaderInterface,
)
from src.common.tool_is_installed import assert_tool_is_installed

FDSDUMP_CSV_FIELDS = "flowstart,flowend,proto,srcip,dstip,srcport,dstport,packets,bytes"
ANALYZER_CSV_FIELDS = "START_TIME,END_TIME,PROTOCOL,SRC_IP,DST_IP,SRC_PORT,DST_PORT,PACKETS,BYTES"


class Fdsdump(CollectorOutputReaderInterface):
    """Iterate over entries in FDS file and return them as JSON/dict.

    Attributes
    ----------
    _executor : Executor
        Initialized executor object.
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

    def __init__(self, executor: Executor, file: str):  # pylint: disable=super-init-not-called
        """Initialize the collector output reader.

        Parameters
        ----------
        executor : Executor
            Initialized executor object.
        file : str
            FDS file to read. If running on remote machine, it should be
            path on remote machine.

        Raises
        ------
        CollectorOutputReaderException
            fdsdump binary could not be located.
            File cannot be accessed.
            Two or more FDS files found.
        """

        assert_tool_is_installed("fdsdump", executor)

        command = Tool(f"ls {file} -1", executor=executor, failure_verbosity="silent")
        stdout, _ = command.run()
        if command.returncode() != 0:
            raise CollectorOutputReaderException(f"Cannot access file '{file}'")
        if stdout.count("\n") != 1:
            raise CollectorOutputReaderException("More than one file found.")

        self._executor = executor
        self._file = file
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
        self._buf = iter(self._process.stdout)

        return self

    def _start(self):
        """Starts the fdsdump process.

        Raises
        ------
        CollectorOutputReaderException
            Fdsdump process exited unexpectedly with an error.
        """

        self._process = AsyncTool(self._cmd_json, executor=self._executor)

        try:
            self._process.run()
        except ExecutableProcessError as err:
            logging.getLogger().error("fdsdump return code: %d, error: %s", self._process.returncode(), err)
            raise CollectorOutputReaderException("fdsdump startup error") from err

    def _stop(self):
        """Stop fdsdump process.

        Raises
        ------
        CollectorOutputReaderException
            Fdsdump process exited unexpectedly with an error.
        """

        stdout, _ = self._process.wait_or_kill(1)

        if self._process.returncode() > 0:
            # stderr is redirected to stdout
            # Since stdout could be filled with normal output, print only last line
            err = stdout[-1]
            logging.getLogger().error("fdsdump runtime error: %s, error: %s", self._process.returncode(), err)
            raise CollectorOutputReaderException("fdsdump runtime error")

        self._process = None
        self._buf = None
        self._idx = 0

        return stdout

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

        if self._process is None and self._buf is None:
            logging.getLogger().error("fdsdump process not started")
            raise CollectorOutputReaderException("fdsdump process not started")

        while self._buf is not None:
            try:
                output = next(self._buf)
                json_output = json.loads(output)
                return json_output
            except json.JSONDecodeError as err:
                logging.getLogger().error("processing line=%s error=%s", output, str(err))
                output = self._buf.readline()
                continue
            except StopIteration:
                if self._process is not None:
                    # the process is complete, but all output may not have been processed
                    rest_output = self._stop()
                    self._buf = iter(rest_output.splitlines())
                else:
                    # after processing rest of output
                    self._buf = None

        raise StopIteration

    def save_csv(self, csv_file: str):
        """Convert flows from FDS format to CSV file.
        Used for significant amount of flows in performance testing.

        Parameters
        ----------
        csv_file: str
            Path to CSV file. Local file, CSV will be downloaded when collector running on remote.
        """

        rsync = Rsync(self._executor)
        filename = path.basename(csv_file)
        tmp_file = path.join(rsync.get_data_directory(), filename)

        logging.getLogger().info("Preparing CSV output by calling fdsdump command...")
        start = time.time()
        # use internal (analyzer) column names
        Tool(f"echo '{ANALYZER_CSV_FIELDS}' > {tmp_file}", executor=self._executor).run()
        # discard row with column names from fdsdump
        Tool(f"{self._cmd_csv} | tail -n +2 >> {tmp_file}", executor=self._executor).run()
        end = time.time()
        logging.getLogger().info("CSV output saved in %.2f seconds.", (end - start))

        start = time.time()
        rsync.pull_path(tmp_file, path.dirname(csv_file))
        end = time.time()
        logging.getLogger().info("CSV output downloaded in %.2f seconds.", (end - start))
