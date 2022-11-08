"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains BiflowValidator class which is capable of validating flows
which have the same key including flows in the opposite direction.
The validation strategy is to find best possible mapping of flows to reference flows.
"""

from itertools import permutations, product
from typing import Dict, List, Optional, Tuple, Union

from ftanalyzer.flow import ValidationFlow


class BiflowValidator:
    """Biflow validator is capable of validating flows with the same key including flows in the opposite direction.

    The validator tries to map all provided flows to reference flows and finds the best possible combination.
    The best combination is the one that produces the least number of errors.
    Every flow validation is done in 2 flow pairs:
        - forward flow, forward flow reference, reverse flow, reverse flow reference

    Attributes
    ----------
    _flows : list
        Flow objects to be validated.
    _ref : list
        ValidationFlow objects acting as reference flows.
    _flow_map : Dict[Tuple[str, ...], Dict[str, List[int]]]
        Indexes of flows and reference flows separated by their key (direction).
    _flow_pairs : dict
        Indexes of flows that are known to be pairs (parts of the same bi-flow).
    _ref_pairs : dict
        Indexes of reference flows that are known to be pairs (parts of the same bi-flow).
    _ref_flow_indexes: list
        Sequence of indexes to reference flows which form the validation permutation.
    _results_cache : Dict[Tuple[int, ...], ValidationResult]
        Map for caching validation results.
    """

    def __init__(self, key: Tuple[str, ...], rev_key: Tuple[str, ...]) -> None:
        """Basic init.

        Parameters
        ----------
        key : tuple
            Flow key.
        rev_key : tuple
            Key that could be expected in flows from the opposite direction.
        """

        self._flows = []
        self._ref = []
        self._flow_map = {key: {"flows": [], "ref": []}, rev_key: {"flows": [], "ref": []}}
        self._flow_pairs = {}
        self._ref_pairs = {}
        self._ref_flow_indexes = []
        self._results_cache = {}

    def add_flows(self, fwd_flow: Union["Flow", ValidationFlow], rev_flow: Union["Flow", ValidationFlow, None]) -> None:
        """Add provided flows to the validator.

        Parameters
        ----------
        fwd_flow : Flow, ValidationFlow
            Flow object.
        rev_flow : Flow, ValidationFlow, None
            Flow object known to be part of the same bi-flow, just in the opposite direction.
            Can be None.
        """
        if isinstance(fwd_flow, ValidationFlow):
            self._store_flows(fwd_flow, rev_flow, self._ref, self._ref_pairs, "ref")
        else:
            self._store_flows(fwd_flow, rev_flow, self._flows, self._flow_pairs, "flows")

    def validate(
        self, supported: List[str], special: Dict[str, str]
    ) -> List[Tuple[Optional["Flow"], Optional[ValidationFlow], Optional["ValidationResult"]]]:
        """Perform the validation of flows present in the validator.

        Compute validation results for all possible flow mappings.
        Find the mapping with the least number of errors.
        Gather results for individual validations performed in the best permutation.

        Parameters
        ----------
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        List[Tuple[Optional["Flow"], Optional[ValidationFlow], Optional["ValidationResult"]]]
            Validation results.
            Format: [(flow, reference, result), (flow, None, None), (None, reference, None), ...]
        """

        perms = self._gen_permutations()

        # Find the permutation producing the least amount of errors.
        best_perm = None
        best_score = None
        for perm in perms:
            tests = self._break_perm_to_tests(perm)
            score = self._compute_tests_score(tests, supported, special)
            if best_score is None or score < best_score:
                best_score = score
                best_perm = perm

        return self._convert_perm_to_results(best_perm)

    def _store_flows(
        self, fwd_flow: "Flow", rev_flow: Optional["Flow"], storage: List["Flow"], pairs: Dict[int, int], f_type: str
    ) -> None:
        """Store provided flow objects to the validator private structures.

        Parameters
        ----------
        fwd_flow : Flow,
            Flow object.
        rev_flow : Flow, None
            Flow object known to be part of the same bi-flow, just in the opposite direction.
            Can be None.
        storage : list
            Collection for storing the Flow object.
        pairs: dict
            Map of flow pairs, where new pair should be added, if the reverse flow exists.
        f_type: str
            Flow type.
        """
        size = len(storage)
        storage.append(fwd_flow)
        self._flow_map[fwd_flow.key][f_type].append(size)
        if rev_flow:
            storage.append(rev_flow)
            self._flow_map[rev_flow.key][f_type].append(size + 1)
            pairs[size] = size + 1
            pairs[size + 1] = size

    def _gen_permutations(self) -> List[List[int]]:
        """Generate all possible valid mappings of flows to reference flows.

        Firstly, create possible mappings of forward flows to forward references
        resp. reverse flows to reverse references separately.
        Then, make a cross product of those mappings.
        Finally, filter out mappings which are not feasible due to known bi-flow pairings.

        Returns
        ------
        List[List[int]]
            Mappings of flows to reference flows.
        """

        perms = [None, None]
        for index, val in enumerate(self._flow_map.values()):
            self._ref_flow_indexes += val["ref"]
            flows = val["flows"]
            ref_cnt = len(val["ref"])
            if len(flows) < ref_cnt:
                # Extend the flows field.
                flows += [-1] * (ref_cnt - len(flows))

            perms[index] = tuple(set(permutations(flows, ref_cnt)))

        ret = []
        for prod in list(product(perms[0], perms[1])):
            collapsed = prod[0] + prod[1]
            if self._is_perm_valid(collapsed):
                ret.append(collapsed)

        return ret

    def _convert_perm_to_results(
        self, perm: List[int]
    ) -> List[Tuple[Optional["Flow"], Optional[ValidationFlow], Optional["ValidationResult"]]]:
        """Find flow validation results which correspond to the provided permutation.

        Parameters
        ----------
        perm : list
            Mapping of flow indexes to reference flows.

        Returns
        ------
        List[Tuple[Optional["Flow"], Optional[ValidationFlow], Optional["ValidationResult"]]]
            Validation results.
            Format: [(flow, reference, result), (flow, None, None), (None, reference, None), ...]
        """

        ret = []
        for test in self._break_perm_to_tests(perm):
            ret.append((self._flows[test[0]], self._ref[test[1]], self._results_cache[test][0]))
            if len(test) > 2:
                ret.append((self._flows[test[2]], self._ref[test[3]], self._results_cache[test][1]))

        # Missing flows.
        for index, val in enumerate(perm):
            if val == -1:
                ret.append((None, self._ref[self._ref_flow_indexes[index]], None))

        # Unexpected flows.
        for index, flow in enumerate(self._flows):
            if index not in perm:
                ret.append((flow, None, None))

        return ret

    def _compute_tests_score(
        self, tests: List[Tuple[int, int, Optional[int], Optional[int]]], supported: List[str], special: Dict[str, str]
    ) -> int:
        """Get error score for provided batch of tests.

        Parameters
        ----------
        tests : List[Tuple[int, int, Optional[int], Optional[int]]]
            Bi-flow validation tests.
            Format: [(flow index 1, reference flow index 1, flow index 2, reference flow index 2)]
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"

        Returns
        ------
        int
            Total score of this test batch.
        """

        score = 0
        for test in tests:
            # May have already been computed before.
            if test in self._results_cache:
                score += self._results_cache[test][2]
                continue

            res_a, res_b = self._validate_biflow(supported, special, *self._get_biflow_indexes(*test))
            test_score = res_a.get_score()
            if res_b is not None:
                test_score += res_b.get_score()

            score += test_score
            self._results_cache[test] = (res_a, res_b, test_score)

        return score

    def _get_biflow_indexes(
        self, f_index: int, r_index: int, f_index_rev: Optional[int] = None, r_index_rev: Optional[int] = None
    ) -> Tuple["Flow", ValidationFlow, Optional["Flow"], Optional[ValidationFlow]]:
        """Get Flow objects based on their indexes

        Parameters
        ----------
        f_index : int
            Index of the forward flow.
        r_index : int
            Index of the forward reference flow.
        f_index_rev : int, None
            Index of the reverse flow.
        r_index_rev : int, None
            Index of the reverse reference flow.

        Returns
        ------
        tuple
            Flows objects according to the indexes.
        """

        if f_index_rev is not None:
            f_index_rev = self._flows[f_index_rev]
            r_index_rev = self._ref[r_index_rev]

        return self._flows[f_index], self._ref[r_index], f_index_rev, r_index_rev

    @staticmethod
    def _validate_biflow(
        supported: List[str],
        special: Dict[str, str],
        flow_a: "Flow",
        ref_a: ValidationFlow,
        flow_b: Optional["Flow"],
        ref_b: Optional[ValidationFlow],
    ) -> Tuple["ValidationResult", Optional["ValidationResult"]]:
        """Perform biflow validation. The opposite direction may not be present.

        Parameters
        ----------
        supported : list
            Names of fields which should be compared if present in the flow object.
        special : dict
            Fields which require non-standard evaluation (typically lists).
            Format: "name: strategy"
        flow_a: Flow
            First flow object.
        ref_a: ValidationFlow
            Reference flow to the first flow.
        flow_b: Flow, None
            Seconds flow object in the opposite direction.
        ref_b: ValidationFlow, None
            Reference flow to the second flow.

        Returns
        ------
        tuple
            Validation results. The second result is None if flow from the opposite direction is not provided.
        """

        result_a = ref_a.validate(flow_a, flow_b, supported, special)
        result_b = ref_b.validate(flow_b, flow_a, supported, special) if flow_b is not None else None

        return result_a, result_b

    def _break_perm_to_tests(self, perm: List[int]) -> List[Tuple[int, int, Optional[int], Optional[int]]]:
        """Break the flow mapping into several individual bi-flow tests.

        Parameters
        ----------
        perm : list
            Mapping of flow indexes to reference flows.

        Returns
        ------
        List[Tuple[int, int, Optional[int], Optional[int]]]
            Format: [(flow index 1, reference flow index 1, flow index 2, reference flow index 2]
        """

        used = []
        tests = []
        for index, f_index in enumerate(perm):
            # Skip if the flow is already used in test or if the reference flow does not have a flow assigned.
            if f_index in used or f_index == -1:
                continue

            r_index = self._ref_flow_indexes[index]
            r_pair_index = self._ref_pairs.get(r_index, -1)

            rev_f_index = perm[self._get_ref_pos(r_pair_index)]
            if r_pair_index == -1 or rev_f_index == -1:
                # Reference flow does not originate from a biflow or the reverse flow is not present.
                tests.append((f_index, r_index))
                used.append(f_index)
                continue

            tests.append((f_index, r_index, rev_f_index, r_pair_index))
            used.append(f_index)
            used.append(rev_f_index)

        return tests

    def _is_perm_valid(self, perm: List[int]) -> bool:
        """Determine whether the flow mapping violates the known bi-flow pairings.

        Parameters
        ----------
        perm : list
            Mapping of flow indexes to reference flows.

        Returns
        ------
        bool
            True - valid mapping, False otherwise
        """

        for i, f_index in enumerate(perm):
            r_index = self._ref_flow_indexes[i]
            if f_index == -1:
                # f_index is -1 (missing flow) - valid
                continue

            f_pair_index = self._flow_pairs.get(f_index, -1)
            r_pair_index = self._ref_pairs.get(r_index, -1)
            ref_rev_pos = self._get_ref_pos(r_pair_index)
            flow_rev_pos = self._get_flow_pos(f_pair_index, perm)
            if f_pair_index == -1:
                # Flow does not originate from biflow.
                if ref_rev_pos == -1:
                    # Reverse reference flow does not originate from biflow.
                    continue

                if self._flow_pairs.get(perm[ref_rev_pos], -1) != -1:
                    # Flow at reverse position exists and has a pair - invalid.
                    return False

                continue

            if r_pair_index == -1:
                # Reference flow does not originate from biflow.
                if flow_rev_pos == -1 or self._ref_pairs.get(self._ref_flow_indexes[flow_rev_pos], -1) != -1:
                    # Reverse flow is not in the permutation or
                    # reference flow at reverse position exists and has a pair - invalid.
                    return False

                continue

            if ref_rev_pos != flow_rev_pos:
                # Reverse flow and reverse reference flow are not at the same permutation slot.
                return False

        return True

    def _get_ref_pos(self, index: int) -> int:
        """Get position of a reference flow in the flow mapping.

        Parameters
        ----------
        index : int
            Index of the reference flow.

        Returns
        ------
        int
            Position in the flow mapping, -1 if not present.
        """
        if index == -1:
            return -1

        return self._ref_flow_indexes.index(index)

    @staticmethod
    def _get_flow_pos(index: int, perm: List[int]) -> int:
        """Get position of flow in the flow mapping.

        Parameters
        ----------
        index : int
            Index of the flow.
        perm : list
            Flow mapping.

        Returns
        ------
        int
            Position in the flow mapping, -1 if not present.
        """
        if index == -1 or index not in perm:
            return -1

        return perm.index(index)
