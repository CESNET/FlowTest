"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

FlowReplicator tool. Tool is used to replicate flows in reference CSV file, which is necessary
in case a replicator (ft-replay) was used as a generator during testing.
"""


import ipaddress
import re
from dataclasses import dataclass
from typing import List, Optional

import numpy as np
import pandas as pd


class FlowReplicatorException(Exception):
    """General exception used in flow replicator."""


@dataclass
class IpAddConstant:
    """Ip address modifier. Result of parsing of addConstant(number) or addOffset(number).

    Attributes
    ----------
    value : int
        Parameter of modifier function. Constant value.
    """

    value: int


@dataclass
class ReplicatorUnit:
    """Representation of single replication unit. Source IP or destination IP can be changed with modifier.

    Attributes
    ----------
    srcip : IpAddConstant, optional
        Source IP modifier. If None, no changes during replication.
    dstip : IpAddConstant, optional
        Destination IP modifier. If None, no changes during replication.
    """

    srcip: Optional[IpAddConstant]
    dstip: Optional[IpAddConstant]


@dataclass
class ReplicatorConfig:
    """Representation of replicator configuration. Parsed from dict (ft-replay like) representation.

    Attributes
    ----------
    units : list
        List of replication units.
        In each loop, all replication units take the source flows and replicate (copy and edit) them.
    loop : list
        Defines behavior of IP addresses modifying in loops. An IP offset can be set to provide IP subnet separation.
    """

    units: List[ReplicatorUnit]
    loop: ReplicatorUnit


# pylint: disable=too-few-public-methods
class FlowReplicator:
    """FlowReplicator tool. Tool is used to replicate flows in reference CSV file, which is necessary in case
    a replicator (ft-replay) was used as a generator during testing.

    Data source must be CSV files with the following columns (order of columns does not matter):
        START_TIME: time of the first observed packet in the flow (UTC timestamp in milliseconds)
        END_TIME: time of the last observed packet in the flow (UTC timestamp in milliseconds)
        PROTOCOL: protocol number defined by IANA
        SRC_IP: source IP address (IPv4 or IPv6)
        DST_IP: destination IP address (IPv4 or IPv6)
        SRC_PORT: source port number (can be 0 if the flow does not contain TCP or UDP protocol)
        DST_PORT: destination port number (can be 0 if the flow does not contain TCP or UDP protocol)
        PACKETS: number of transferred packets
        BYTES: number of transferred bytes (IP headers + payload)

    Replicator automatically merges flows that have the same flow key within single replay loop. This behavior occurs
    when multiple replication units do not affect the source or destination ip address.

    Replicator is able to merge flows with the same flow key across replay loops. This feature must be enabled with
    parameter 'merge_across_loops'. Merging takes into account inactive timeout: when gap between end and start of two
    flows with the same flow key is greater or equal than inactive timeout, flows are left unmerged as well as in export
    from a probe.

    "addConstant(number)" and "addOffset(number)" IP modifiers are supported. "addCounter" ft-replay modifier is
    unsupported because of nondeterministic IP address distribution to replication workers/threads.

    Attributes
    ----------
    _config : ReplicatorConfig
        Replicator configuration - ft-replay style.
    _flows : pd.DataFrame
        Source (original) flow records.
    _inactive_timeout : int or None
        Probe inactive timeout in milliseconds. Used when merging across loops.
    """

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

    def __init__(self, config: dict) -> None:
        """Init flow replicator. Parse config dict.

        Parameters
        ----------
        config : dict
            Configuration in form of dict, the same as ft-replay configuration.

        Raises
        ------
        FlowReplicatorException
            When bad config format or unsupported replication modifier is used.
        """

        self._config = self._normalize_config(config)
        self._flows = None
        self._inactive_timeout = None

    def replicate(
        self,
        input_file: str,
        output_file: str,
        loops: int,
        merge_across_loops: bool = False,
        inactive_timeout: int = -1,
    ) -> None:
        """Read source data and replicate source flows based on configuration.
        Save replication result to CSV file. Helper columns like "ORIG_INDEX" are not exported.

        Parameters
        ----------
        input_file : str
            Path to CSV file with source flow records.
        output_file : str
            Path to output CSV file to save replicated flows.
        loops : int
            Number of replay loops.
        merge_across_loops : bool, optional
            Set to true when flows are to be merged across loops.
            Feature description is provided in FlowReplicator docstring.
        inactive_timeout : int, optional
            Probe inactive timeout in seconds. Time after which inactive flow is marked as ended.
            Ignored during merge if the value is -1.

        Raises
        ------
        FlowReplicatorException
            When source CSV file cannot be read.
        """

        try:
            self._flows = pd.read_csv(input_file, engine="pyarrow", dtype=self.CSV_COLUMN_TYPES)
        except Exception as err:
            raise FlowReplicatorException("Unable to read file with flows.") from err

        self._flows.loc[:, "SRC_IP"] = self._flows["SRC_IP"].apply(ipaddress.ip_address)
        self._flows.loc[:, "DST_IP"] = self._flows["DST_IP"].apply(ipaddress.ip_address)
        # index of source flow is used when merging flows within single loop
        self._flows["ORIG_INDEX"] = self._flows.index

        # replicate and drop original record indexes - deduplicate
        result = self._replicate(loops)
        result.reset_index(drop=True, inplace=True)
        result.reindex()

        if merge_across_loops:
            self._inactive_timeout = inactive_timeout * 1000 if inactive_timeout > -1 else None
            result = self._merge_across_loop(result)

        result.loc[:, self.CSV_COLUMN_TYPES.keys()].to_csv(output_file, index=False)

    @staticmethod
    def _parse_config_item(item: str, src_dict: dict) -> Optional[IpAddConstant]:
        """Parse single modifier in configuration. In replication unit section or loop section.

        Parameters
        ----------
        item : str
            Name of parsed item, e.g. "srcip" or "dstip".
        src_dict : dict
            Nested dict in which the item is parsed.

        Returns
        -------
        IpAddConstant or None
            Parsed modifier if found in src_dict. Otherwise None.

        Raises
        ------
        FlowReplicatorException
            When modifier function is not supported by flow replicator.
        """

        if item in src_dict and src_dict[item] != "None":
            func = src_dict[item]
        else:
            return None

        if func.startswith("addConstant"):
            return IpAddConstant(int(re.findall(r"\d+", func)[0]))
        if func.startswith("addOffset"):
            return IpAddConstant(int(re.findall(r"\d+", func)[0]))

        raise FlowReplicatorException(
            f"Value '{func}' in replicator configuration is not supported by flow replicator (ft-analyzer)."
        )

    def _normalize_config(self, config: dict) -> ReplicatorConfig:
        """Parse, check and get replicator configuration in ReplicatorConfig representation.

        Parameters
        ----------
        config : dict
            Dictionary with "units" and "loop" configuration (ft-replay style).

        Returns
        -------
        ReplicatorConfig
            Parsed replicator configuration.

        Raises
        ------
        FlowReplicatorException
            When configuration dict has bad format.
        """

        if not set(config.keys()).issubset({"units", "loop"}):
            raise FlowReplicatorException("Only 'units' and 'loop' keys are allowed in replicator configuration.")

        units = []
        for unit in config.get("units", []):
            units.append(ReplicatorUnit(self._parse_config_item("srcip", unit), self._parse_config_item("dstip", unit)))

        loop_config = config.get("loop", {})
        loop = ReplicatorUnit(
            self._parse_config_item("srcip", loop_config), self._parse_config_item("dstip", loop_config)
        )

        return ReplicatorConfig(units, loop)

    def _replicate(self, loops: int) -> pd.DataFrame:
        """Replicate flows from source according to the configuration.

        Parameters
        ----------
        loops : int
            Number of replay loops.

        Returns
        -------
        pd.DataFrame
            Replicated flows.
        """

        loop_start = self._flows.loc[:, "START_TIME"].min()
        loop_end = self._flows.loc[:, "END_TIME"].max()
        loop_length = int(loop_end - loop_start)

        res = pd.DataFrame()
        for loop_n in range(loops):
            loop_flows = self._process_single_loop(loop_n, loop_length)
            res = pd.concat([res, loop_flows], axis=0)

        return res

    def _process_single_loop(self, loop_n: int, loop_length: int) -> pd.DataFrame:
        """Replicate flows for single loop. Copy, add time offset to timestamps and replicate with units.

        Parameters
        ----------
        loop_n : int
            Sequence number of loop.
        loop_length : int
            Duration of one loop in milliseconds.

        Returns
        -------
        pd.DataFrame
            Replicated flows (deep copy).
        """

        time_offset = loop_n * loop_length
        srcip_offset = 0
        dstip_offset = 0
        if self._config.loop.srcip:
            srcip_offset += loop_n * self._config.loop.srcip.value
        if self._config.loop.dstip:
            dstip_offset += loop_n * self._config.loop.dstip.value

        flows = self._flows.copy()
        flows["START_TIME"] = flows["START_TIME"] + time_offset
        flows["END_TIME"] = flows["END_TIME"] + time_offset
        flows["SRC_IP"] = flows["SRC_IP"] + srcip_offset
        flows["DST_IP"] = flows["DST_IP"] + dstip_offset

        res = [self._process_replication_unit(unit, flows) for unit in self._config.units]
        res = pd.concat(res, axis=0)

        # merge replicated flows with the same key within one loop
        # (when replication unit does not change src nor dst ip)
        # ORIG_INDEX - leave flows that are separated in input csv unmerged - expectation of correct reference
        # (e.g. two flows with the same flow key at different times)
        key = self.FLOW_KEY + ["ORIG_INDEX"]
        res = res.groupby(key, sort=False).agg(self.AGGREGATE_SPLIT_FLOWS).reset_index()
        res.reindex()
        res.sort_values(by=["ORIG_INDEX"], inplace=True)

        return res

    def _process_replication_unit(self, unit: ReplicatorUnit, orig_flows: pd.DataFrame) -> pd.DataFrame:
        """Replicate flows by single replication unit within single loop.

        Parameters
        ----------
        unit : ReplicatorUnit
            Configuration of replication unit.
        orig_flows : pd.DataFrame
            Source flows with corrected timestamps and IP addresses according to the loop being processed.

        Returns
        -------
        pd.DataFrame
            Replicated flows (deep copy).
        """

        flows = orig_flows.copy()
        if unit.srcip:
            flows["SRC_IP"] = flows["SRC_IP"] + unit.srcip.value
        if unit.dstip:
            flows["DST_IP"] = flows["DST_IP"] + unit.dstip.value
        return flows

    def _merge_func(self, group: pd.DataFrame) -> pd.DataFrame:
        """Helper function used on group to merge flows across loops.

        Parameters
        ----------
        group : pd.DataFrame
            Grouped flows by flow key.

        Returns
        -------
        pd.DataFrame
            Merged flows.
        """

        # check if group has more than 1 flow (row)
        if group.shape[0] > 1:
            # drop original index, index within a group from 0
            group.reset_index(drop=True, inplace=True)
            group.reindex()

            # create column with start time of following flow (shift by 1)
            next_start = group["START_TIME"][1:]
            next_start.reset_index(drop=True, inplace=True)
            next_start.reindex()

            group["GAP"] = next_start - group["END_TIME"]
            group["AGGR_NO"] = 0

            if self._inactive_timeout:
                # split merge group when gap between flows are greater or equal inactive timeout
                aggr_no = 0
                for index, row in group.iterrows():
                    group.at[index, "AGGR_NO"] = aggr_no
                    if row["GAP"] >= self._inactive_timeout:
                        aggr_no += 1

            res_group = group.groupby(self.FLOW_KEY + ["AGGR_NO"]).aggregate(self.AGGREGATE_SPLIT_FLOWS).reset_index()
            res_group.reindex()
            return res_group
        return group

    def _merge_across_loop(self, flows: pd.DataFrame) -> pd.DataFrame:
        """Merge replicated flows across loops.
        Feature description is provided in FlowReplicator docstring.

        Warning: merging does not take ORIG_INDEX into account, so if the source contains multiple flow records with
        the same flow key, the records will be merged.

        Parameters
        ----------
        flows : pd.DataFrame
            Replicated flows to be merged.

        Returns
        -------
        pd.DataFrame
            Merged flows.
        """

        return flows.groupby(self.FLOW_KEY).apply(self._merge_func)
