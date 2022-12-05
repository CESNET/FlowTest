"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains the Normalizer class for creating normalized uni-flows or bi-flows from flow fields,
so that they can be used in evaluation models.
"""

import logging
from typing import Any, Dict, List, Optional, Tuple, Union

from ftanalyzer.fields import FieldDirection
from ftanalyzer.flow import Flow, ValidationFlow


class Normalizer:
    """Normalizer accepts a set of flow fields and normalizes them so that they can be used in evaluation models.

    Normalization process consists of:
        Removing unknown fields from the provided flow fields.
        Converting all known fields into their expected type (recursively for subfields).
        Determining whether provided set of fields contains uni-flow or bi-flow data.
        Separation of provided fields into 2 subsets, each for one of the directions.
        Flow objects (resp. ValidationFlow objects) are created from the normalized fields.

    Attributes
    ----------
    _field_db : FieldDatabase
        Fields database object.
    _fwd_key_fmt : tuple
        Flow key format for flows in forward direction.
    _rev_key_fmt : tuple
        Flow key format for flows in reverse direction.
    _fwd_fields : list
        Flow fields which may be present only in forward direction.
    _rev_fields : list
        Flow fields which may be present only in reverse direction.
    """

    def __init__(self, field_db: "FieldDatabase") -> None:
        """Basic init.

        Parameters
        ----------
        field_db : FieldDatabase
            Fields database object.
        """

        self._field_db = field_db
        self._fwd_fields = self._field_db.get_fields_in_direction(FieldDirection.FORWARD)
        self._rev_fields = self._field_db.get_fields_in_direction(FieldDirection.REVERSE)

        # No error expected here, because the keys are already initialized in the database object.
        self._fwd_key_fmt, self._rev_key_fmt = self._field_db.get_key_formats()

    def set_key_fmt(self, key_fmt: List[str]) -> None:
        """Set forward key format, overriding the default one from field database.
        Reverse key format is created automatically.

        Parameters
        ----------
        key_fmt : list
            Names of flow fields which specify the flow key.

        Raises
        ------
        KeyError
            One or more field names in the key format are not present in the field database.
        """

        self._fwd_key_fmt, self._rev_key_fmt = self._field_db.get_key_formats(key_fmt)

    def normalize(self, flows: List[Dict[str, Any]]) -> List[Tuple[Flow, Optional[Flow]]]:
        """Create pairs of Flow objects (forward and reverse) from provided sets of flow fields.

        Parameters
        ----------
        flows : List[Dict[str, Any]]
            List of flows described by their flow fields.

        Returns
        ------
        List[Tuple[Flow, Optional[Flow]]]
            Pairs of Flow objects (forward and reverse) for each set of flow fields.
            The reverse Flow object can be None if reverse fields were not detected.
            Format: [(Flow, Flow), (Flow, None), ...]
        """

        return [self._normalize_single(flow, False) for flow in flows]

    def normalize_validation(
        self, flows: List[Dict[str, Any]]
    ) -> List[Tuple[ValidationFlow, Optional[ValidationFlow]]]:
        """Create pairs of ValidationFlow objects (forward and reverse) from provided sets of flow fields.

        Parameters
        ----------
        flows : List[Dict[str, Any]]
            List of flows described by their flow fields.

        Returns
        ------
        List[Tuple[ValidationFlow, Optional[ValidationFlow]]]
            Pairs of ValidationFlow objects (forward and reverse) for each set of flow fields.
            The reverse ValidationFlow object can be None if reverse fields were not detected.
            Format: [(ValidationFlow, ValidationFlow), (ValidationFlow, None), ...]
        """

        return self._normalize_noexcept(flows, True)

    def _normalize_noexcept(
        self, flows: List[Dict[str, Any]], validation_flag: bool
    ) -> List[Tuple[Union[Flow, ValidationFlow], Union[Flow, ValidationFlow, None]]]:
        """Create objects from provided flow fields. Skip objects which could not be created.

        Parameters
        ----------
        flows : List[Dict[str, Any]]
            List of flows described by their flow fields.
        validation_flag: bool
            Flag indicating whether ValidationFlow should be created instead of Flow.

        Returns
        ------
        List[Tuple[Union[Flow, ValidationFlow], Union[Flow, ValidationFlow, None]]]
            Flow objects for each set of flow fields.
            Format: [(Flow, Flow), (Flow, None), ...]
        """

        ret = []
        for flow in flows:
            try:
                ret.append(self._normalize_single(flow, validation_flag))
            except KeyError as err:
                logging.getLogger().error("unable to create object from flow fields, error=%s", str(err))

        return ret

    def _normalize_single(
        self, fields: Dict[str, Any], validation_flag: bool
    ) -> Tuple[Union[Flow, ValidationFlow], Union[Flow, ValidationFlow, None]]:
        """Normalize provided flow fields and create Flow or ValidationFlow object from provided flow fields.
        May return 2 objects, if provided flow fields describe a bi-flow.

        Parameters
        ----------
        fields : Dict[str, Any]
            Flow fields.
        validation_flag: bool
            Flag indicating whether ValidationFlow should be created instead of Flow.

        Returns
        ------
        Tuple[Union[Flow, ValidationFlow], Union[Flow, ValidationFlow, None]]
            Forward flow object and reverse flow object (if fields describe bi-flow).

        Raises
        ------
        KeyError
            Unable to create Flow object due to a missing key field.
        """
        norm_fields = self._normalize_fields(fields)
        biflow = self._is_biflow(norm_fields)

        rev_flow = None
        fwd_flow = self._build_flow(FieldDirection.FORWARD, norm_fields, validation_flag)
        if biflow:
            rev_flow = self._build_flow(FieldDirection.REVERSE, norm_fields, validation_flag)

        return fwd_flow, rev_flow

    def _build_flow(
        self,
        direction: FieldDirection,
        fields: Dict[str, Union[str, int, Dict, List]],
        validation_flag: bool,
    ) -> Union[Flow, ValidationFlow]:
        """Create Flow or ValidationFlow object from provided normalized flow fields.

        Parameters
        ----------
        direction : FieldDirection.FORWARD, FieldDirection.REVERSE
            Forward or reverse. Indicates which fields should be selected to create the flow object.
        fields : Dict[str, Union[str, int, Dict, List]]
            Normalized flow fields.
        validation_flag: bool
            Flag indicating whether ValidationFlow should be created instead of Flow.

        Returns
        ------
        Flow, ValidationFlow
            Forward flow object or reverse flow object.

        Raises
        ------
        KeyError
            Unable to create Flow object due to a missing key field.
        """
        uni_fields = self._filter_fields_by_direction(direction, fields)

        if validation_flag:
            return ValidationFlow(
                self._fwd_key_fmt, self._rev_key_fmt, uni_fields, self._field_db, direction == FieldDirection.REVERSE
            )

        return Flow(self._fwd_key_fmt, self._rev_key_fmt, uni_fields, direction == FieldDirection.REVERSE)

    def _filter_fields_by_direction(
        self,
        direction: FieldDirection,
        fields: Dict[str, Union[str, int, Dict, List]],
    ) -> Dict[str, Union[str, int, Dict, List]]:
        """Filter provided fields based on the specified direction.
        Names of fields in reverse direction are automatically mapped
        to their forward counterparts (packets@rev -> packets, ..).

        Parameters
        ----------
        direction : FieldDirection.FORWARD, FieldDirection.REVERSE
            Forward or reverse. Indicates which fields should be selected to create the flow object.
        fields : Dict[str, Union[str, int, Dict, List]]
            Normalized flow fields.

        Returns
        ------
        Dict[str, Union[str, int, Dict, List]]
            Fields filtered based on specified direction.
        """

        excluded = self._rev_fields if direction == FieldDirection.FORWARD else self._fwd_fields
        filtered = {}

        # Remove fields fixed for this direction to be later merged with the original fields.
        fixed_fields = fields.pop(f"_{direction.value}", {})

        for f_name, f_value in fields.items():
            # Skip meta fields and fields in the opposite direction.
            if f_name.startswith("_") or f_name in excluded:
                continue

            # Transform the field name if the field has its reverse counterpart, but use original values.
            if (
                direction == FieldDirection.REVERSE
                and self._field_db.get_field_direction(f_name) != FieldDirection.ALWAYS
            ):
                f_name = self._field_db.get_rev_field_name(f_name)

            filtered[f_name] = f_value

        return {**filtered, **fixed_fields}

    def _is_biflow(self, fields: Dict[str, Union[str, int, Dict, List]]) -> bool:
        """Check if the flow fields contain any non-empty reverse fields.

        Parameters
        ----------
        fields : Dict[str, Union[str, int, Dict, List]]
            Normalized flow fields.

        Returns
        ------
        bool
            True - non-empty reverse fields present, False otherwise
        """

        for f_name, f_value in fields.items():
            if f_name.startswith("_"):
                continue

            if self._field_db.get_field_direction(f_name) == FieldDirection.REVERSE and f_value:
                return True

        return False

    def _normalize_fields(self, fields: Dict[str, Any]) -> Dict[str, Union[str, int, Dict, List]]:
        """Iterate over provided flow fields, filter out unknown fields and convert known fields to their expected type.

        Parameters
        ----------
        fields : Dict[str, Any]
            Flow fields to be normalized.

        Returns
        ------
        Dict[str, Union[str, int, Dict, List]]
            Normalized flow fields.
        """

        normalized_fields = {}
        for f_name, f_value in fields.items():
            new_value = self._parse_field(f_name, f_value)
            if new_value is not None:
                normalized_fields[f_name] = new_value
            else:
                logging.getLogger().warning("unknown or empty field=%s, skipping...", f_name)

        return normalized_fields

    def _parse_field(self, name: str, value: Any) -> Union[str, int, Dict, List, None]:
        """Recursively parse flow field and convert it to its expected type.

        Parameters
        ----------
        name : str
            Flow field name.
        value : str, int, dict, list
            Flow field value.

        Returns
        ------
        Union[str, int, Dict, List, None]
            Flow field value. None is returned if the field is not present in the flow database.
        """

        if not self._field_db.exists(name) and not name.startswith("_"):
            return None

        if isinstance(value, list):
            ret = []
            for item in value:
                new_value = self._parse_field(name, item)
                if new_value is not None:
                    if isinstance(new_value, list):
                        ret += new_value
                    else:
                        ret.append(new_value)

            return ret if len(ret) > 0 else None

        if isinstance(value, dict):
            ret = {}
            for f_name, f_value in value.items():
                extended_name = f"{name}.{f_name}"
                if name.startswith("_"):
                    # Special do not create prefix.
                    extended_name = f_name

                new_value = self._parse_field(extended_name, f_value)
                if new_value is not None:
                    ret[extended_name] = new_value

            return self._normalize_field_type(name, ret) if len(ret) > 0 else None

        return self._normalize_field_type(name, value)

    def _normalize_field_type(self, f_name, f_value: Any) -> Union[str, int, Dict, List, None]:
        """Convert a flow field to its expected type, Supported types: str, int, Dict, List.

        Parameters
        ----------
        f_name : str
            Flow field name.
        f_value : str, int, dict, list
            Flow field value.

        Returns
        ------
        Union[str, int, Dict, List, None]
            Converted flow field, None if the conversion failed.
        """

        # Skip special directives.
        if not self._field_db.exists(f_name) and f_name.startswith("_"):
            return f_value

        try:
            f_type = self._field_db.get_field_type(f_name)
            if f_type == "string":
                return str(f_value)

            if f_type == "int":
                return int(f_value)

            if f_type == "list":
                return f_value if isinstance(f_value, list) else [f_value]

            if f_type == "dict":
                if not isinstance(f_value, dict):
                    raise TypeError(f"Field '{f_name}' is not a dictionary, but {type(f_value)}.")

                return f_value

            raise TypeError(f"Field '{f_name}' unsupported type {f_type}.")
        except TypeError as err:
            logging.getLogger().warning("unable to convert field=%s to designated type, error=%s", f_name, str(err))
            return None
