"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""
import ipaddress
from typing import List, Optional, Tuple, Union

import numpy as np
import pandas as pd
from ftanalyzer.models.sm_data_types import (
    SMException,
    SMMetricType,
    SMRule,
    SMSubnetSegment,
    SMTestOutcome,
    SMTimeSegment,
)
from ftanalyzer.reports import StatisticalReport


class StatisticalModel:
    """Statistical model reads flows obtained from a network probe and compares them with a provided reference.

    Both data sources must be CSV files with the following columns (order of columns does not matter):
        START_TIME: time of the first observed packet in the flow (UTC timestamp in milliseconds)
        END_TIME: time of the last observed packet in the flow (UTC timestamp in milliseconds)
        PROTOCOL: protocol number defined by IANA
        SRC_IP: source IP address (IPv4 or IPv6)
        DST_IP: destination IP address (IPv4 or IPv6)
        SRC_PORT: source port number (can be 0 if the flow does not contain TCP or UDP protocol)
        DST_PORT: destination port number (can be 0 if the flow does not contain TCP or UDP protocol)
        PACKETS: number of transferred packets
        BYTES: number of transferred bytes (IP headers + payload)

    Statistical model is able to merge flows which were split by active timeout configured on the network probe.
    The model is able to perform statistical analysis of the provided data to determine how much it differs from the
    reference. Every analysis can be done either with the whole data or with a specific subset (called "segment")
    which can be specified either by IP subnets or time intervals.

    Attributes
    ----------
    _flows : pandas.DataFrame
        Flow records acquired from a network probe.
    _ref : pandas.DataFrame
        Flow records acting as a reference.
    """

    # pylint: disable=too-few-public-methods
    TIME_EPSILON = 100
    FLOW_KEY = ["PROTOCOL", "SRC_IP", "DST_IP", "SRC_PORT", "DST_PORT"]
    CSV_COLUMN_TYPES = {
        "START_TIME": np.uint64,
        "END_TIME": np.uint64,
        "PROTOCOL": np.uint8,
        "SRC_IP": str,
        "DST_IP": str,
        "SRC_PORT": np.uint16,
        "DST_PORT": np.uint16,
        "PACKETS": np.uint64,
        "BYTES": np.uint64,
    }

    AGGREGATE_SPLIT_FLOWS = {
        "START_TIME": "min",
        "END_TIME": "max",
        "PACKETS": "sum",
        "BYTES": "sum",
    }

    def __init__(self, flows: str, reference: str, timeouts: Tuple[int, int], start_time: int = 0) -> None:
        """Read provided files and converts it to data frames.

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

        try:
            self._flows = pd.read_csv(flows, engine="pyarrow", dtype=self.CSV_COLUMN_TYPES)
            self._ref = pd.read_csv(reference, engine="pyarrow", dtype=self.CSV_COLUMN_TYPES)
        except Exception as err:
            raise SMException("Unable to read file with flows.") from err

        if start_time > 0:
            self._ref["START_TIME"] = self._ref["START_TIME"] + start_time
            self._ref["END_TIME"] = self._ref["END_TIME"] + start_time

        self._merge_flows(timeouts[0])

        self._flows.loc[:, "SRC_IP"] = self._flows["SRC_IP"].apply(ipaddress.ip_address)
        self._flows.loc[:, "DST_IP"] = self._flows["DST_IP"].apply(ipaddress.ip_address)
        self._ref.loc[:, "SRC_IP"] = self._ref["SRC_IP"].apply(ipaddress.ip_address)
        self._ref.loc[:, "DST_IP"] = self._ref["DST_IP"].apply(ipaddress.ip_address)

    def validate(self, rules: List[SMRule]) -> StatisticalReport:
        """Evaluate data in the statistical model based on the provided evaluation rules.

        Parameters
        ----------
        rules : list
            Evaluation rules which are used for the evaluation.

        Returns
        ------
        StatisticalReport
            Report containing results of individual performed tests.

        Raises
        ------
        SMException
            When duplicated metrics in a single validation rule are present.
        """

        report = StatisticalReport()
        for rule in rules:
            flows, ref = self._filter_segment(rule.segment)

            # Check duplicated metrics.
            if len({m.key for m in rule.metrics}) != len(rule.metrics):
                raise SMException(f"Rule contains duplicated metrics: {rule.metrics}")

            for metric in rule.metrics:
                if metric.key == SMMetricType.FLOWS:
                    value = len(flows.index)
                    reference = len(ref.index)
                else:
                    value = flows[metric.key.value].sum()
                    reference = ref[metric.key.value].sum()

                report.add_test(
                    SMTestOutcome(metric, rule.segment, value, reference, abs(value - reference) / reference)
                )

        return report

    def _merge_flows(self, active_t: int) -> None:
        """Merge flows acquired from the probe which were split due to active timeout.

        Find flows in the reference which are longer than the active timeout and attempt to
        find its parts in the flows acquired from the probe.

        Parameters
        ----------
        active_t : int
            Active timeout.
        """

        long_flows = self._ref.loc[(self._ref["END_TIME"] - self._ref["START_TIME"]) > (active_t * 1000)]
        for _, row in long_flows.iterrows():
            merge = self._flows.loc[
                (self._flows["PROTOCOL"] == row["PROTOCOL"])
                & (self._flows["SRC_IP"] == row["SRC_IP"])
                & (self._flows["DST_IP"] == row["DST_IP"])
                & (self._flows["SRC_PORT"] == row["SRC_PORT"])
                & (self._flows["DST_PORT"] == row["DST_PORT"])
                & (self._flows["START_TIME"] >= row["START_TIME"])
                & (self._flows["END_TIME"] <= row["END_TIME"] + self.TIME_EPSILON)
            ]
            self._flows.drop(merge.index, axis=0, inplace=True)
            agg = merge.groupby(self.FLOW_KEY).agg(self.AGGREGATE_SPLIT_FLOWS).reset_index()
            self._flows = pd.concat([self._flows, agg], axis=0, ignore_index=True)

        self._flows.reindex()

    def _filter_segment(
        self, segment: Optional[Union[SMSubnetSegment, SMTimeSegment]]
    ) -> Tuple[pd.DataFrame, pd.DataFrame]:
        """Create subsets of data frames based on the provided segment.

        Parameters
        ----------
        segment : SMSubnetSegment, SMTimeSegment, None
            Segment to be used to create subsets.

        Returns
        ------
        tuple
            subset of flows acquired from the probe, subset of reference flows
        """

        if isinstance(segment, SMSubnetSegment):
            return self._filter_subnet_segment(segment)

        if isinstance(segment, SMTimeSegment):
            return self._filter_time_segment(segment)

        assert segment is None
        return self._flows, self._ref

    def _filter_subnet_segment(self, segment: SMSubnetSegment) -> Tuple[pd.DataFrame, pd.DataFrame]:
        """Create subsets of data frames based on subnets.

        Parameters
        ----------
        segment : SMSubnetSegment
            Segment to be used to create subsets.

        Returns
        ------
        tuple
            subset of flows acquired from the probe, subset of reference flows
        """

        subnet_source = ipaddress.ip_network(segment.source) if segment.source is not None else None
        subnet_dest = ipaddress.ip_network(segment.dest) if segment.dest is not None else None

        if subnet_source is not None and subnet_dest is not None:
            if segment.bidir:
                mask_flow = (
                    self._flows["SRC_IP"].apply(lambda x: x in subnet_source)
                    & self._flows["DST_IP"].apply(lambda x: x in subnet_dest)
                ) | (
                    self._flows["SRC_IP"].apply(lambda x: x in subnet_dest)
                    & self._flows["DST_IP"].apply(lambda x: x in subnet_source)
                )
                mask_ref = (
                    self._ref["SRC_IP"].apply(lambda x: x in subnet_source)
                    & self._ref["DST_IP"].apply(lambda x: x in subnet_dest)
                ) | (
                    self._ref["SRC_IP"].apply(lambda x: x in subnet_dest)
                    & self._ref["DST_IP"].apply(lambda x: x in subnet_source)
                )
            else:
                mask_flow = self._flows["SRC_IP"].apply(lambda x: x in subnet_source) & self._flows["DST_IP"].apply(
                    lambda x: x in subnet_dest
                )
                mask_ref = self._ref["SRC_IP"].apply(lambda x: x in subnet_source) & self._ref["DST_IP"].apply(
                    lambda x: x in subnet_dest
                )
        elif subnet_source is not None:
            if segment.bidir:
                mask_flow = self._flows["SRC_IP"].apply(lambda x: x in subnet_source) | self._flows["DST_IP"].apply(
                    lambda x: x in subnet_source
                )
                mask_ref = self._ref["SRC_IP"].apply(lambda x: x in subnet_source) | self._ref["DST_IP"].apply(
                    lambda x: x in subnet_source
                )
            else:
                mask_flow = self._flows["SRC_IP"].apply(lambda x: x in subnet_source)
                mask_ref = self._ref["SRC_IP"].apply(lambda x: x in subnet_source)
        else:
            if segment.bidir:
                mask_flow = self._flows["SRC_IP"].apply(lambda x: x in subnet_dest) | self._flows["DST_IP"].apply(
                    lambda x: x in subnet_dest
                )
                mask_ref = self._ref["SRC_IP"].apply(lambda x: x in subnet_dest) | self._ref["DST_IP"].apply(
                    lambda x: x in subnet_dest
                )
            else:
                mask_flow = self._flows["DST_IP"].apply(lambda x: x in subnet_dest)
                mask_ref = self._ref["DST_IP"].apply(lambda x: x in subnet_dest)

        return self._flows[mask_flow], self._ref[mask_ref]

    def _filter_time_segment(self, segment: SMTimeSegment) -> Tuple[pd.DataFrame, pd.DataFrame]:
        """Create subsets of data frames based on time interval.

        Parameters
        ----------
        segment : SMTimeSegment
            Segment to be used to create subsets.

        Returns
        ------
        tuple
            subset of flows acquired from the probe, subset of reference flows
        """

        start_time = end_time = None
        if segment.start is not None:
            start_time = int(segment.start.timestamp() * 1000)

        if segment.end is not None:
            end_time = int(segment.end.timestamp() * 1000)

        if start_time is not None and end_time is not None:
            mask_flow = self._flows["START_TIME"].apply(lambda x: x >= start_time) & self._flows["END_TIME"].apply(
                lambda x: x <= end_time
            )
            mask_ref = self._ref["START_TIME"].apply(lambda x: x >= start_time) & self._ref["END_TIME"].apply(
                lambda x: x <= end_time
            )
        elif start_time is not None:
            mask_flow = self._flows["START_TIME"].apply(lambda x: x >= start_time)
            mask_ref = self._ref["START_TIME"].apply(lambda x: x >= start_time)
        else:
            mask_flow = self._flows["END_TIME"].apply(lambda x: x <= end_time)
            mask_ref = self._ref["END_TIME"].apply(lambda x: x <= end_time)

        return self._flows[mask_flow], self._ref[mask_ref]
