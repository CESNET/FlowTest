"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Contains interface definition which all input flow readers must implement.
"""

import argparse
from abc import ABCMeta, abstractmethod

from profiler.flow import Flow


class InputException(Exception):
    """Basic exception raised by input readers."""

    pass


class InputInterface:
    """Abstract class defining interface between input readers and the core of the profiler."""

    __metaclass__ = ABCMeta

    @abstractmethod
    def __init__(self, args: argparse.Namespace) -> None:
        """Initialize the reader.

        Parameters
        ----------
        args : argparse.Namespace
            Startup arguments.

        Raises
        ------
        InputException
            Unable to initialize input reader.
        """
        raise NotImplementedError

    @abstractmethod
    def __iter__(self) -> "InputInterface":
        """Basic iterator.

        Returns
        -------
        InputInterface
            Object instance.
        """
        raise NotImplementedError

    @abstractmethod
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
            Unrecoverable exception while reading flows.
        """
        raise NotImplementedError

    @abstractmethod
    def terminate(self) -> None:
        """Terminate the reader."""
        raise NotImplementedError

    @classmethod
    @abstractmethod
    def add_argument_parser(cls, parsers: argparse._SubParsersAction) -> None:
        """Add argument parser to existing argument parsers.

        Parameters
        ----------
        parsers : argparse._SubParsersAction
            Argument parsers which can be extended.
        """
        raise NotImplementedError
