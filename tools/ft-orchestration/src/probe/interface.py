"""
Author(s):  Vojtech Pecen <vojtech.pecen@progress.com>
            Michal Panek <michal.panek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Contains interface definition which all probes must implement.
"""

from abc import ABC, abstractmethod


class ProbeException(Exception):
    """Basic exception raised by the probe implementations"""


class ProbeInterface(ABC):
    """Abstract class defining common interface for all probes"""

    @abstractmethod
    def __init__(self, host, target, protocols, **kwargs):
        """Initialize the local or remote probe interface as object

        Parameters
        ----------

        host : object
            Initialized host object with the deployed probe.

        target : src.probe.probe_target
            Target object for the exporter/probe

        protocols : list
            List of the networking protocols which the probe should parse and export.

        kwargs : dict
            Additional startup arguments for specific probe variants.

        Raises
        ------
        ProbeException
            Unable to initialize the probe.
        """
        raise NotImplementedError

    @abstractmethod
    def start(self):
        """Start the probe."""
        raise NotImplementedError

    def supported_fields(self):
        """Get list of IPFIX fields the probe may export in its current configuration."""
        raise NotImplementedError

    def get_special_fields(self):
        """Return dictionary of exported fields that need special evaluation."""
        raise NotImplementedError

    def stop(self):
        """Stop the probe."""
        raise NotImplementedError

    def cleanup(self):
        """Clean any artifacts which were created by the connector or the active probe itself."""
        raise NotImplementedError
