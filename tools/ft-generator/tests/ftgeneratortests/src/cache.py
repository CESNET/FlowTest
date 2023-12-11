"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains FlowCache class.
"""

import csv
from pathlib import Path
from typing import ItemsView, Optional, Union, ValuesView

from ftgeneratortests.src.flow import ExtendedFlow, Flow


class FlowCache:
    """Class used to store Flow data, based on their hash or key.

    Attributes
    ----------
    _cache : dict
        Used to store Flows based on their hash or key.
    """

    def __init__(self):
        """FlowCache class init. Initializes cache."""
        self._cache = {}

    def __len__(self) -> int:
        """Function returns length of cache.

        Returns
        -------
        int
            Length of cache.
        """

        return len(self._cache)

    def __contains__(self, key: Union[Flow, ExtendedFlow, int]) -> bool:
        """Function verifies if key present in cache.

        Parameters
        ----------
        key : Union[Flow, ExtendedFlow, int]
            Key to be searched in the cache.

        Returns
        -------
        bool
            True if key found in cache, False otherwise.
        """

        return key in self._cache

    def items(self) -> ItemsView:
        """Function returns [key, value] pairs from cache.

        Returns
        -------
        ItemsView[dict_items]
            All [key, value] pairs from cache.
        """

        return self._cache.items()

    def values(self) -> ValuesView:
        """Function returns all values from cache.

        Returns
        -------
        ValuesView[dict_values]
            All values from cache.
        """

        return self._cache.values()

    def get(self, key: Union[Flow, ExtendedFlow, int]) -> Union[Flow, ExtendedFlow]:
        """Function return item with correspondent key from cache.

        Parameters
        ----------
        key : Union[Flow, ExtendedFlow, int]
            Key to be searched in the cache.

        Returns
        -------
        Union[Flow, ExtendedFlow]
            Value of element with correspondent key.

        Raises
        ------
        KeyError
            Raised when key is not found in cache.
        """

        if key not in self._cache:
            raise KeyError("FlowCache::__getitem__(): Key not found.")
        return self._cache[key]

    def add(self, flow: Union[Flow, ExtendedFlow], key: Optional[int] = -1):
        """Functions adds new element to cache.

        Parameters
        ----------
        flow : Union[Flow, ExtendedFlow]
            Value to add to cache.
        key : Optional[int], optional
            Key to insert element, by default hash(flow).

        Raises
        ------
        ValueError
            Raised when incorrect type of argument.
        KeyError
            Raised when key already present in cache.
        """

        if not isinstance(flow, Flow) and not isinstance(flow, ExtendedFlow):
            raise ValueError("FlowCache::add(): Incorrect type of argument.")

        if key == -1:
            key = flow

        if key not in self:
            self._cache[key] = flow
        else:
            raise KeyError("FlowCache::add(): Unable to add element, key already in FlowCache.")

    def update(self, flow: ExtendedFlow, key: Optional[int] = -1):
        """Functions adds or updates element.
        Element must support aggregate function, used to update element.

        Parameters
        ----------
        flow : ExtendedFlow
            Value to add or update in cache.
        key : Optional[int], optional
            Key of element to be added or updated, by default hash(flow).

        Raises
        ------
        ValueError
            Raised when incorrect type of argument.
        """

        if not isinstance(flow, ExtendedFlow):
            raise ValueError("FlowCache::update(): Incorrect type of argument.")

        if key == -1:
            key = flow

        if key not in self._cache:
            self._cache[key] = flow
        else:
            self._cache[key].aggregate(flow)

    def pop(self, key: Union[Flow, ExtendedFlow, int]) -> Union[Flow, ExtendedFlow]:
        """Function removes and returns element with correspondent key from cache.

        Parameters
        ----------
        key : Union[Flow, ExtendedFlow, int]
            Key of element to be removed and returned.

        Returns
        -------
        Union[Flow, ExtendedFlow]
            Value of element with correspondent key.

        Raises
        ------
        KeyError
            Raised when ket not found in cache.
        """

        if key not in self._cache:
            raise KeyError("FlowCache::pop(): Invalid key.")
        return self._cache.pop(key)

    def to_csv_profile(self, file: Path):
        """Function writes all flows from cache to csv file.
        File is formatted as profile file for ft-generator.

        Parameters
        ----------
        file : Path
            Path to file to write to.
        """

        file.touch(exist_ok=True)
        profile_header = [
            "START_TIME",
            "END_TIME",
            "L3_PROTO",
            "L4_PROTO",
            "SRC_PORT",
            "DST_PORT",
            "PACKETS",
            "BYTES",
            "PACKETS_REV",
            "BYTES_REV",
        ]

        with file.open("w") as stream:
            writer = csv.writer(stream)
            writer.writerow(profile_header)

            for value in self.values():
                writer.writerow(value.get_profile())
