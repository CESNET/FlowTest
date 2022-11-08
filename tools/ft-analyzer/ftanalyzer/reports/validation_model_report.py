"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains class ValidationModelReport which processes validation results
and provides report from validation testing.
"""

from typing import Any, Dict, Optional, Tuple

from ftanalyzer.flow import ValidationResult


class ValidationModelReport:
    """Acts as a storage for validation results which are created during the validation testing.
    Provides a report after the validation test is concluded.

    Attributes
    ----------
    _results : dict
        Storage of validation results per each flow key.
        Format: {flow key: {"missing": [reference flows], "unexpected": [flows], "comparisons": [(flow, result)]}
    _key_fmt : tuple
        Flow key format used during the validation test.
    _passing : bool
        Flag indicating whether the validation model is passing or not.
    """

    ERR_CLR = "\033[91m"
    RST_CLR = "\033[0m"

    def __init__(self, key_fmt: Tuple[str, ...]) -> None:
        """Basic init.

        Parameters
        ----------
        key_fmt : tuple
            Flow key format used during the validation test.
        """
        self._results = {}
        self._key_fmt = key_fmt
        self._passing = True

    def add_validation_result(
        self,
        flow: Optional["Flow"],
        reference: Optional["ValidationFlow"],
        result: Optional[ValidationResult],
    ) -> None:
        """Store validation result.

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
        if key not in self._results:
            self._results[key] = {"missing": [], "unexpected": [], "comparisons": []}

        if not result or result.get_score() != 0:
            self._passing = False

        if flow is None and reference is not None:
            self._results[key]["missing"] += [reference]
        elif flow is not None and result is not None:
            self._results[key]["comparisons"] += [(flow, result)]
        elif flow is not None:
            self._results[key]["unexpected"] += [flow]

    def is_passing(self) -> bool:
        """Get information whether the model contains some validation errors or not.

        Returns
        ------
        bool
            True - no errors observed, False - errors observed
        """
        return self._passing

    def get_report(self) -> dict:
        """Generate validation report.

        Returns
        ------
        dict
            Validation report containing flow key format and errors of all flow results.
            See ValidationResult.get_error_report() for more information.
            Format:
            {"key": (names key fields),
            "flows": {
                flow key 1: {
                    "missing": [{field name: field value}, ...]                 # fields of flows that were missing
                    "unexpected": [{field name: field value}, ...]              # fields of flows that were unexpected
                    "errors": [({field name: field value}, result error report), ...]
                    }
                }
            }
        """

        report = {"key": self._key_fmt, "flows": {}}
        for flow_key, results in self._results.items():
            flow_key_report = {
                "missing": [flow.get_non_key_fields() for flow in results["missing"]],
                "unexpected": [flow.get_non_key_fields() for flow in results["unexpected"]],
                "errors": [(flow.get_non_key_fields(), res.get_error_report()) for flow, res in results["comparisons"]],
            }

            report["flows"][flow_key] = flow_key_report

        return report

    def get_stats(self) -> Dict[str, Dict[str, int]]:
        """Generate report of statistics counters per flow field across all validation results.

        Returns
        ------
        Dict[str, Dict[str, int]]
            Statistics counters per flow field.
            Format: "{field name: {counter name: counter value}}"
        """

        all_stats = ValidationResult()
        comparison_results = [res for result in self._results.values() for res in result["comparisons"]]
        for res in comparison_results:
            all_stats.merge(res[1])

        return all_stats.get_stats_report()

    def print_report(self) -> None:
        """Generate validation test report and prints it to stdout."""

        report = self.get_report()
        key_format = report["key"]
        for key, flow_report in report["flows"].items():
            print(self._dict_to_str(dict(zip(key_format, key))))
            for flow in flow_report["missing"]:
                print(f"\t- Flow record: {self._dict_to_str(flow)}")
                print(f"{self.ERR_CLR}\t\t- Flow record not found.{self.RST_CLR}")

            for flow in flow_report["unexpected"]:
                print(f"\t- Flow record: {self._dict_to_str(flow)}")
                print(f"{self.ERR_CLR}\t\t- Unexpected flow record.{self.RST_CLR}")

            for flow, res in flow_report["errors"]:
                print(f"\t- Flow record: {self._dict_to_str(flow)}")
                for err in res["missing"]:
                    print(f"{self.ERR_CLR}\t\t- Missing '{err[0]}' value (expected: '{err[1]}').{self.RST_CLR}")
                for err in res["unexpected"]:
                    print(f"{self.ERR_CLR}\t\t- Unexpected field '{err[0]}' (value: '{err[1]}').{self.RST_CLR}")
                for err in res["wrong"]:
                    print(
                        f"{self.ERR_CLR}\t\t- Invalid '{err[0]}' value (expected: '{err[2]}', observed: '{err[1]}')"
                        f".{self.RST_CLR}"
                    )

    def print_stats(self) -> None:
        """Generate validation test statistics report and prints it to stdout."""

        report = self.get_stats()
        print(f"{'FIELD':<50}{'CORRECT':<10}{'WRONG':<10}{'MISSING':<10}{'UNEXPECTED':<10}")
        for field, stats in report.items():
            print(
                f"{field:<50}{stats['correct']:<10}{stats['wrong']:<10}{stats['missing']:<10}{stats['unexpected']:<10}"
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

        return "[" + ret[: len(ret) - 2] + "]"
