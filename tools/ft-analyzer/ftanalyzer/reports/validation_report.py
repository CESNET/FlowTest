"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains class ValidationReport which processes validation results
and provides report from validation testing.
"""

from typing import Any, Dict, List, Optional, Set, Tuple
from dataclasses import dataclass
from collections import defaultdict

from ftanalyzer.flow import FieldsDict, ValidationResult, ValidationStats


class ValidationReportSummary:
    """Class for merging data from multiple ValidationReport objects
    to provide summarized statistics across multiple validation tests.

    Attributes
    ----------
    fields : defaultdict
        Dictionary with summary counters of each field.
    flows : ValidationStats
        Summary counters for flow statistics.
    unknown_fields : set
        Fields which could not be recognized by Normalizer.
    unmapped_fields : set
        Fields which could not be mapped by Mapper.
    """

    def __init__(self) -> None:
        """Basic init."""
        # pylint: disable=W0108
        self.fields = defaultdict(lambda: ValidationStats())
        self.flows = ValidationStats()
        self.unknown_fields = set()
        self.unmapped_fields = set()

    def update_fields_stats(self, fields: Dict[str, ValidationStats]) -> None:
        """Update field statistics with a dictionary of ValidationStats object.

        Parameters
        ----------
        fields : dict
            Flow fields statistics from a validation report.
        """
        for name, stats in fields.items():
            self.fields[name].update(stats)

    def update_flows_stats(self, stats: ValidationStats) -> None:
        """Update flow statistics with a ValidationStats object.

        Parameters
        ----------
        stats : ValidationStats
            Flow statistics from a validation report.
        """
        self.flows.update(stats)

    def update_unknown_fields(self, fields: Set[str]) -> None:
        """Update the set of unknown fields.

        Parameters
        ----------
        fields : set
            Flow field names.
        """
        self.unknown_fields.update(fields)

    def update_unmapped_fields(self, fields: Set[str]) -> None:
        """Update the set of unmapped fields.

        Parameters
        ----------
        fields : set
            Flow field names.
        """
        self.unmapped_fields.update(fields)

    def get_fields_summary(self) -> ValidationStats:
        """Get fields statistics summary across all compared flow fields.

        Returns
        ------
        ValidationStats
            Flow fields statistics summary.
        """

        summary = ValidationStats()
        for stats in self.fields.values():
            summary.update(stats)

        return summary


@dataclass
class ValidationReportFlow:
    """Dataclass with validation result of a specific flow comparison.

    Attributes
    ----------
    fields : FieldsDict
        Flow fields with its values (except fields part of a flow key).
    result : ValidationResult
        Validation result object.
    """

    fields: FieldsDict
    result: ValidationResult


@dataclass
class ValidationReportFlowGroup:
    """Dataclass with validation results of flows with the same key.

    Attributes
    ----------
    flows : list
        Validation results of individual flows.
    missing : list
        Missing flows (in form of expected flow fields).
    unexpected : list
        Unexpected flows (in form of observed flow fields).
    """

    flows: List[ValidationReportFlow]
    missing: List[Dict[str, Any]]
    unexpected: List[Dict[str, Any]]

    def add_missing_flow(self, fields: FieldsDict) -> None:
        """Add missing flow to the flow group report.

        Parameters
        ----------
        fields : FieldsDict
            Flow fields.
        """

        self.missing.append(fields)

    def add_unexpected_flow(self, fields: FieldsDict) -> None:
        """Add unexpected flow to the flow group report.

        Parameters
        ----------
        fields : FieldsDict
            Flow fields.
        """

        self.unexpected.append(fields)

    def add_comparison_result(self, fields: FieldsDict, result: ValidationResult) -> None:
        """Add flow validation to the flow group report.

        Parameters
        ----------
        fields : FieldsDict
            Flow fields.
        result : ValidationResult
            Result of the flow validation process.
        """

        self.flows.append(ValidationReportFlow(fields, result))


class ValidationReport:
    """Acts as a storage for validation results which are created during the validation testing.
    Provides a report after the validation test is concluded.

    Attributes
    ----------
    fields_stats : defaultdict
        Statistics of compared flow fields.
    flows_stats : ValidationStats
        Statistics of processed flows.
    key_fmt : tuple
        Flow key format used during the validation test.
    flow_groups : defaultdict
        Flows aggregated by their flow key.
    passing : bool
        Flag indicating whether the validation model is passing or not.
    """

    ERR_CLR = "\033[31m"
    WARN_CLR = "\033[33m"
    RST_CLR = "\033[0m"

    def __init__(self, key_fmt: Tuple[str, ...]) -> None:
        """Initialize validation test report.

        Parameters
        ----------
        key_fmt : tuple
            Flow key format.
        """
        # pylint: disable=W0108
        self.fields_stats = defaultdict(lambda: ValidationStats())
        self.flows_stats = ValidationStats()
        self.key_fmt = key_fmt
        self.flow_groups = defaultdict(lambda: ValidationReportFlowGroup([], [], []))
        self.passing = True

    def add_entry(
        self,
        flow: Optional["Flow"],
        reference: Optional["ValidationFlow"],
        result: Optional[ValidationResult],
    ) -> None:
        """Add entry to the validation report.

        Empty flow and existing reference flow indicates missing flow.
        Empty reference flow and existing flow indicates unexpected flow.

        Parameters
        ----------
        flow : Flow, None
            Flow that was validated.
        reference : ValidationFlow, None
            Reference flow.
        result : ValidationResult, None
            Result of the validation.
        """

        key = flow.key if flow is not None else reference.key
        if not result or result.score() != 0:
            self.passing = False

        if flow is None and reference is not None:
            self.flow_groups[key].add_missing_flow(reference.get_non_key_fields())
            self.flows_stats.missing += 1
        elif flow is not None and reference is None:
            self.flow_groups[key].add_unexpected_flow(flow.get_non_key_fields())
            self.flows_stats.unexpected += 1
        elif flow and reference and result:
            if result.score() != 0:
                self.flows_stats.error += 1
            else:
                self.flows_stats.ok += 1

            self.flow_groups[key].add_comparison_result(flow.get_non_key_fields(), result)
            for name, stats in result.stats.items():
                self.fields_stats[name].update(stats)

    def is_passing(self) -> bool:
        """Get information whether the model contains some validation errors or not.

        Returns
        ------
        bool
            True - no errors observed, False - errors observed
        """
        return self.passing

    def get_result_by_key_and_field(self, key: Tuple[str, ...], name: str, value: Any) -> Optional[ValidationResult]:
        """Find the first flow in a flow group which contains specified flow field and value.

        Parameters
        ----------
        key : tuple
            Flow key to select flow group.
        name : str
            Flow field to be searched in every flow.
        value : Any
            Value of the flow field.

        Returns
        ------
        ValidationResult, None
            Validation result of the found flow. None if no matching flow was found.
        """

        if key not in self.flow_groups:
            return None

        for flow in self.flow_groups[key].flows:
            if name in flow.fields and flow.fields[name] == value:
                return flow.result

        return None

    def get_fields_summary_stats(self) -> ValidationStats:
        """Get fields statistics summary across all compared flow fields.

        Returns
        ------
        ValidationStats
            Flow fields statistics summary.
        """

        summary = ValidationStats()
        for stats in self.fields_stats.values():
            summary.update(stats)

        return summary

    def print_results(self) -> None:
        """Print validation test report to stdout."""

        for key, group in self.flow_groups.items():
            print(self._dict_to_str(dict(zip(self.key_fmt, key))))
            for flow in group.missing:
                print(f"\t{self._dict_to_str(flow)}")
                print(f"{self.ERR_CLR}\t\t- Flow record not found.{self.RST_CLR}")

            for flow in group.unexpected:
                print(f"\t{self._dict_to_str(flow)}")
                print(f"{self.ERR_CLR}\t\t- Unexpected flow record.{self.RST_CLR}")

            for flow in group.flows:
                print(f"\t{self._dict_to_str(flow.fields)}")
                for field in flow.result.missing:
                    print(
                        f"{self.ERR_CLR}\t\t- Missing '{field.name}' value "
                        f"(expected: '{field.expected}').{self.RST_CLR}"
                    )
                for field in flow.result.unexpected:
                    print(
                        f"{self.ERR_CLR}\t\t- Unexpected field '{field.name}' "
                        f"(value: '{field.observed}').{self.RST_CLR}"
                    )
                for field in flow.result.errors:
                    print(
                        f"{self.ERR_CLR}\t\t- Invalid '{field.name}' value "
                        f"(expected: '{field.expected}', observed: '{field.observed}')"
                        f".{self.RST_CLR}"
                    )
                if len(flow.result.unchecked) > 0:
                    unchecked = ", ".join(flow.result.unchecked)
                    print(f"{self.WARN_CLR}\t\t- Unchecked fields: {unchecked}.{self.RST_CLR}")

    def print_flows_stats(self) -> None:
        """Print statistics of the processed flows to stdout."""

        print(f"\n{'':<25}================ FLOWS =================")
        print(f"{'':<25}{'OK':<10}{'ERROR':<10}{'MISSING':<10}{'UNEXPECTED':<10}")
        stats = self.flows_stats
        print(f"{'':<25}{stats.ok:<10}{stats.error:<10}{stats.missing:<10}{stats.unexpected:<10}")

    def print_fields_stats(self) -> None:
        """Print statistics of the compared flow fields to stdout."""

        print(f"\n{'':<25}===================== FIELDS ======================")
        print(f"{'FIELD':<25}{'OK':<10}{'ERROR':<10}{'MISSING':<10}{'UNEXPECTED':<11}{'UNCHECKED':<10}")
        for field, stats in self.fields_stats.items():
            print(
                f"{field:<25}{stats.ok:<10}{stats.error:<10}{stats.missing:<10}"
                f"{stats.unexpected:<11}{stats.unchecked:<10}"
            )

        summary = self.get_fields_summary_stats()
        print(f"{'':<25}{'----------':<10}{'----------':<10}{'----------':<10}{'-----------':<11}{'----------':<10}")
        print(
            f"{'':<25}{summary.ok:<10}{summary.error:<10}{summary.missing:<10}"
            f"{summary.unexpected:<11}{summary.unchecked:<10}"
        )

    @staticmethod
    def _dict_to_str(dictionary: Dict[Any, Any]) -> str:
        """Convert dictionary to string.

        Parameters
        ----------
        dictionary : dict
            Dictionary to be converted.

        Returns
        ------
        str
            Dictionary converted to string.
            Format: [key: value, key: value, ...]
        """

        ret = ""
        for key, val in dictionary.items():
            ret += f"{key}: {val}, "

        return "[" + ret[:-2] + "]"
