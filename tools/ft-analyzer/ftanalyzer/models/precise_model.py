"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""
from typing import Optional, Union

import pandas as pd
from ftanalyzer.models.sm_data_types import SMSubnetSegment, SMTimeSegment
from ftanalyzer.models.statistical_model import StatisticalModel
from ftanalyzer.reports.precise_report import PMFlow, PMTestCategory, PreciseReport


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
        * flows which consists of incorrect number of partial flows

    The format of the input data is the same as for the statistical model.
    Subnet and time segments behave the same as in the statistical model.
    IMPORTANT: requires flow keys in reference data to be unique.

    Attributes
    ----------
    _report : PreciseReport
        Report which is created from discovered flow differences.
    _active_timeout : int
        Maximum duration of a flow (in milliseconds) which was configured on the network
        probe during the monitoring period.
    """

    DEFAULT_OK_TIME_DIFF = 100

    def __init__(
        self,
        flows: str,
        reference: str,
        active_timeout: int,
        start_time: int = 0,
        biflows_ts_correction: bool = False,
    ) -> None:
        """Initialize the statistical model with sorted input data.
        Parameters
        ----------
        flows : str
            Path to a CSV containing flow records acquired from a network probe.
        reference : str
            Path to a CSV containing flow records acting as a reference.
        active_timeout : int
            Maximum duration of a flow (in seconds) which was configured on the network
            probe during the monitoring period.
        start_time : int
            Treat times in the reference file as offsets (in milliseconds) from the provided start time.
            UTC timestamp in milliseconds.
        biflows_ts_correction : bool
            Value should be True when probe exporting biflows.
            Timestamps in reverse direction flows is corrected.

        Raises
        ------
        SMException
            Unable to process provided files.
        """

        super().__init__(flows, reference, start_time, merge=True, biflows_ts_correction=biflows_ts_correction)
        self._report = None
        self._active_timeout = active_timeout * 1000

    def validate_precise(
        self,
        segments: Optional[list[Union[SMSubnetSegment, SMTimeSegment]]] = None,
        ok_time_diff: int = DEFAULT_OK_TIME_DIFF,
    ) -> PreciseReport:
        """Evaluate data in the precise model for each of the provided segments.

        Detects:
            * missing flows
            * unexpected flows
            * incorrect timestamps
            * incorrect values of packets and bytes
            * flows which consists of incorrect number of partial flows

        Parameters
        ----------
        segments : list, None
            Segments for which the evaluation should be performed.
            If None, the evaluation is performed for all data.
        ok_time_diff : int
            Maximum difference between timestamps of a flow and its reference flow which is not reported (in ms).

        Returns
        ------
        PreciseReport
            Report containing results of individual performed tests.
        """

        self._report = PreciseReport()
        if segments is None:
            segments = [None]

        for segment in segments:
            self._report.add_segment(segment)
            flows, refs = self._filter_segment(segment)

            # perform outer join thank to which we can easily identify flows
            # which are in one frame and not the other using '_merge' column
            # store original indexes using 'reset_index()'
            # integer values are cast to float because of the introduction of NaN values
            combined = pd.merge(flows.reset_index(), refs.reset_index(), on=self.FLOW_KEY, how="outer", indicator=True)

            missing = combined[combined["_merge"] == "right_only"]
            self._report_flows(refs.iloc[missing["index_y"]], PMTestCategory.MISSING)

            unexpected = combined[combined["_merge"] == "left_only"]
            self._report_flows(flows.iloc[unexpected["index_x"]], PMTestCategory.UNEXPECTED)

            # filter flows present in both frames and change type of saved indexes back to uint64
            combined = combined[combined["_merge"] == "both"]
            combined["index_x"] = combined["index_x"].astype("uint64")
            combined["index_y"] = combined["index_y"].astype("uint64")

            # add time difference column for later use
            combined["TIME_DIFF"] = combined.apply(self._time_diff, axis=1)

            combined = self._discard_correct_flows(combined, ok_time_diff)
            combined = self._report_scaled_flows(flows, refs, combined)
            combined = self._report_shifted_flows(flows, refs, combined, ok_time_diff)
            self._report_flows(
                flows.iloc[combined["index_x"]].sort_values(by=["FLOW_COUNT"], ascending=False), PMTestCategory.SPLIT
            )

        return self._report

    @staticmethod
    def _time_diff(flow: pd.Series) -> int:
        """Get the difference of the flow timestamps from the reference flow.

        Parameters
        ----------
        flow : pandas.Series
            Single flow record.

        Returns
        ------
        int
            Timestamp difference (in milliseconds).
        """

        start_time_diff = abs(flow["START_TIME_x"] - flow["START_TIME_y"])
        end_time_diff = abs(flow["END_TIME_x"] - flow["END_TIME_y"])
        return max(start_time_diff, end_time_diff)

    def _discard_correct_flows(self, frame: pd.DataFrame, ok_time_diff: int) -> pd.DataFrame:
        """Get dataframe containing only flows with errors.

        Parameters
        ----------
        frame : pandas.DataFrame
            Dataframe containing both data from the probe and from the reference merged on flow key.
        ok_time_diff : int
            Maximum difference between timestamps of a flow and its reference flow which is not reported (in ms).

        Returns
        ------
        pandas.DataFrame
            Dataframe with only error flows.
        """

        frame["FLOW_COUNT_ORIG"] = 1 + ((frame["END_TIME_y"] - frame["START_TIME_y"]) // (self._active_timeout + 1))

        correct = frame[
            (frame["TIME_DIFF"] <= ok_time_diff)
            & (frame["PACKETS_x"] == frame["PACKETS_y"])
            & (frame["BYTES_x"] == frame["BYTES_y"])
            & (frame["FLOW_COUNT"] == frame["FLOW_COUNT_ORIG"])
        ]

        return frame.drop(correct.index)

    def _report_scaled_flows(self, flows: pd.DataFrame, refs: pd.DataFrame, combined: pd.DataFrame) -> pd.DataFrame:
        """Report flows containing incorrect number of packets or bytes.

        Parameters
        ----------
        flows : pandas.DataFrame
            Original dataframe with probe flows.
        refs : pandas.DataFrame
            Original dataframe with reference flows.
        combined : pandas.DataFrame
            Dataframe containing both data from the probe and from the reference merged on flow key.

        Returns
        ------
        pandas.DataFrame
            Dataframe without flows with incorrect number of packets or bytes.
        """

        scaled = combined[
            (combined["PACKETS_x"] != combined["PACKETS_y"]) | (combined["BYTES_x"] != combined["BYTES_y"])
        ]
        for _, row in scaled.iterrows():
            self._report.add_test(
                PMTestCategory.SCALED,
                PMFlow(**(flows.iloc[row["index_x"]].to_dict())),
                PMFlow(**(refs.iloc[row["index_y"]].to_dict())),
            )

        return combined.drop(scaled.index)

    def _report_shifted_flows(
        self, flows: pd.DataFrame, refs: pd.DataFrame, combined: pd.DataFrame, ok_time_diff: int
    ) -> pd.DataFrame:
        """Report flows which timestamps differ from the reference.

        Parameters
        ----------
        flows : pandas.DataFrame
            Original dataframe with probe flows.
        refs : pandas.DataFrame
            Original dataframe with reference flows.
        combined : pandas.DataFrame
            Dataframe containing both data from the probe and from the reference merged on flow key.
        ok_time_diff : int
            Maximum difference between timestamps of a flow and its reference flow which is not reported (in ms).

        Returns
        ------
        pandas.DataFrame
            Dataframe without flows with problematic timestamps.
        """

        shifted = combined[combined["TIME_DIFF"] > ok_time_diff]

        for _, row in shifted.iterrows():
            flow = flows.iloc[row["index_x"]].to_dict()
            flow.update({"TIME_DIFF": row["TIME_DIFF"]})
            self._report.add_test(
                PMTestCategory.SHIFTED,
                PMFlow(**flow),
                PMFlow(**(refs.iloc[row["index_y"]].to_dict())),
            )

        return combined.drop(shifted.index)

    def _report_flows(self, flows: pd.DataFrame, category: PMTestCategory) -> None:
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
