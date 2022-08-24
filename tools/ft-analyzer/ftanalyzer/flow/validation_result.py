"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

File contains definition of ValidationResult and ValidationStats classes which hold
information about the result of comparison between two flows.
"""

from typing import Dict, Union, List, Tuple


class ValidationStats:
    """Class for storing flow comparison counters.

    Attributes
    ----------
    _correct : int
        Number of times compared flow fields were equal.
    _missing : int
        Number of times a flow field was missing.
    _wrong : int
        Number of times compared flow fields were not equal.
    _unexpected : int
        Number of times a flow field was present but should not have been.
    """

    __slots__ = ("_correct", "_missing", "_wrong", "_unexpected")

    def __init__(self) -> None:
        """Initialize all counters to 0."""
        self._correct = 0
        self._missing = 0
        self._wrong = 0
        self._unexpected = 0

    def update(self, correct: int = 0, missing: int = 0, wrong: int = 0, unexpected: int = 0) -> None:
        """Increase counters.

        Parameters
        ----------
        correct : int
            Number of times compared flow fields were equal.
        missing : int
            Number of times a flow field was missing.
        wrong : int
            Number of times compared flow fields were not equal.
        unexpected : int
            Number of times a flow field was present but should not have been.
        """

        self._correct += correct
        self._missing += missing
        self._wrong += wrong
        self._unexpected += unexpected

    def score(self) -> int:
        """Get error score which is the sum of all error counters.

        Returns
        ------
        int
            Sum of all error counters.
        """

        return self._missing + self._wrong + self._unexpected

    def get_report(self) -> Dict[str, int]:
        """Get all error counters.

        Returns
        ------
        dict
            Error counters.
        """

        return {
            "correct": self._correct,
            "missing": self._missing,
            "wrong": self._wrong,
            "unexpected": self._unexpected,
        }


class ValidationResult:
    """Class for storing flow comparison errors and counters.

    Attributes
    ----------
    _missing : list
        Missing fields and their expected values.
    _wrong : list
        Fields which have different values than they should.
    _unexpected : list
        Unexpected fields.
    _stats : Dict[str, ValidationStats]
        Validation counters per each compared flow field.
    """

    __slots__ = ("_missing", "_wrong", "_unexpected", "_stats")

    def __init__(self) -> None:
        """Initialize empty errors and counters."""

        self._missing = []
        self._wrong = []
        self._unexpected = []
        self._stats = {}

    def merge(self, other: "ValidationResult") -> None:
        """Merge other validation result into this one.

        Parameters
        ----------
        other : ValidationResult
            Results that should be merged into this one.
        """

        error_rep = other.get_error_report()
        self._missing += error_rep["missing"]
        self._wrong += error_rep["wrong"]
        self._unexpected += error_rep["unexpected"]

        for field, stats in other.get_stats_report().items():
            self._update_stats(field, **stats)

    def report_correct_field(self, name: str) -> None:
        """Report that field comparison was successful.

        Parameters
        ----------
        name : str
            Name of the field.
        """

        self._update_stats(name, correct=1)

    def report_missing_field(self, name: str, reference: Union[str, int]) -> None:
        """Report missing field.

        Parameters
        ----------
        name : str
            Name of the field.
        reference : str, int
            Expected field value.
        """
        self._update_stats(name, missing=1)
        self._missing.append((name, reference))

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

        self._update_stats(name, wrong=1)
        self._wrong.append((name, value, reference))

    def report_unexpected_field(self, name: str, value: Union[str, int]) -> None:
        """Report unexpected field.

        Parameters
        ----------
        name : str
            Name of the field.
        value : str, int
            Field value.
        """

        self._update_stats(name, unexpected=1)
        self._unexpected.append((name, value))

    def get_score(self) -> int:
        """Get number of errors in the validation result.

        Returns
        ------
        int
            Number of errors in the result.
        """

        return sum(map(lambda stat: stat.score(), self._stats.values()))

    def get_stats_report(self) -> Dict[str, Dict[str, int]]:
        """Get statistics counters per each field.

        Returns
        ------
        Dict[str, Dict[str, int]]
            Statistics counters per field.
            Format: "{field name: {counter name: counter value}}"
        """

        return {field: stats.get_report() for field, stats in self._stats.items()}

    def get_error_report(self) -> Dict[str, List[Tuple[str, ...]]]:
        """Get all reported errors.

        Returns
        ------
        Dict[str, List[Tuple[str, ...]]]
            Reported errors per error type.
            Format: {
                    missing: [(name, expected value), ...],
                    wrong: [(name, actual value, expected value), ...],
                    unexpected: [(name, actual value), ...],
                    }
        """

        return {"missing": self._missing, "wrong": self._wrong, "unexpected": self._unexpected}

    def _update_stats(self, name: str, correct: int = 0, missing: int = 0, wrong: int = 0, unexpected: int = 0) -> None:
        """Increase counters of a specified field.

        Parameters
        ----------
        name : str
            Name of the field.
        correct : int
            Number of times compared flow fields were equal.
        missing : int
            Number of times a flow field was missing.
        wrong : int
            Number of times compared flow fields were not equal.
        unexpected : int
            Number of times a flow field was present but should not have been.
        """

        if name not in self._stats:
            self._stats[name] = ValidationStats()

        self._stats[name].update(correct=correct, missing=missing, wrong=wrong, unexpected=unexpected)
