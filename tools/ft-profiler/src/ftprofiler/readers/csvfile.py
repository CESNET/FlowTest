"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

CSVFile reader reads flows from CSV files.
"""

import argparse
import csv
import logging

from ftprofiler.flow import Flow
from ftprofiler.readers.interface import InputException, InputInterface


class CSVFile(InputInterface):
    """CSVFile input reader continuously reads flows from a CSV file.

    Lines starting with char # are skipped.
    First row must contain the header.
    Expected fields: START_TIME (float unix timestamp in seconds, e.g., 1438603883.553),
    DURATION (float in seconds, e.g., 10.5), PROTO (L4), SRC_IP, DST_IP, SRC_PORT, DST_PORT, PACKETS, BYTES

    Attributes
    ----------
    _file : str
        Name of the CSV file.
    _descriptor : TextIOWrapper
        File descriptor.
    _reader : Iterator
        Content of the CSV file.
    _zero_time : int
        Timestamp of the first flow.
    """

    NAME = "csvfile"
    FIELDS = [
        "START_TIME",
        "DURATION",
        "PROTO",
        "SRC_IP",
        "DST_IP",
        "SRC_PORT",
        "DST_PORT",
        "PACKETS",
        "BYTES",
    ]

    def __init__(self, args: argparse.Namespace) -> None:
        """Initialize the reader.

        Parameters
        ----------
        args : argparse.Namespace
            Startup arguments.
        """
        logging.getLogger().info("Initiating flow data reader: %s", self.NAME)
        self._file = args.file
        self._descriptor = None
        self._reader = None
        self._zero_time = None

    def __iter__(self) -> "CSVFile":
        """Basic iterator. Open the CSV file.

        Returns
        -------
        CSVFile
            Iterable object instance.

        Raises
        ------
        InputException
            Problem with opening the CSV file or missing header fields.
        """

        if self._reader is not None:
            self.terminate()

        try:
            # pylint: disable=R1732
            self._descriptor = open(self._file, mode="r", newline="", encoding="ascii")
            # Filter returns generator since python 3.6, therefore it can be used to skip commented lines
            # without loading the whole file.
            self._reader = csv.DictReader(filter(lambda row: row[0] != "#", self._descriptor))
            for field in self.FIELDS:
                if field not in self._reader.fieldnames:
                    logging.getLogger().error("field '%s' not present in the CSV file", field)
                    raise InputException(f"field '{field}' not present in the CSV file")

        except IOError as err:
            logging.getLogger().error("unable to read file=%s, reason=%s", self._file, str(err))
            raise InputException(str(err)) from err

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
            CSV file not open.
        """

        if not self._reader:
            logging.getLogger().error("csv reader not initialized")
            raise InputException("csv reader not initialized")

        while True:
            row = next(self._reader)
            try:
                # pylint: disable=R0801
                start = int(float(row["START_TIME"]) * 1000)
                dur = int(float(row["DURATION"]) * 1000)

                if not self._zero_time:
                    self._zero_time = start

                return Flow(
                    start - self._zero_time,
                    start + dur - self._zero_time,
                    int(row["PROTO"]),
                    row["SRC_IP"],
                    row["DST_IP"],
                    int(row["SRC_PORT"]),
                    int(row["DST_PORT"]),
                    int(row["PACKETS"]),
                    int(row["BYTES"]),
                )
            except ValueError as err:
                logging.getLogger().error("processing row=%s error=%s", row, str(err))

    def terminate(self) -> None:
        """Close the CSV file."""
        self._descriptor.close()
        self._descriptor = None
        self._reader = None
        self._zero_time = None

    @classmethod
    def add_argument_parser(cls, parsers: argparse._SubParsersAction) -> None:
        """Add argument parser to existing argument parsers.

        Parameters
        ----------
        parsers : argparse._SubParsersAction
            Argument parsers which can be extended.
        """
        csvfile_reader = parsers.add_parser(cls.NAME)
        csvfile_reader.add_argument(
            "-f",
            "--file",
            type=str,
            required=True,
            help="specify file to be read",
        )
