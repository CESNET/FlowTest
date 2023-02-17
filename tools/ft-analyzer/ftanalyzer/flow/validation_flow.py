"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains ValidationFlow class which is used as an input to Validation model.
"""

from itertools import permutations
from typing import Tuple, Union, Dict, List, Optional, Any

from ftanalyzer.fields import FieldDirection
from ftanalyzer.flow.flow import Flow
from ftanalyzer.flow.validation_result import ValidationResult


# pylint: disable=R0903
class ValidationFlow(Flow):
    """ValidationFlow inherits from Flow class and acts as a reference flow.
    Its purpose is to compare its fields with other flows and find differences.

    Attributes
    ----------
    _any_dir_fields : list
        Names of present flow fields which have direction value "any".
    """

    __slots__ = ("_any_dir_fields",)

    def __init__(
        self,
        key_fmt: Tuple[str, ...],
        rev_key_fmt: Tuple[str, ...],
        fields: Dict[str, Union[str, int, Dict, List]],
        fields_db: "FieldDatabase",
    ) -> None:
        """Initialize Flow object by creating its keys from the provided normalized flow fields.

        Parameters
        ----------
        key_fmt : tuple
            Names of flow fields which create the flow key.
        rev_key_fmt : tuple
            Names of flow fields which create the reverse flow key.
        fields : Dict[str, Union[str, int, Dict, List]]
            Flow fields in format "name: value".
        fields_db : FieldDatabase
            Fields database object.

        Raises
        ------
        ValueError
            Key field not present among flow fields.
        """

        super().__init__(key_fmt, rev_key_fmt, fields)
        self._any_dir_fields = [
            field for field in fields_db.get_fields_in_direction(FieldDirection.ANY) if field in self.fields
        ]

    def validate(
        self, flow: Flow, rev_flow: Optional[Flow], supported: List[str], special: Dict[str, str]
    ) -> ValidationResult:
        """Compare fields of this reference flow with fields of provided Flow object.

        Only fields present in the 'supported' parameter are compared.
        If a field is not present in the compared Flow object and has direction value "any",
        the reverse Flow object (if provided) is also checked for its presence.

        Parameters
        ----------
        flow : Flow
            Flow object to be compared with.
        rev_flow : Flow, None
            Flow object from the opposite direction (second part of the biflow).
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        ValidationResult
            Object containing comparison statistics and reported errors.
        """
        ret = ValidationResult()
        expected = {k: v for k, v in self.fields.items() if k in supported and k not in self.key_fmt}
        for f_exp_name, f_exp_value in expected.items():
            if f_exp_name not in flow.fields:
                # Flows with direction "any" can be in the flow from the opposite direction.
                if f_exp_name in self._any_dir_fields and rev_flow is not None and f_exp_name in rev_flow.fields:
                    f_value = rev_flow.fields[f_exp_name]
                else:
                    ret.report_missing_field(f_exp_name, f_exp_value)
                    continue
            else:
                f_value = flow.fields[f_exp_name]

            result = self._validate_field(f_exp_name, f_value, f_exp_value, supported, special)
            ret.update(result)

        return ret

    def _validate_field(
        self,
        name: str,
        value: Union[str, int, Dict, List],
        ref: Union[str, int, Dict, List],
        supported: List[str],
        special: Dict[str, str],
    ) -> ValidationResult:
        """Compare flow field with its reference field. Fields of type list or dictionary are compared recursively.

        Parameters
        ----------
        name : str
            Name of the field to be compared.
        value : str, int, dict, list
            Value of the field.
        ref : str, int, dict, list
            Reference value of the field.
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        ValidationResult
            Object containing comparison statistics and reported errors.
        """

        ret = ValidationResult()

        if isinstance(value, list):
            result = self._validate_list(name, value, ref, supported, special)
            ret.update(result)
        elif isinstance(value, dict):
            result = self._validate_dict(value, ref, supported, special)
            ret.update(result)
        elif (isinstance(ref, list) and value in ref) or ref == value:
            ret.report_correct_field(name)
        else:
            ret.report_wrong_value_field(name, value, ref)

        return ret

    def _validate_list(
        self, name: str, values: List[Any], references: List[Any], supported: List[str], special: Dict[str, str]
    ) -> ValidationResult:
        """Compare flow field of type list with its reference field. All fields in the list are compared recursively.

        Parameters
        ----------
        name : str
            Name of the field to be compared.
        values : list
            Flow subfields.
        references : list
            Reference subfields.
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        ValidationResult
            Object containing comparison statistics and reported errors.
        """

        results = {}
        strategy = special.get(name, "FullArray")
        best_result = None

        # Precompute all possible results and store them into a map.
        for val_index, val in enumerate(values):
            results[val_index] = {}
            for ref_index, ref in enumerate(references):
                result = self._validate_field(name, val, ref, supported, special)

                results[val_index].update({ref_index: result})
                if strategy == "OneInArray":
                    if result.score() == 0:
                        return result

                    if not best_result or best_result.score() > result.score():
                        best_result = result

        if strategy == "OneInArray":
            return best_result

        # Find the best possible mapping of fields to the reference fields.
        best_perm = self._find_best_mapping(len(values), len(references), results)

        return self._validate_list_full_result(name, values, references, best_perm, results)

    @staticmethod
    def _validate_list_full_result(
        name: str,
        values: List[Any],
        references: List[Any],
        mapping: List[int],
        results: Dict[int, Dict[int, ValidationResult]],
    ) -> ValidationResult:
        """Get validation result for all fields in the list. Report missing, wrong and unexpected fields.

        Parameters
        ----------
        name : str
            Name of the field to be compared.
        values : list
            Flow subfields.
        references : list
            Reference subfields.
        mapping : list
            Sequence of flows which has the least error score.
        results : Dict[int, Dict[int, ValidationResult]]
            Map of precomputed results in format: "{flows number: {reference flow number: result}}"

        Returns
        ------
        ValidationResult
            Object containing comparison statistics and reported errors.
        """

        ret = ValidationResult()

        # Reduce the results to a single result and report missing fields.
        for ref_index, val_index in enumerate(mapping):
            if val_index == -1:
                for f_name, f_value in references[ref_index].items():
                    ret.report_missing_field(f_name, f_value)

                continue

            ret.update(results[val_index][ref_index])

        # If the flow list size is greater than the reference list size, it is necessary to report unexpected fields.
        for val_index, value in enumerate(values):
            if val_index not in mapping:
                ret.report_unexpected_field(name, value)

        return ret

    def _validate_dict(
        self, values: Dict[str, Any], references: Dict[str, Any], supported: List[str], special: Dict[str, str]
    ) -> ValidationResult:
        """Compare flow field of type dictionary with its reference field.
        All fields in the dictionary are compared recursively.

        Parameters
        ----------
        values : dict
            Flow subfields.
        references : dict
            Reference subfields.
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        ValidationResult
            Object containing comparison statistics and reported errors.
        """

        ret = ValidationResult()
        for f_exp_name, f_exp_value in references.items():
            if f_exp_name not in values:
                if f_exp_name in supported:
                    ret.report_missing_field(f_exp_name, f_exp_value)

                continue

            result = self._validate_field(f_exp_name, values[f_exp_name], f_exp_value, supported, special)
            ret.update(result)

        return ret

    @staticmethod
    def _find_best_mapping(flow_cnt: int, ref_cnt: int, results: Dict[int, Dict[int, ValidationResult]]) -> List[int]:
        """Find the best possible mapping of a list of evaluated field groups to
        a list of reference field groups based on precomputed results.

        Error score is computed for every possible mapping.
        If there are fewer fields than reference field groups, value -1 is written to all positions
        of the resulting permutation where a field group was not assigned.

        Parameters
        ----------
        flow_cnt : int
            Number of flows.
        ref_cnt : Flow, None
            Number of reference flows.
        results : Dict[int, Dict[int, ValidationResult]]
            Map of precomputed results in format: "{flows number: {reference flow number: result}}"

        Returns
        ------
        list
            Sequence of indexes to the list of evaluated field groups which has the least error score.
            The size of the sequence is the number of reference field groups.
            Values -1 in the sequence indicate that the reference field groups has no evaluated field group assigned.
        """

        candidates = list(range(flow_cnt))
        if flow_cnt < ref_cnt:
            # Extend the list of candidates to match the number of references.
            candidates += [-1] * (ref_cnt - flow_cnt)

        perms = set(permutations(candidates, ref_cnt))
        best_perm = None
        best_score = 0
        for perm in perms:
            score = 0
            for ref_index, flow_index in enumerate(perm):
                if flow_index == -1:
                    continue

                score += results[flow_index][ref_index].score()

            if best_perm is None or score < best_score:
                best_score = score
                best_perm = perm

        return best_perm
