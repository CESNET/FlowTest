"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Contains interface definition which all collectors and
collector output readers must implement.
"""

from abc import ABC, abstractmethod


class CollectorException(Exception):
    """Basic exception raised by collector."""


class CollectorOutputReaderException(Exception):
    """Basic exception raised by collector output reader."""


class CollectorInterface(ABC):
    """Abstract class defining common interface for all collectors."""

    @abstractmethod
    def __init__(self, verbose, **kwargs):
        """Initialize the collector.

        Parameters
        ----------
        verbose : bool
            Increase verbosity of the collector logs.

        kwargs : dict
            Startup arguments.

        Raises
        ------
        CollectorException
            Unable to initialize collector.
        """
        raise NotImplementedError

    @abstractmethod
    def start(self):
        """Start the collector."""
        raise NotImplementedError

    @abstractmethod
    def stop(self):
        """Stop the collector."""
        raise NotImplementedError

    @abstractmethod
    def download_logs(self, directory: str):
        """Download the logs to a given directory.

        Parameters
        ----------
        directory : str
            Path to a local directory where logs should be stored.
        """
        raise NotImplementedError

    @abstractmethod
    def cleanup(self):
        """Delete directory/files created during collector runtime."""
        raise NotImplementedError

    @abstractmethod
    def get_reader(self):
        """Return flow reader.

        Flow reader implements ``__iter__`` and ``__next__`` methods for
        reading flows. Example usages::

            reader = CollectorInterface().get_reader()
            reader = iter(reader)
            flow = next(reader)
            print(flow)

            # second example
            for flow in reader:
                print(flow)

        Returns
        -------
        CollectorOutputReaderInterface
            Flow reader.
        """
        raise NotImplementedError


class CollectorOutputReaderInterface(ABC):
    """Abstract class defining common interface for all collector output readers."""

    @abstractmethod
    def __init__(self, **kwargs):
        """Initialize the collector output reader.

        Parameters
        ----------
        kwargs : dict
            Startup arguments.

        Raises
        ------
        CollectorOutputReaderException
            Unable to initialize collector.
        """
        raise NotImplementedError

    @abstractmethod
    def __iter__(self):
        """Basic iterator.

        Returns
        -------
        CollectorOutputReaderInterface
            Object instance.
        """
        raise NotImplementedError

    @abstractmethod
    def __next__(self):
        """Read next flow entry from collector output.

        Returns
        -------
        dict
            JSON entry in form of dict.

        Raises
        ------
        StopIteration
            No more entries for processing.
        """
        raise NotImplementedError
