"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""
from dataclasses import dataclass
from typing import Any, Optional, Union

from ftanalyzer.models.sm_data_types import SMSubnetSegment, SMTimeSegment
from ftanalyzer.models.statistical_model import StatisticalModel
from ftanalyzer.reports.precise_report import PMFlow, PMTestCategory, PreciseReport


@dataclass
class GrouperIteratorWrapper:
    """
    Wrapper for the pandas grouper iterator. Includes information whether the end of the data frame was reached.

    Attributes
    ----------
    key : tuple
        Group key.
    index : any, None
        Indexes of rows which belong to the group.
    empty : bool
        Flag indicating whether the end of the data frame was reached.
    """

    key: tuple = ()
    index: Any = None
    empty: bool = False

    def pop_index(self) -> Any:
        """
        Remove the group from the wrapper marking the group as processed.

        Returns
        ------
        Any
            Indexes of rows which belong to the group.
        """

        tmp = self.index
        self.key = ()
        self.index = None
        return tmp

    def __lt__(self, other: "GrouperIteratorWrapper") -> bool:
        if isinstance(self.key[0], type(other.key[0])):
            return self.key < other.key

        key_1 = (str(self.key[0]), str(self.key[1])) + self.key[2:]
        key_2 = (str(other.key[0]), str(other.key[1])) + other.key[2:]
        return key_1 < key_2


class DoubleFrameIterator:
    """
    Class for iterating over 2 sorted pandas grouper objects at the same time.

    Attributes
    ----------
    _flow_groups : pandas.Grouper
        Groups of flows.
    _ref_groups : pandas.Grouper
        Groups of reference flows.
    _f_iter : dict item iterator
        Flow groups iterator.
    _r_iter : dict item iterator
        Reference flows groups iterator.
    _flow : GrouperIteratorWrapper
        Flow currently being processed.
    _ref : GrouperIteratorWrapper
        Reference flow currently being processed.
    """

    def __init__(self, f_groups: "pandas.Grouper", r_groups: "pandas.Grouper") -> None:
        """
        Initialize the statistical model with sorted input data.

        Parameters
        ----------
        f_groups : pandas.Grouper
            Groups of flows.
        r_groups : pandas.Grouper
            Groups of reference flows.
        """

        self._flow_groups = f_groups
        self._ref_groups = r_groups
        self._f_iter = None
        self._r_iter = None
        self._flow = None
        self._ref = None

    def __iter__(self) -> "DoubleFrameIterator":
        """
        Start iterating from the beginning.

        Returns
        ------
        DoubleFrameIterator
            Iterator object.
        """

        self._f_iter = iter(self._flow_groups.groups.items())
        self._r_iter = iter(self._ref_groups.groups.items())
        self._flow = GrouperIteratorWrapper()
        self._ref = GrouperIteratorWrapper()

        return self

    def __next__(self) -> tuple[GrouperIteratorWrapper, GrouperIteratorWrapper]:
        """
        Get next flow groups. Does not advance to a new group until the current one is processed.

        Returns
        ------
        tuple
            Flow group wrapper, reference flow group wrapper.
        """

        try:
            if self._flow.index is None and not self._flow.empty:
                self._flow.key, self._flow.index = next(self._f_iter)

            if self._ref.index is None and not self._ref.empty:
                self._ref.key, self._ref.index = next(self._r_iter)

            return self._flow, self._ref

        except StopIteration as exc:
            if self._flow.empty or self._ref.empty:
                # 2nd exception, both data frames were traversed
                raise StopIteration from exc

            if self._flow.index is None:
                self._flow.empty = True
                return next(self)

            self._ref.empty = True
            return next(self)


# pylint: disable=too-few-public-methods
class PreciseModel(StatisticalModel):
    """
    Precise model aims to discover specific differences between flows from a probe and expected (reference) flows.
    It is an extension of the statistical model mainly for situations in which network errors (such as packet drops)
    are not expected. The model is able to discover the following differences:
        * missing flows
        * unexpected flows
        * incorrect timestamps
        * incorrect values of packets and bytes

    The format of the input data is the same as for the statistical model.
    Subnet and time segments behave the same as in the statistical model.

    Attributes
    ----------
    _max_time_diff : int
        Maximum acceptable difference between timestamps of a flow and a reference flow (in ms).
    _ok_time_diff : int
        Maximum difference between timestamps of a flow and its reference flow which is not reported (in ms).
    _report : PreciseReport
        Report which is created from discovered flow differences.
    """

    DEFAULT_MAX_TIME_DIFF = 1000
    DEFAULT_OK_TIME_DIFF = 100

    def __init__(
        self,
        flows: str,
        reference: str,
        timeouts: tuple[int, int],
        start_time: int = 0,
    ) -> None:
        """Initialize the statistical model with sorted input data.
        Parameters
        ----------
        flows : str
            Path to a CSV containing flow records acquired from a network probe.
        reference : str
            Path to a CSV containing flow records acting as a reference.
        timeouts : tuple
            Active and inactive timeout (in seconds) which was configured on the network
            probe during the monitoring period.
        start_time : int
            Treat times in the reference file as offsets (in milliseconds) from the provided start time.
            UTC timestamp in milliseconds.

        Raises
        ------
        SMException
            Unable to process provided files.
        """

        super().__init__(flows, reference, start_time, merge=True)
        self._max_time_diff = self.DEFAULT_MAX_TIME_DIFF
        self._ok_time_diff = self.DEFAULT_OK_TIME_DIFF
        self._report = None

    def validate_precise(
        self,
        segments: Optional[list[Union[SMSubnetSegment, SMTimeSegment]]] = None,
        max_time_diff: int = DEFAULT_MAX_TIME_DIFF,
        ok_time_diff: int = DEFAULT_OK_TIME_DIFF,
    ) -> PreciseReport:
        """Evaluate data in the precise model for each of the provided segments.

        Both sets of flows are sorted (in init) and grouped by the flow key.
        The groups are traversed at the same time resulting in linear time complexity O(n + m).

        Parameters
        ----------
        segments : list, None
            Segments for which the evaluation should be performed.
            If None, the evaluation is performed for all data.
        max_time_diff : int
            Maximum acceptable difference between timestamps of a flow and a reference flow (in ms).
        ok_time_diff : int
            Maximum difference between timestamps of a flow and its reference flow which is not reported (in ms).

        Returns
        ------
        PreciseReport
            Report containing results of individual performed tests.
        """

        self._max_time_diff = max_time_diff
        self._ok_time_diff = ok_time_diff
        self._report = PreciseReport()
        if segments is None:
            segments = [None]

        for segment in segments:
            self._report.add_segment(segment)
            flows, refs = self._filter_segment(segment)
            f_group = flows.groupby(self.FLOW_KEY)
            r_group = refs.groupby(self.FLOW_KEY)
            group_itr = iter(DoubleFrameIterator(f_group, r_group))

            try:
                while True:
                    flow, ref = next(group_itr)
                    if flow.empty or (ref.index is not None and ref < flow):
                        self._report_flows(refs.take(ref.pop_index()), PMTestCategory.MISSING)
                    elif ref.empty or (flow.index is not None and flow < ref):
                        self._report_flows(flows.take(flow.pop_index()), PMTestCategory.UNEXPECTED)
                    else:
                        self._compare_group(flows.take(flow.pop_index()), refs.take(ref.pop_index()))
            except StopIteration:
                pass

        return self._report

    def _compare_group(self, flows: "pandas.DataFrame", refs: "pandas.DataFrame") -> None:
        """Find differences among flows in a flow group.

        Parameters
        ----------
        flows : pandas.DataFrame
            Flows from a probe.
        refs : pandas.DataFrame
            Reference flows.
        """

        for _, ref in refs.iterrows():
            flows["TIME_DIFF"] = abs(flows["START_TIME"] - ref["START_TIME"]) + abs(flows["END_TIME"] - ref["END_TIME"])

            # try to find the exact flow
            flow = flows[
                (flows.TIME_DIFF == flows.TIME_DIFF.min()) & (flows.PACKETS == ref.PACKETS) & (flows.BYTES == ref.BYTES)
            ]
            if len(flow.index) == 0:
                # find the flow which is closest in time
                flow = flows[flows.TIME_DIFF == flows.TIME_DIFF.min()]
                if len(flow.index) == 0:
                    self._report.add_test(PMTestCategory.MISSING, PMFlow(**(ref.to_dict())))
                    continue

            flow = flow.iloc[0]
            if flow["TIME_DIFF"] > self._max_time_diff:
                self._report.add_test(PMTestCategory.MISSING, PMFlow(**(ref.to_dict())))
                continue

            if flow["PACKETS"] == ref["PACKETS"] and flow["BYTES"] == ref["BYTES"]:
                # if the sizes are correct, we should expect times to be also correct
                if flow["TIME_DIFF"] > self._ok_time_diff:
                    self._report.add_test(PMTestCategory.SHIFTED, PMFlow(**(flow.to_dict())), PMFlow(**(ref.to_dict())))
            else:
                # counters differ, timestamps are probably skewed, report only counters
                self._report.add_test(PMTestCategory.SCALED, PMFlow(**(flow.to_dict())), PMFlow(**(ref.to_dict())))

            flows.drop(index=flow.name, inplace=True)

        self._report_flows(flows, PMTestCategory.UNEXPECTED)

    def _report_flows(self, flows: "pandas.DataFrame", category: PMTestCategory) -> None:
        """Report all flows to a category.

        Parameters
        ----------
        flows : pandas.DataFrame
            Flows or reference flows.
        category : PMTestCategory
            The test category the flows should be reported to (either MISSING or UNEXPECTED).
        """

        for _, flow in flows.iterrows():
            self._report.add_test(category, PMFlow(**(flow.to_dict())))
