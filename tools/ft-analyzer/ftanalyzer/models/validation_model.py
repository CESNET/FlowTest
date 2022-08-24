"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains ValidationModel class for performing accurate comparison between lists of flows.
"""

from copy import deepcopy
from typing import List, Dict, Optional, Tuple

from ftanalyzer.reports import ValidationModelReport
from ftanalyzer.models.biflow_validator import BiflowValidator


# pylint: disable=R0903
class ValidationModel:
    """Validation model accepts lists of flows and reference flows. It tries to find the best mapping
    of provided flows to the reference flows. All differences in the resulting report should be treated as errors.

    The purpose of this model is to determine whether there are differences in the individual fields,
    so it is purpose is mainly for checking the quality of fields exported by an IPFIX probe.
    Due to the time complexity of the mapping strategy, it is advised to avoid scenarios where a lot of flows
    with the same key (including reverse direction) are present.

    Attributes
    ----------
    _key_fmt : tuple
        Names of flow fields which create the flow key.
    _ref_matrix : Dict[Tuple[str, ...], BiflowValidator]
        Validation matrix where provided reference flows are separated into buckets of bi-flows.
    """

    def __init__(
        self, key_fmt: Tuple[str, ...], references: List[Tuple["ValidationFlow", Optional["ValidationFlow"]]]
    ) -> None:
        """Initializes the model by creating a matrix of reference.

        Parameters
        ----------
        key_fmt : tuple
            Names of flow fields which create the flow key.
        references : list
            Pairs of ValidationFlow and reverse ValidationFlow objects.
        """

        self._key_fmt = key_fmt
        self._ref_matrix = {}

        for fwd_flow, rev_flow in references:
            access_key = self._get_matrix_access_key(fwd_flow.key, fwd_flow.rev_key)
            if access_key is None:
                access_key = fwd_flow.key
                self._ref_matrix[access_key] = BiflowValidator(fwd_flow.key, fwd_flow.rev_key)

            self._ref_matrix[access_key].add_flows(fwd_flow, rev_flow)

    def validate(
        self, flows: List[Tuple["Flow", Optional["Flow"]]], supported: List[str], special: Dict[str, str]
    ) -> ValidationModelReport:
        """Perform the validation of provided flows against the initialized model.

        Parameters
        ----------
        flows : list
            Pairs of Flow and reverse Flow objects.
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        ValidationModelReport
            Validation report.
        """
        report = ValidationModelReport(self._key_fmt)
        ref_matrix = deepcopy(self._ref_matrix)

        for fwd_flow, rev_flow in flows:
            access_key = self._get_matrix_access_key(fwd_flow.key, fwd_flow.rev_key)
            if access_key is None:
                report.add_validation_result(fwd_flow, None, None)
                if rev_flow is not None:
                    report.add_validation_result(rev_flow, None, None)

                continue

            ref_matrix[access_key].add_flows(fwd_flow, rev_flow)

        for mtrx in ref_matrix.values():
            results = mtrx.validate(supported, special)
            for flow, ref, res in results:
                report.add_validation_result(flow, ref, res)

        return report

    def _get_matrix_access_key(self, fwd_key: Tuple[str, ...], rev_key: Tuple[str, ...]) -> Optional[Tuple[str, ...]]:
        """Determine which key should be used to access the reference matrix.

        Parameters
        ----------
        fwd_key : tuple
            Flow key.
        rev_key : tuple
            Flow key for the opposite direction.

        Returns
        ------
        tuple, None
            Key that should be used to access the bi-flow matrix.
            Returns None if neither forward key nor reverse key exists in the reference matrix.
        """
        if fwd_key in self._ref_matrix:
            return fwd_key

        if rev_key in self._ref_matrix:
            return rev_key

        return None
