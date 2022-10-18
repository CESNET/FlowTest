"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Mapping JSON output of ipfixcol2 (ipfix attributes) to format expected by validation tests (YAML test description).
"""
import string
from typing import Tuple, Any
import yaml

from src.collector.interface import CollectorOutputReaderInterface
from src.collector.protocols import known_protocols


class MappingException(Exception):
    """Basic exception raised by collector output mapper."""


class MappingFileReadException(Exception):
    """Error reading mapping configuration file."""


class MappingFormatException(Exception):
    """Error in mapping configuration file."""


class Converters:
    """Class with methods used as ipfix value converters."""

    @staticmethod
    def flags_to_hex(flags: str) -> int:
        """Convert flags in string form ('.AP.S.') to number (hexadecimal in annotation)."""
        res = 0
        for i, flag in enumerate(reversed(flags)):
            if flag != ".":
                res += 2**i
        return res

    @staticmethod
    def protocol_identifier_to_number(identifier: str) -> int:
        """Convert protocol identifier to protocol number (IANA). E.g. 'TCP' -> 6."""
        if identifier in known_protocols:
            return known_protocols.index(identifier)
        raise MappingException(f"Unknown protocol with identifier '{identifier}'.")

    @staticmethod
    def rstrip_zeroes(raw_str: str) -> str:
        """Strip all zeroes from the right of byte array or hexadecimal string."""
        if raw_str.startswith("0x") and all(c in string.hexdigits for c in raw_str[2:]):
            # string is hexa number
            return raw_str.rstrip("0")
        # string is byte array
        return raw_str.rstrip("\x00")


class CollectorOutputMapper:
    """Wrapper of CollectorOutputReaderInterface. Flows are mapped from JSON ipfix to YAML annotation format.

    Attributes
    ----------
    _reader : CollectorOutputReaderInterface
        Object instance for iterating over flows.
    _mapping : dict
        Loaded attribute keys mapping from file.
    """

    def __init__(self, reader: CollectorOutputReaderInterface, mapping_filename: str):
        """Init mapper.

        Parameters
        ----------
        reader : CollectorOutputReaderInterface
            Object from which are JSON flows taken.
        mapping_filename : str
            Path to mapping configuration file.

        Raises
        ------
        MappingFileReadException
            Cannot open mapping configuration file.
        MappingFormatException
            Bad YAML format in mapping configuration file.
        """
        self._reader = reader
        try:
            with open(mapping_filename, "r", encoding="utf-8") as file:
                mapping = yaml.safe_load(file)
        except OSError as exc:
            raise MappingFileReadException(f"Cannot open mapping configuration file '{mapping_filename}'.") from exc
        except yaml.YAMLError as exc:
            raise MappingFormatException("Error while parsing mapping configuration file.") from exc

        self._check_yaml_format(mapping)
        self._mapping = mapping

    def __iter__(self) -> "CollectorOutputMapper":
        self._reader = iter(self._reader)
        return self

    def __next__(self) -> Tuple[dict, list, list]:
        """Get flow from collector reader and map to YAML annotation format.

        Returns
        -------
        flow
            Mapped flow.
        mapped_keys
            Names of attributes successfully mapped.
        unmapped_key
            Names of attributes with unknown mapping.

        Raises
        ------
        MappingException
            Converter method from config not found.
        """
        return self._map(next(self._reader))

    @staticmethod
    def _check_yaml_format(mapping: dict) -> None:
        """Check YAML configuration file format before start normalizing.

        Parameters
        ----------
        mapping : dict
            Loaded YAML configuration file.

        Raises
        ------
        MappingFormatException
            Bad YAML format.
        """
        for attrib, attrib_mapping in mapping.items():
            if "map" not in attrib_mapping:
                raise MappingFormatException(f"Missing map attribute for key '{attrib}'.")
            for key, value in attrib_mapping.items():
                if key in ["map", "converter"]:
                    if not isinstance(value, str):
                        raise MappingFormatException(
                            f"Unexpected type of value of '{key}' in property '{attrib}' mapping."
                        )
                    if key == "converter" and not hasattr(Converters, value):
                        raise MappingFormatException(
                            f"Converter method '{value}' from property '{attrib}' mapping not found."
                        )
                else:
                    raise MappingFormatException(
                        f"Unexpected key '{key}' in property '{attrib}' mapping. Allowed only 'map' and 'converter'."
                    )

    def _map(self, json_flow: dict) -> Tuple[dict, list, list]:
        """Map attributes of single flow.

        Parameters
        ----------
        json_flow : dict
            Raw JSON flow from collector.

        Returns
        -------
        flow
            Mapped flow.
        mapped_keys
            Names of attributes successfully mapped.
        unmapped_key
            Names of attributes with unknown mapping.

        Raises
        ------
        MappingException
            Converter method from config not found.
        """
        flow = {}
        mapped_keys = []
        unmapped_keys = []
        for key, value in json_flow.items():
            if key in self._mapping:
                mapping = self._mapping[key]
                mapped_keys.append(key)
            else:
                unmapped_keys.append(key)
                continue

            if "." not in mapping["map"]:
                # simple attribute
                flow[mapping["map"]] = self._map_value(mapping, value)
            else:
                # structured attribute
                key_parts = mapping["map"].split(".")
                struct_attribute = flow
                for nested_key in key_parts[:-1]:
                    if nested_key not in struct_attribute:
                        struct_attribute[nested_key] = {}
                    elif not isinstance(struct_attribute[nested_key], dict):
                        raise MappingException(f"Attribute must be either dict or simple value (key: '{nested_key}')")
                    struct_attribute = struct_attribute[nested_key]
                inner_key = key_parts[-1]
                struct_attribute[inner_key] = self._map_value(mapping, value)

        return flow, mapped_keys, unmapped_keys

    @staticmethod
    def _map_value(mapping: dict, value: Any) -> Any:
        """Convert value of ipfix attribute.

        Parameters
        ----------
        mapping : dict
            Mapping record for attribute from configuration file.
        value
            Raw value of attribute.

        Returns
        -------
        value
            Converted value or unchanged value if converter not specified.

        Raises
        ------
        MappingException
            Converter method from config not found.
        """
        if "converter" in mapping:
            converter_name = mapping["converter"]
            # converter method presence in Converters class is checked in _check_yaml_format
            converter = getattr(Converters, converter_name)
            return converter(value)
        return value
