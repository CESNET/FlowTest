"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Provides class for writing Flows in CSV format to a file.
"""

import gzip
import logging
from types import TracebackType
from typing import Optional, Type

from ftprofiler.flow import Flow


class OutputException(Exception):
    """Basic exception raised by output writer."""


class ProfileWriter:
    """Writes flow objects in CSV format into a file.

    Capable of using gzip compression.

    Attributes
    ----------
    _name : str
        Name (path) of the file where flows should be written.
    _header : str
        Header line to be written into the CSV file.
    _compress : bool
        Flag indicating whether gzip compression should be used.
    _written_cnt : int
        Counter for storing the information about the number of written flows.
    """

    def __init__(self, name: str, header: str, compress: bool = True) -> None:
        """Initialize the CSV writer.

        Parameters
        ----------
        name : str
            Path or file name where the flows should be written.
        header : str
            Header line to be written into the CSV file.
        compress : bool, default True
            Flag indicating, whether gzip compression should be used.
        """

        if compress and name.endswith(".gz") is False:
            self._name = f"{name}.gz"
        else:
            self._name = name
        self._header = header
        self._compress = compress
        self._written_cnt = 0
        self._fd = None

    def __enter__(self) -> "ProfileWriter":
        """Open the CSV file for writing (discard current content) and write the header line.

        Returns
        -------
        ProfileWriter
            Object instance.

        Raises
        ------
        OutputException
            Problem occurred when opening the CSV file or writing the header line.
        """

        logging.getLogger().info("Opening file: %s", self._name)
        try:
            if self._compress:
                self._fd = gzip.open(self._name, "wb")
            else:
                self._fd = open(self._name, "w", encoding="ascii")
        except (OSError, IOError) as err:
            logging.getLogger().error("Unable to initialize output: %s", str(err))
            raise OutputException(str(err)) from err

        self.write(self._header)
        return self

    def write(self, flow: Flow) -> None:
        """Append new flow record into the file.

        Parameters
        ----------
        flow : Flow
            Flow to be written.

        Raises
        ------
        OutputException
            Problem occurred when writing the flow into the file.
        """

        try:
            if self._compress:
                self._fd.write((str(flow) + "\n").encode())
            else:
                self._fd.write(str(flow) + "\n")
        except (OSError, IOError) as err:
            logging.getLogger().error("Unable to write to file: %s. Reason: %s", self._name, str(err))
            raise OutputException(str(err)) from err

        self._written_cnt += 1

    def __exit__(
        self,
        ex_type: Optional[Type[BaseException]],
        ex_value: Optional[BaseException],
        ex_trace: Optional[TracebackType],
    ) -> bool:
        """Close the opened file.

        Parameters
        ----------
        ex_type : Type[BaseException], None
            Type of the exception.
        ex_value : BaseException, None
            Value of the exception.
        ex_trace : TracebackType, None
            Traceback of the exception.

        Returns
        -------
        bool
            True - no exception occurred, False otherwise

        Raises
        ------
        BaseException
            Unhandled exception.
        """
        if self._fd is not None:
            self._fd.close()

        logging.getLogger().info("%d lines written to %s", self._written_cnt, self._name)
        return ex_type is None
