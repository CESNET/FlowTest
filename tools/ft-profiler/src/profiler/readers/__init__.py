"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause
"""

import argparse

from .interface import InputException, InputInterface

flow_readers = []


def init_reader(name, args: argparse.Namespace) -> InputInterface:
    """Initialize input reader.

    Parameters
    ----------
    name : str
        Name of the reader.
    args : argparse.Namespace
        Arguments to be passed to the reader.

    Returns
    -------
    InputInterface
        Input reader (object) overriding methods of InputInterface abstract class.

    Raises
    ------
    InputException
        Unknown name of the input reader or error during reader initialization.
    """

    for reader in flow_readers:
        if reader.NAME == name:
            return reader(args)

    raise InputException(f"Unknown input reader: {name}")
