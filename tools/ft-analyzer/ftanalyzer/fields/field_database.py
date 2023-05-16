"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Database of flow fields created from a yaml configuration file.
"""

import logging
from enum import Enum
from typing import Dict, List, Tuple, Union

import yaml


class FieldDirection(Enum):
    """Flow field direction enum."""

    ALWAYS = "always"
    ANY = "any"
    FORWARD = "forward"
    REVERSE = "reverse"


class FieldDatabase:
    """Database of flow fields created from a yaml configuration file.

    Expected configuration file structure:
        key: <list of fields which identify a flow>
        fields:
            - <name of the field>:
                type:      <string/int/list/dict>
                direction: <always/any/forward/reverse>
                reverse:   <name of the reverse field>
                subfields: <recursive list of fields>

    Example of expansion of reverse fields:
    src_ip:                     dst_ip:
        type:      string           type:      string
        direction: always    =>     direction: always
        reverse:   dst_ip           reverse:   src_ip

    Example of expansion of subfields:
    dns_resp_rr:                        dns_resp_rr:
        type: list                          type: list
        direction: always                   direction: always
        subfields:                 =>   dns_resp_rr.name:
            name:                           type:      string
                type:      string           direction: always
                direction: always       dns_resp_rr.type:
            type:                           type:      int
                type:      int              direction: always
                direction: always

    Attributes
    ----------
    _fields : dict
        All fields present in the configuration yaml file, including expanded subfields and reverse fields.
    _fwd_key_fmt : tuple
        Field names which create the flow key format.
    _rev_key_fmt : tuple
        Field names which create the reverse flow key format.
    """

    DIR_MAP = {"always": "always", "any": "any", "reverse": "forward", "forward": "reverse"}
    SUPPORTED_TYPES = ["int", "string", "list", "dict"]

    def __init__(self, fields_file: str) -> None:
        """Read flow fields specification file and expand its content.
        Establish flow key and reverse flow key formats.

        Parameters
        ----------
        fields_file : str
            Path to the file containing flow fields specification.

        Raises
        ------
        ValueError
            Unable to open the flow fields specification file.
            Bad format of the flow fields specification file.
            Unable to create reverse flow key format.
        """
        try:
            with open(fields_file, "r", encoding="ascii") as stream:
                data = yaml.safe_load(stream)
                self._fields = self._expand_fields(data["fields"])
                self._fwd_key_fmt, self._rev_key_fmt = self._create_key_formats(data["key"])
        except (IOError, yaml.YAMLError, ValueError, KeyError) as err:
            logging.getLogger().error("unable to process fields database file=%s, reason=%s", fields_file, str(err))
            raise ValueError(f"Unable to process fields database file={fields_file}.") from err

    def _expand_fields(self, fields: Dict[str, Dict], prefix: str = "") -> Dict[str, Dict[str, str]]:
        """Expand reverse fields and subfields present in the flow fields specification.
        Also perform basic format validation. Can be invoked recursively.

        Parameters
        ----------
        fields : Dict[str, Dict]
            Flow fields specification.
        prefix : str
            Flow field name prefix which indicates, that the flow field is actually a subfield.

        Returns
        -------
        Dict[str, Dict[str, str]]
            Flow fields database.

        Raises
        ------
        ValueError
            Bad format of the flow fields specification file.
        """
        expanded_fields = {}
        for f_name, f_value in fields.items():
            if "subfields" in f_value:
                subfields = self._expand_fields(f_value["subfields"], prefix=f"{f_name}.")
                expanded_fields.update(subfields)
                del f_value["subfields"]

            if f_value["direction"] not in self.DIR_MAP:
                logging.getLogger().error("invalid 'direction' value=%s of field=%s", f_value["direction"], f_name)
                raise ValueError(f"Invalid 'direction' value={f_value['direction']} of field={f_name}.")

            if f_value["type"] not in self.SUPPORTED_TYPES:
                logging.getLogger().error("invalid 'type' value=%s of field=%s", f_value["type"], f_name)
                raise ValueError(f"Invalid 'type' value={f_value['type']} of field={f_name}.")

            expanded_fields[prefix + f_name] = f_value
            if "reverse" in f_value:
                expanded_fields[f_value["reverse"]] = {
                    "type": f_value["type"],
                    "direction": self.DIR_MAP[f_value["direction"]],
                    "reverse": f_name,
                }

        return expanded_fields

    def _create_key_formats(self, key_fmt: List[str]) -> Tuple[Tuple[str, ...], Tuple[str, ...]]:
        """Create regular and reverse flow key formats.

        Parameters
        ----------
        key_fmt : list
            Flow fields which should create the regular flow key format.

        Returns
        -------
        tuple(tuple(str, ...), tuple(str, ...))
            Regular and reverse flow key formats.

        Raises
        ------
        KeyError
            One or more field names in the key format are not present in the field database.
        """
        fwd_key = tuple(key_fmt)
        rev_key = tuple(map(lambda k: self._fields[k].get("reverse", k), fwd_key))

        return fwd_key, rev_key

    def get_key_formats(
        self, key: Union[List[str], Tuple[str], None] = None
    ) -> Tuple[Tuple[str, ...], Tuple[str, ...]]:
        """Get regular or reverse flow key format.

        Parameters
        ----------
        key : list[str], tuple(str), None
            Names of flow fields which specify the flow key.
            If None, default flow key format is going to be used (from flow fields specification file).

        Returns
        -------
        tuple(tuple(str, ...), tuple(str, ...))
            Regular or reverse flow key format.

        Raises
        ------
        KeyError
            One or more field names in the key format are not present in the field database.
        """
        if key is None:
            return self._fwd_key_fmt, self._rev_key_fmt

        return self._create_key_formats(key)

    def exists(self, name: str) -> bool:
        """Get information whether flow field exists in the database.

        Parameters
        ----------
        name : str
            Field name.

        Returns
        -------
        bool
            True - exists, False - does not exist
        """

        return name in self._fields

    def get_field_type(self, name: str) -> str:
        """Get a flow field type.

        Parameters
        ----------
        name : str
            Field name.

        Returns
        -------
        str
            Field type.

        Raises
        ------
        KeyError
            Field does not exist in the database.
        """

        return self._fields[name]["type"]

    def get_field_direction(self, name: str) -> FieldDirection:
        """Get a flow field direction.

        Parameters
        ----------
        name : str
            Field name.

        Returns
        -------
        FieldDirection enum
            Field direction.

        Raises
        ------
        KeyError
            Field does not exist in the database.
        """

        return FieldDirection[self._fields[name]["direction"].upper()]

    def get_rev_field_name(self, name: str) -> str:
        """Get reverse name of a flow field.
        If the field does not have a reverse field counterpart, return the name of the field.

        Parameters
        ----------
        name : str
            Field name.

        Returns
        -------
        str
            Reverse field name, or the same name if the flow does not have its reverse counterpart.

        Raises
        ------
        KeyError
            Field does not exist in the database.
        """

        return self._fields[name].get("reverse", name)

    def get_fields_in_direction(
        self,
        direction: FieldDirection,
    ) -> List[str]:
        """Get all field names with the specified direction.

        Parameters
        ----------
        direction : FieldDirection enum
            Direction.

        Returns
        -------
        list(str)
            Field names of fields with the specified direction.
        """

        return [k for k, v in self._fields.items() if v["direction"] == direction.value]
