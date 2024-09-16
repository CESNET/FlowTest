"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""

from typing import List, Optional, Union

from ftanalyzer.models.sm_data_types import (
    SMMetricType,
    SMSubnetSegment,
    SMTestOutcome,
    SMTimeSegment,
)


class StatisticalReport:
    """Acts as a storage for tests performed in a statistical model.

    Attributes
    ----------
    tests : list
        Statistics of compared flow fields.
    """

    ERR_CLR = "\033[31m"
    RST_CLR = "\033[0m"

    def __init__(self) -> None:
        """Basic init."""
        self.tests = []

    def add_test(self, test: SMTestOutcome) -> None:
        """Append the performed test into the report.

        Parameters
        ----------
        test : SMTest
            Test to be added into the report.
        """
        self.tests.append(test)

    def is_passing(self) -> bool:
        """Get information whether all performed tests passed.

        Returns
        ------
        bool
            True - all tests passed, False - some tests failed
        """
        return all(test.is_passing() for test in self.tests)

    def get_failed(self) -> List[SMTestOutcome]:
        """Get list of failed tests.

        Returns
        ------
        list
            List of failed tests.
        """
        return [test for test in self.tests if not test.is_passing()]

    def get_test(
        self, metric: SMMetricType, segment: Optional[Union[SMSubnetSegment, SMTimeSegment]] = None
    ) -> Optional[SMTestOutcome]:
        """Find specific test.

        Parameters
        ----------
        metric : SMMetricType
            Metric which was used in the test.
        segment: SMSubnetSegment, SMTimeSegment, None
            Segment that was used in the test (if any).

        Returns
        ------
        SMTest, None
            Test which satisfies the criteria. None if a test could not be found.
        """

        for test in self.tests:
            if test.metric.key == metric and test.segment == segment:
                return test

        return None

    def print_results(self) -> None:
        """Print results of all tests to stdout."""

        print()
        for test in self.tests:
            test_str = ""
            if not test.is_passing():
                test_str = f"{self.ERR_CLR}"

            if test.segment is None:
                test_str += "ALL DATA"
            else:
                test_str += str(test.segment)

            test_str += (
                f"\t{test.metric.key.value}\t{test.diff:.4f}/"
                f"{test.metric.diff:.4f}\t({test.value}/{test.reference})"
            )
            if not test.is_passing():
                test_str += f"{self.RST_CLR}"

            print(test_str)
