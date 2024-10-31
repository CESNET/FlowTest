"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2024 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Extend profile with IP addresses according to defined probabilities.
"""

import ipaddress
import random
from typing import Union

import numpy as np
import pandas as pd

NEAR_TO_ZERO = 0.000001


class ProfileEnhancerError(Exception):
    """Error while profile enhancing."""


# pylint: disable=too-few-public-methods
class ProfileEnhancer:
    """Extend profile with IP addresses according to defined probabilities."""

    CSV_COLUMN_TYPES = {
        "START_TIME": np.uint64,
        "END_TIME": np.uint64,
        "L3_PROTO": np.uint8,
        "L4_PROTO": np.uint8,
        "SRC_PORT": np.uint16,
        "DST_PORT": np.uint16,
        "PACKETS": np.uint64,
        "BYTES": np.uint64,
        "PACKETS_REV": np.uint64,
        "BYTES_REV": np.uint64,
    }

    def _process_ranges(
        self,
        ranges: list[str],
    ) -> tuple[list[Union[ipaddress.IPv4Network, ipaddress.IPv6Network]], list[float]]:
        """Parse IP ranges with probability number.

        Parameters
        ----------
        ranges : list[str]
            List of IPv4 and IPv6 ranges in string format.

        Returns
        -------
        tuple[list[Union[ipaddress.IPv4Network, ipaddress.IPv6Network]], list[float]]
            Tuple of parsed IP ranges and their probabilities.

        Raises
        ------
        ProfileEnhancerError
            When probabilities don't sum to 100% or probability is written in bad form (bad input).
        """

        res_ranges = []
        probabilities = []

        for ip_range in ranges:
            parts = ip_range.split(maxsplit=1)
            ip = ipaddress.ip_network(parts[0])
            if len(parts) > 1:
                prob = parts[1]
                if prob[-1] == "%":
                    prob = int(prob[:-1]) / 100
                else:
                    raise ProfileEnhancerError("IP range probability must be written in form {number}%, e.q. 50%.")
                res_ranges.append(ip)
                probabilities.append(prob)
            else:
                res_ranges.append(ip)
                probabilities.append(None)

        nones = sum(p is None for p in probabilities)
        if nones > 0:
            total = sum(p for p in probabilities if p is not None)
            rest = (1 - total) / nones
            probabilities = [rest if p is None else p for p in probabilities]

        if abs(sum(probabilities) - 1) > NEAR_TO_ZERO:
            raise ProfileEnhancerError("Probabilities don't sum to 100%.")

        return res_ranges, probabilities

    @staticmethod
    def _random_ip(
        network: Union[ipaddress.IPv4Network, ipaddress.IPv6Network]
    ) -> Union[ipaddress.IPv4Address, ipaddress.IPv6Address]:
        """Generate random IP address from IP network range.

        Parameters
        ----------
        network : Union[ipaddress.IPv4Network, ipaddress.IPv6Network]
            IP network to generate from.

        Returns
        -------
        Union[ipaddress.IPv4Address, ipaddress.IPv6Address]
            Generated IPv4 or IPv6 address.
        """

        network_int = int(network.network_address)
        rand_bits = network.max_prefixlen - network.prefixlen
        rand_host_int = random.randint(0, 2**rand_bits - 1)
        if isinstance(network, ipaddress.IPv4Network):
            ip_address = ipaddress.IPv4Address(network_int + rand_host_int)
        else:
            ip_address = ipaddress.IPv6Address(network_int + rand_host_int)
        return ip_address

    @staticmethod
    def _flow_key(
        row: pd.DataFrame,
    ) -> tuple[
        Union[ipaddress.IPv4Address, ipaddress.IPv6Address],
        Union[ipaddress.IPv4Address, ipaddress.IPv6Address],
        int,
        int,
        int,
    ]:
        """Get flow key from DataFrame row.

        Parameters
        ----------
        row : pd.DataFrame
            Row of profile file.

        Returns
        -------
        tuple[ipaddress, ipaddress, int, int, int]
            Flow key
        """

        if row["SRC_IP"] < row["DST_IP"] or (row["SRC_IP"] == row["DST_IP"] and row["SRC_PORT"] < row["DST_PORT"]):
            src_ip = row["SRC_IP"]
            dst_ip = row["DST_IP"]
            src_port = row["SRC_PORT"]
            dst_port = row["DST_PORT"]
        else:
            src_ip = row["DST_IP"]
            dst_ip = row["SRC_IP"]
            src_port = row["DST_PORT"]
            dst_port = row["SRC_PORT"]
        return (src_ip, dst_ip, src_port, dst_port, row["L4_PROTO"])

    def _enhance_flows_by_ip_version(
        self,
        flows: pd.DataFrame,
        ip_version: int,
        ranges: list[Union[ipaddress.IPv4Network, ipaddress.IPv6Network]],
        probabilities: list[float],
    ) -> None:
        """Enhance flow records with random generated IP addresses in part filtered by L3 proto.

        Parameters
        ----------
        flows : pd.DataFrame
            Network profile in DataFrame.
        ip_version : int
            Number of IP version.
        ranges : list[Union[ipaddress.IPv4Network, ipaddress.IPv6Network]]
            List of ranges from generate IP addresses.
        probabilities : list[float]
            List of probabilities of IP ranges (distribution function).

        Raises
        ------
        ProfileEnhancerError
            When cannot be generated flow with unique flow key.
        """

        flow_keys_set = set()

        def _generate_addresses(row: pd.DataFrame) -> pd.DataFrame:
            """Apply function for generate SRC and DST IP address."""

            range_idx = int(row["_GENERATOR_IDX"])

            if ranges[range_idx].num_addresses == 1:
                # special case for single IP address range, one of the SRC_IP or DST_IP is totally random
                if ranges[range_idx].version == 4:
                    random_range = ipaddress.ip_network("0.0.0.0/0")
                else:
                    random_range = ipaddress.ip_network("0::/0")
                if random.choice([0, 1]) == 0:
                    src_range = ranges[range_idx]
                    dst_range = random_range
                else:
                    src_range = random_range
                    dst_range = ranges[range_idx]
            else:
                src_range = ranges[range_idx]
                dst_range = ranges[range_idx]

            for _ in range(10):
                row["SRC_IP"] = self._random_ip(src_range)
                row["DST_IP"] = self._random_ip(dst_range)

                flow_key = self._flow_key(row)
                if flow_key not in flow_keys_set:
                    flow_keys_set.add(flow_key)
                    return row

            raise ProfileEnhancerError(
                f"Unique combination of SRC_IP and DST_IP not found. Too small range {ranges[range_idx]}?"
            )

        mask = flows["L3_PROTO"] == ip_version
        flows.loc[mask, "_GENERATOR_IDX"] = np.random.choice(
            range(len(ranges)), flows.loc[mask].shape[0], p=probabilities
        )
        flows.loc[mask] = flows.loc[mask].astype("O").apply(_generate_addresses, axis=1)

    def enhance(self, profile_path: str, profile_path_out: str, ipv4_ranges: list[str], ipv6_ranges: list[str]) -> None:
        """Enhance flow records with random generated IP addresses.

        Parameters
        ----------
        profile_path : str
            Path to CSV file with network profile.
        profile_path_out : str
            Path to CSV file to save enhanced profile.
        ipv4_ranges : list[str]
            IPv4 ranges in form "<ip_range> <probability>%" (e.g.: "10.0.0.1/8 23%").
            The IP range without probability is treated as a residual.
        ipv6_ranges : list[str]
            IPv6 ranges in form "<ip_range> <probability>%" (e.g.: "2001:db8::/64 46%").
            The IP range without probability is treated as a residual.

        Raises
        ------
        ProfileEnhancerError
            Cannot open CSV input file.
        """

        try:
            flows = pd.read_csv(profile_path, engine="pyarrow", dtype=self.CSV_COLUMN_TYPES)
        except Exception as err:
            raise ProfileEnhancerError("Unable to read profile CSV.") from err

        # helper column to select random generator
        flows["_GENERATOR_IDX"] = 0
        flows["SRC_IP"] = ipaddress.IPv4Address(0)
        flows["DST_IP"] = ipaddress.IPv4Address(0)

        ranges, probabilities = self._process_ranges(ipv4_ranges)
        self._enhance_flows_by_ip_version(flows, 4, ranges, probabilities)

        ranges, probabilities = self._process_ranges(ipv6_ranges)
        self._enhance_flows_by_ip_version(flows, 6, ranges, probabilities)

        export_columns = list(self.CSV_COLUMN_TYPES.keys()) + ["SRC_IP", "DST_IP"]
        flows.loc[:, export_columns].to_csv(profile_path_out, index=False)
