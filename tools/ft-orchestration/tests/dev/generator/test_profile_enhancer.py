"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2024 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Test ProfileEnhancer module.
"""

import ipaddress
import logging
import os
from collections import defaultdict

import numpy as np
import pandas as pd
import pytest
from src.generator.profile_enhancer import ProfileEnhancer

FILE_DIR = os.path.dirname(os.path.realpath(__file__))
INPUT_DIR = os.path.join(FILE_DIR, "input")

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
    "SRC_IP": str,
    "DST_IP": str,
}


def _analyze(
    output_file_path: str,
    ipv4_ranges: list[tuple[ipaddress.IPv4Network, float]],
    ipv6_ranges: list[tuple[ipaddress.IPv6Network, float]],
):
    """Function to analyze distribution of IP ranges."""

    hist_ipv4 = defaultdict(lambda: 0)
    hist_ipv4_bytes = defaultdict(lambda: 0)
    hist_ipv6 = defaultdict(lambda: 0)
    hist_ipv6_bytes = defaultdict(lambda: 0)

    try:
        flows = pd.read_csv(output_file_path, engine="pyarrow", dtype=CSV_COLUMN_TYPES)
    except Exception as err:
        raise RuntimeError("Unable to read profile CSV.") from err

    flows["SRC_IP"] = flows["SRC_IP"].apply(ipaddress.ip_address)
    flows["DST_IP"] = flows["DST_IP"].apply(ipaddress.ip_address)

    for _, row in flows.iterrows():
        for r in ipv4_ranges:
            if row["SRC_IP"] in r[0] or row["DST_IP"] in r[0]:
                hist_ipv4[r[0]] += 1
                hist_ipv4_bytes[r[0]] += row["BYTES"] + row["BYTES_REV"]
        for r in ipv6_ranges:
            if row["SRC_IP"] in r[0] or row["DST_IP"] in r[0]:
                hist_ipv6[r[0]] += 1
                hist_ipv6_bytes[r[0]] += row["BYTES"] + row["BYTES_REV"]

    error = False
    for r in ipv4_ranges:
        range_percentage = hist_ipv4[r[0]] / sum(hist_ipv4.values())
        range_percentage_bytes = hist_ipv4_bytes[r[0]] / sum(hist_ipv4_bytes.values())
        diff = abs(range_percentage - r[1])
        logging.warning(
            "%s: %.2f%% (diff: %.2f) (bytes perc: %.2f%%)",
            r[0],
            range_percentage * 100,
            diff * 100,
            range_percentage_bytes * 100,
        )
        if diff > 0.01:
            error = True

    for r in ipv6_ranges:
        range_percentage = hist_ipv6[r[0]] / sum(hist_ipv6.values())
        range_percentage_bytes = hist_ipv6_bytes[r[0]] / sum(hist_ipv6_bytes.values())
        diff = abs(range_percentage - r[1])
        logging.warning(
            "%s: %.2f%% (diff: %.2f) (bytes perc: %.2f%%)",
            r[0],
            range_percentage * 100,
            diff * 100,
            range_percentage_bytes * 100,
        )
        if diff > 0.01:
            error = True

    assert error is False, "Probabilities of IP ranges in flows don't match definition."


def test_basic():
    """Test with basic profile. Profile is too small so no analyze part is done.
    Checking only successful invocation of enhancer.
    """

    enhc = ProfileEnhancer()

    ipv4_ranges = ["146.102.0.0/16 40%", "147.230.0.0/16"]
    ipv6_ranges = ["2001:0718:0001:0005::/64 10%", "4001:0718:0001:0005::/64"]
    enhc.enhance(
        os.path.join(INPUT_DIR, "csv_basic_profile.csv"), os.path.join(FILE_DIR, "tmp.csv"), ipv4_ranges, ipv6_ranges
    )


@pytest.mark.dev
def test_big():
    """Test with real (big) network profile. Not part of the repository."""

    enhc = ProfileEnhancer()

    ipv4_ranges = [(ipaddress.IPv4Network("146.102.0.0/16"), 0.4), (ipaddress.IPv4Network("147.230.0.0/16"), 0.6)]
    ipv6_ranges = [
        (ipaddress.IPv6Network("2001:0718:0001:0005::/64"), 0.1),
        (ipaddress.IPv6Network("4001:0718:0001:0005::/64"), 0.9),
    ]
    enhc.enhance(
        os.path.join(INPUT_DIR, "cesnet_nix_zikova_morning.csv"),
        os.path.join(FILE_DIR, "tmp.csv"),
        [f"{n} {int(p*100)}%" for n, p in ipv4_ranges],
        [f"{n} {int(p*100)}%" for n, p in ipv6_ranges],
    )

    _analyze(os.path.join(FILE_DIR, "tmp.csv"), ipv4_ranges, ipv6_ranges)
