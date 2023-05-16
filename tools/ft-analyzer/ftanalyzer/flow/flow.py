"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains Flow class.
"""

from typing import Dict, List, Tuple, Union

FieldsDict = Dict[str, Union[str, int, Dict, List]]


class Flow:
    """Flow object containing flow fields identifiable by its "key" - list of specific flow fields.

    Attributes
    ----------
    key_fmt : tuple
        Names of flow fields which create the flow key.
    key : tuple
        Flow key.
    rev_key : tuple
        Key that could be expected in flows in the opposite direction.
    fields : FieldsDict
        Flow fields in format "name: value".
    biflow : bool
        Flag indicating whether the flow originates from a biflow.
    """

    __slots__ = ("key_fmt", "key", "rev_key", "fields", "biflow")

    def __init__(
        self,
        key_fmt: Tuple[str, ...],
        rev_key_fmt: Tuple[str, ...],
        fields: FieldsDict,
        biflow: bool,
    ) -> None:
        """Initialize Flow object by creating its keys from the provided normalized flow fields.

        Parameters
        ----------
        key_fmt : tuple
            Names of flow fields which create the flow key.
        rev_key_fmt : tuple
            Names of flow fields which create the reverse flow key.
        fields : FieldsDict
            Flow fields in format "name: value".
        biflow : bool
            Flag indicating whether the flow originates from a biflow.

        Raises
        ------
        KeyError
            Key field not present among flow fields.
        """
        self.key_fmt = key_fmt
        self.fields = fields
        self.biflow = biflow

        self.key = self._parse_key(key_fmt)
        self.rev_key = self._parse_key(rev_key_fmt)

    def __eq__(self, flow: "Flow") -> bool:
        """Compare 2 flows. Two flows are equal if their keys are equal.

        Parameters
        ----------
        flow : Flow
            Flow to be compared with.

        Returns
        ------
        bool
            True if flows are equal, False otherwise.
        """
        return self.key == flow.key

    def get_non_key_fields(self) -> FieldsDict:
        """Get all flow fields which are not part of the key.

        Returns
        ------
        FieldsDict
            Flow fields in format "name: value".
        """
        return {f_name: f_value for f_name, f_value in self.fields.items() if f_name not in self.key_fmt}

    def _parse_key(self, key_fmt: Tuple[str, ...]) -> Tuple[str, ...]:
        """Parse flow key from flow fields based on the key format.

        Parameters
        ----------
        key_fmt : tuple
            Key format.

        Returns
        ------
        tuple
            Parsed flow key.

        Raises
        ------
        ValueError
            Key field not present among flow fields.
        """
        for key_field_name in key_fmt:
            if key_field_name not in self.fields:
                raise KeyError(f"Key field '{key_field_name}' not present among flow fields.")

        return tuple(map(lambda name: self.fields[name], key_fmt))
