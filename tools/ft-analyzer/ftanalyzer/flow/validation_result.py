"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains definition of ValidationResult and ValidationStats classes which hold
information about the result of comparison between two flows.
"""

from dataclasses import dataclass
from collections import defaultdict

from typing import Any, Union, Set


@dataclass
class ValidationStats:
    """Class for storing flow comparison counters.

    Attributes
    ----------
    ok : int
        Number of times compared flow fields were equal.
    error : int
        Number of times a flow field was missing.
    missing : int
        Number of times compared flow fields were not equal.
    unexpected : int
        Number of times a flow field was present but should not have been.
    """

    # pylint: disable=invalid-name
    ok: int = 0
    error: int = 0
    missing: int = 0
    unexpected: int = 0

    def update(self, other: "ValidationStats") -> None:
        """Update every counter with values from the other statistics object.

        Parameters
        ----------
        other : ValidationStats
            Statistics to be used for update.
        """

        self.ok += other.ok
        self.error += other.error
        self.missing += other.missing
        self.unexpected += other.unexpected

    def score(self) -> int:
        """Get error score which is the sum of all error counters.

        Returns
        ------
        int
            Sum of all error counters.
        """

        return self.error + self.missing + self.unexpected


@dataclass
class ValidationField:
    """Flow field validation result issue.

    Attributes
    ----------
    name : str
        Flow field name.
    expected : Any
        Expected value of the flow field (from annotation).
    observed : Any
        Observed value of the flow field (from tested probe).
    """

    name: str
    expected: Any = None
    observed: Any = None


class ValidationResult:
    """Class for storing flow comparison errors and counters.

    Attributes
    ----------
    errors : List[ValidationField]
        Missing fields and their expected values.
    missing : List[ValidationField]
        Fields which have different values than they should.
    unexpected : List[ValidationField]
        Unexpected fields.
    stats : Dict[str, ValidationStats]
        Validation counters per each compared flow field.
    """

    __slots__ = ("errors", "missing", "unexpected", "stats")

    def __init__(self) -> None:
        """Initialize empty error lists and counters."""

        self.errors = []
        self.missing = []
        self.unexpected = []
        # pylint: disable=unnecessary-lambda
        self.stats = defaultdict(lambda: ValidationStats())

    def update(self, other: "ValidationResult") -> None:
        """Update validation result by merging lists of discovered validation issues and counters.

        Parameters
        ----------
        other : ValidationResult
            Result to be merged into this.
        """

        self.errors += other.errors
        self.missing += other.missing
        self.unexpected += other.unexpected

        for name, stats in other.stats.items():
            self.stats[name].update(stats)

    def report_correct_field(self, name: str) -> None:
        """Report that field comparison was successful.

        Parameters
        ----------
        name : str
            Name of the field.
        """

        self.stats[name].ok += 1

    def report_missing_field(self, name: str, reference: Union[str, int]) -> None:
        """Report missing field.

        Parameters
        ----------
        name : str
            Name of the field.
        reference : str, int
            Expected field value.
        """
        self.stats[name].missing += 1
        self.missing.append(ValidationField(name, reference))

    def report_wrong_value_field(self, name: str, value: Union[str, int], reference: Union[str, int]) -> None:
        """Report wrong field value.

        Parameters
        ----------
        name : str
            Name of the field.
        value : str, int
            Actual field value.
        reference : str, int
            Expected field value.
        """

        self.stats[name].error += 1
        self.errors.append(ValidationField(name, reference, value))

    def report_unexpected_field(self, name: str, value: Union[str, int]) -> None:
        """Report unexpected field.

        Parameters
        ----------
        name : str
            Name of the field.
        value : str, int
            Field value.
        """

        self.stats[name].unexpected += 1
        self.unexpected.append(ValidationField(name, None, value))

    def score(self) -> int:
        """Get number of errors in the validation result.

        Returns
        ------
        int
            Number of errors in the result.
        """

        return sum(map(lambda stat: stat.score(), self.stats.values()))
