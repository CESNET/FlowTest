"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""
import ipaddress
import logging
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
    FLOW_KEY = ["SRC_IP", "DST_IP", "SRC_PORT", "DST_PORT", "PROTOCOL"]
    CSV_COLUMN_TYPES = {
        "START_TIME": np.int64,
        "END_TIME": np.int64,
        "PROTOCOL": np.uint8,
        "SRC_IP": str,
        "DST_IP": str,
        "SRC_PORT": np.uint16,
        "DST_PORT": np.uint16,
        "PACKETS": np.uint64,
        "BYTES": np.uint64,
    }

    AGGREGATE_SPLIT_FLOWS = {
        "START_TIME_y": "min",
        "END_TIME_y": "max",
        "PACKETS_y": "sum",
        "BYTES_y": "sum",
    }

    def __init__(
        self, flows: str, reference: str, timeouts: Tuple[int, int], start_time: int = 0, sort: bool = False
    ) -> None:
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
        sort : bool
            Sort flows by the flow key (SRC_IP, DST_IP, SRC_PORT, DST_PORT, PROTOCOL).
            Necessary for the precise model, not needed for the statistical model.
            Must be done before IP addresses are converted to ipaddr objects.

        Raises
        ------
        SMException
            Unable to process provided files.
        """

        try:
            logging.getLogger().debug("reading file with flows=%s", flows)
            # ports could be empty in flows with protocol like ICMP
            flows = pd.read_csv(flows, engine="pyarrow")
            flows["SRC_PORT"] = flows["SRC_PORT"].fillna(0)
            flows["DST_PORT"] = flows["DST_PORT"].fillna(0)
            self._flows = flows.astype(self.CSV_COLUMN_TYPES)

            logging.getLogger().debug("reading file with references=%s", reference)
            self._ref = pd.read_csv(reference, engine="pyarrow", dtype=self.CSV_COLUMN_TYPES)
        except Exception as err:
            raise SMException("Unable to read file with flows.") from err

        if start_time > 0:
            self._ref["START_TIME"] = self._ref["START_TIME"] + start_time
            self._ref["END_TIME"] = self._ref["END_TIME"] + start_time

        self._merge_flows(timeouts[0])
        if sort:
            logging.getLogger().debug("sorting probe flows")
            self._flows = self._flows.sort_values(self.FLOW_KEY).reset_index(drop=True)
            logging.getLogger().debug("sorting reference flows")
            self._ref = self._ref.sort_values(self.FLOW_KEY).reset_index(drop=True)

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

        # find long flows among reference flows
        long_flows = self._ref.loc[(self._ref["END_TIME"] - self._ref["START_TIME"]) > (active_t * 1000)]
        # backup indexes of probe flows
        flows_with_index = self._flows.reset_index().rename(columns={"index": "index_y"})
        # perform inner join between long flows and probe flows
        combined_flows = pd.merge(long_flows, flows_with_index, on=self.FLOW_KEY, how="inner")
        # filter probe flows which start and end times fit into the frame of their long flow counterpart
        # thus finding flows which were split by an active timeout
        # columns with the same name which are not part of the key (PACKETS, BYTES, START_TIME, END_TIME)
        # are provided with a suffix (_x - columns from "long flows" and _y - columns from "flows_with_index")
        split_flows = combined_flows[
            (combined_flows["START_TIME_y"] >= combined_flows["START_TIME_x"])
            & (combined_flows["END_TIME_y"] <= combined_flows["END_TIME_x"] + self.TIME_EPSILON)
        ]
        logging.getLogger().debug("split flows aggregation - matched %d split flows", len(split_flows))

        # aggregate flows split by an active timeout (sum packets and bytes, minimum start time, maximum end time)
        aggregated_flows = (
            split_flows.groupby(self.FLOW_KEY + ["START_TIME_x", "END_TIME_x"])
            .agg(self.AGGREGATE_SPLIT_FLOWS)
            .reset_index()
            .rename(
                columns={
                    "START_TIME_y": "START_TIME",
                    "END_TIME_y": "END_TIME",
                    "PACKETS_y": "PACKETS",
                    "BYTES_y": "BYTES",
                }
            )
        )
        aggregated_flows.drop(["START_TIME_x", "END_TIME_x"], axis=1, inplace=True)
        logging.getLogger().debug(
            "split flows aggregation - found %d/%d long flows", len(aggregated_flows), len(long_flows)
        )

        # remove split flows from the probe flows (using original index backup)
        self._flows.drop(index=split_flows["index_y"].unique(), axis=0, inplace=True)
        # add aggregated flows to the probe flows
        self._flows = pd.concat([self._flows, aggregated_flows], axis=0, ignore_index=True)

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

        return self._flows[mask_flow].reset_index(drop=True), self._ref[mask_ref].reset_index(drop=True)

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

        return self._flows[mask_flow].reset_index(drop=True), self._ref[mask_ref].reset_index(drop=True)
