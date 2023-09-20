"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""

import ipaddress
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Union

from ftanalyzer.models.sm_data_types import SMSubnetSegment, SMTimeSegment


class PMTestCategory(Enum):
    """Enum with possible test categories."""

    MISSING = 1  # missing flows
    UNEXPECTED = 2  # unexpected flows
    SHIFTED = 3  # differences in timestamps
    SCALED = 4  # differences in values of packets and bytes


@dataclass
class PMFlow:
    """
    Flow object.
    Attribute names are kept the same as in the dataframes for easier data manipulation.
    """

    # pylint: disable=invalid-name
    SRC_IP: Union[ipaddress.IPv4Address, ipaddress.IPv6Address]
    DST_IP: Union[ipaddress.IPv4Address, ipaddress.IPv6Address]
    PROTOCOL: int
    SRC_PORT: int
    DST_PORT: int
    PACKETS: int
    BYTES: int
    START_TIME: int
    END_TIME: int
    TIME_DIFF: int = 0

    def __str__(self) -> str:
        return (
            f"{self.START_TIME},{self.END_TIME},{self.PROTOCOL},{self.SRC_IP},{self.DST_IP},"
            f"{self.SRC_PORT},{self.DST_PORT},{self.PACKETS},{self.BYTES}"
        )


@dataclass
class PMFlowPair:
    """Pair of flow and its assumed reference."""

    flow: PMFlow
    ref: PMFlow


@dataclass
class PMTestOutcome:
    """Test outcome of a segment.

    Contains errors of all possible categories.

    Attributes
    ----------
    segment : SMSubnetSegment, SMTimeSegment, None
        Segment used to filter flow and reference data.
    missing : list
        Flows which were not present but were expected.
    unexpected : list
        Flows which were present but were not expected.
    shifted : list
        Pairs of a flow and its assumed reference flow which time difference is more than expected.
    scaled : list
        Pairs of a flow and its assumed reference flow which values of packets and bytes differ.
    """

    segment: Union[SMSubnetSegment, SMTimeSegment, None] = None
    missing: list[PMFlow] = field(default_factory=list)
    unexpected: list[PMFlow] = field(default_factory=list)
    shifted: list[PMFlowPair] = field(default_factory=list)
    scaled: list[PMFlowPair] = field(default_factory=list)

    def is_passing(self) -> bool:
        """The test is passing if no errors are present in any category.

        Returns
        ------
        bool
            True - test passed, False - test failed
        """
        return (len(self.missing) + len(self.unexpected) + len(self.shifted) + len(self.scaled)) == 0


class PreciseReport:
    """Acts as a storage for tests performed in a precise model.

    Attributes
    ----------
    tests : list
        Test outcomes for each segment.
    """

    ERR_CLR = "\033[31m"
    RST_CLR = "\033[0m"

    def __init__(self) -> None:
        """Basic init."""

        self.tests = []

    def add_segment(self, segment: Union[SMSubnetSegment, SMTimeSegment, None]) -> None:
        """Add new segment to the report. All following tests will be added to this segment
        until the next segment is added.

        Parameters
        ----------
        segment : SMSubnetSegment, SMTimeSegment, None
            New segment.

        Raises
        ------
        ValueError
            This segment is already present.
        """

        if self.get_test(segment) is not None:
            raise ValueError(f"Segment {segment} is already present.")

        self.tests.append(PMTestOutcome(segment=segment))

    def add_test(self, category: PMTestCategory, flow: PMFlow, ref: Optional[PMFlow] = None) -> None:
        """Add new test to a category.

        Parameters
        ----------
        category : PMTestCategory
            Category the test should be added to.
        flow : PMFlow
            Flow from a probe.
        ref : PMFlow, None
            Reference flow, needed for categories SHIFTED and SCALED.
        """

        # There must be at least one segment.
        assert len(self.tests) > 0

        if category == PMTestCategory.MISSING:
            self.tests[-1].missing.append(flow)
        elif category == PMTestCategory.UNEXPECTED:
            self.tests[-1].unexpected.append(flow)
        elif category == PMTestCategory.SHIFTED:
            assert ref is not None
            self.tests[-1].shifted.append(PMFlowPair(flow, ref))
        elif category == PMTestCategory.SCALED:
            assert ref is not None
            self.tests[-1].scaled.append(PMFlowPair(flow, ref))
        else:
            assert False

    def is_passing(self) -> bool:
        """Get information whether all performed tests passed.

        Returns
        ------
        bool
            True - all tests passed, False - some tests failed
        """

        return all(test.is_passing() for test in self.tests)

    def get_test(self, segment: Union[SMSubnetSegment, SMTimeSegment, None] = None) -> Optional[PMTestOutcome]:
        """Find specific test outcome for a segment.

        Parameters
        ----------
        segment: SMSubnetSegment, SMTimeSegment, None
            Segment that was used in the test (if any).

        Returns
        ------
        PMTestOutcome, None
            Test outcome. None if a test could not be found for the given segment.
        """

        for test in self.tests:
            if test.segment == segment:
                return test

        return None

    def print_results(self, limit: int = 100) -> None:
        """Print results of all tests to stdout.

        Parameters
        ----------
        limit: int
            Limit the amount of reported errors per category per segment.
        """

        def print_category(category, title, callback):
            if len(category) > 0:
                print(" - " + title)
                for item in category[:limit]:
                    callback(item)

                total = len(category)
                if total > limit:
                    print(f"\t{self.ERR_CLR}Truncated, total records: {total}{self.RST_CLR}")

        for test in self.tests:
            print()
            if test.segment is None:
                print("ALL DATA")
            else:
                print(test.segment)

            if test.is_passing():
                print(" - PASSED")
                continue

            print_category(test.missing, "missing flows", lambda item: print(f"\t{self.ERR_CLR}{item}{self.RST_CLR}"))
            print_category(
                test.unexpected, "unexpected flows", lambda item: print(f"\t{self.ERR_CLR}{item}{self.RST_CLR}")
            )
            print_category(
                test.shifted,
                "incorrect timestamps",
                lambda item: print(
                    f"\t{self.ERR_CLR}{item.flow} != {item.ref} (diff: {item.flow.TIME_DIFF} ms){self.RST_CLR}"
                ),
            )
            print_category(
                test.scaled,
                "incorrect values of packets / bytes",
                lambda item: print(f"\t{self.ERR_CLR}{item.flow} != {item.ref}{self.RST_CLR}"),
            )
