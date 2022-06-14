"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Fdsdump iterates over entries in FDS file and return them as JSON/dict.
"""

import json
import logging
import os
import pathlib
import shutil
import subprocess

from src.collector.interface import CollectorOutputReaderException, CollectorOutputReaderInterface


class Fdsdump(CollectorOutputReaderInterface):
    """Iterate over entries in FDS file and return them as JSON/dict.

    Attributes
    ----------
    _file : str
        Input FDS file.
    _cmd : list of strings
        Arguments passed to the fdsdump process on startup.
    _process : subprocess.Popen
        Fdsdump process.
    """

    def __init__(self, file):  # pylint: disable=super-init-not-called
        """Initialize the collector output reader.

        Parameters
        ----------
        file : str
            FDS file to read.

        Raises
        ------
        CollectorOutputReaderException
            File cannot be accessed.
            fdsdump binary could not be located.
        """

        if not os.path.exists(file):
            raise CollectorOutputReaderException(f"Cannot access file '{file}'")

        fdsdump_bin = shutil.which("fdsdump")
        if fdsdump_bin is None:
            raise CollectorOutputReaderException("Unable to locate or execute fdsdump binary")

        self._file = file
        self._cmd = [fdsdump_bin, "-r", str(pathlib.Path(file)), "-o", "json-raw"]
        self._process = None

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

        # pylint: disable=R1732
        self._process = subprocess.Popen(self._cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        try:
            return_code = self._process.wait(1)
        except subprocess.TimeoutExpired:
            # Probably OK
            pass
        else:
            if return_code == 0:
                if len(self._process.stdout.peek(1)) == 0:
                    # fdsdump indeed returns 0 when file doesn't exist
                    logging.getLogger().error("File %s either doesn't exist or it's empty.", self._file)
                    raise CollectorOutputReaderException(f"File {self._file} either doesn't exist or it's empty")

                # fdsdump can end early if there is only a few flows
                return

            error = self._process.stderr.readline()
            logging.getLogger().error("fdsdump return code: %d, error: %s", return_code, error.decode())
            raise CollectorOutputReaderException("fdsdump startup error")

    def _stop(self):
        """Stop fdsdump process."""

        self._process.terminate()
        try:
            self._process.wait(2.5)
        except TimeoutError:
            self._process.kill()

        self._process = None

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

        while True:
            output = self._process.stdout.readline()
            if len(output) == 0:
                self._stop()
                raise StopIteration

            output = output.decode().strip()
            try:
                json_output = json.loads(output)
            except json.JSONDecodeError as err:
                logging.getLogger().error("processing line=%s error=%s", output, str(err))
                continue

            return json_output
