#!/usr/bin/env python3

"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Visualize profile attributes on a timeline.
"""

import argparse

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

parser = argparse.ArgumentParser(prog="Profile Visualizer")
parser.add_argument("-V", "--version", action="version", version="%(prog)s 1.0.0")
parser.add_argument(
    "-f",
    "--format",
    default="png",
    help="type of the output (png/pdf/ps/eps/svg, default: png)",
)
parser.add_argument(
    "-i",
    "--inactive",
    type=int,
    default=30,
    help="value of the inactive timeout in seconds (default: 30s)",
)
parser.add_argument(
    "-o",
    "--output",
    type=str,
    required=True,
    help="path to save the visualization",
)
parser.add_argument(
    "-p",
    "--profile",
    type=str,
    required=True,
    help="path to the profile in CSV format",
)

args = parser.parse_args()

INACTIVE_TIMEOUT = args.inactive

# Read the CSV file into a DataFrame
df = pd.read_csv(args.profile)

# Convert START_TIME and END_TIME to time intervals with a granularity in seconds
df["START_TIME"] = df["START_TIME"] // 1000
df["END_TIME"] = df["END_TIME"] // 1000

t_first = df["START_TIME"].min()
t_last = df["END_TIME"].max()
indices = range(t_first, t_last + INACTIVE_TIMEOUT + 1)
buckets = np.zeros((3, t_last - t_first + INACTIVE_TIMEOUT + 1))

# Iterate through each row of the DataFrame and distribute packets into time intervals
for _, row in df.iterrows():
    start_time = row["START_TIME"]
    end_time = row["END_TIME"]
    packets_tot = row["PACKETS"] + row["PACKETS_REV"]
    bytes_tot = row["BYTES"] + row["BYTES_REV"]
    num_intervals = int(end_time - start_time) + 1

    packets_increment = packets_tot / num_intervals / 1000
    bits_increment = (bytes_tot * 8 / num_intervals) / (1024 * 1024)
    for i in range(num_intervals):
        buckets[0][start_time - t_first + i] += packets_increment
        buckets[1][start_time - t_first + i] += bits_increment

    for i in range(num_intervals + INACTIVE_TIMEOUT):
        buckets[2][start_time - t_first + i] += 1

# Create the histogram plot
fig, axes = plt.subplots(2, 2, figsize=(10, 8))
axes[0, 0].bar(indices, buckets[0], width=1, align="center")
axes[0, 0].set_title("Packets Distribution (Kpps)")

axes[0, 1].bar(indices, buckets[1], width=1, align="center")
axes[0, 1].set_title("Octets Distribution (Mbps)")

axes[1, 0].bar(indices, buckets[2], width=1, align="center")
axes[1, 0].set_title("Flow Cache Utilization")

hist1, bins = np.histogram(df["START_TIME"], bins=len(indices))
hist2, _ = np.histogram(df["END_TIME"], bins=bins)

axes[1, 1].bar(bins[:-1], hist1, width=bins[1] - bins[0], label="Flow Start", color="blue")
axes[1, 1].bar(bins[:-1], -hist2, width=bins[1] - bins[0], label="Flow End", color="red")
axes[1, 1].set_title("Flow Start and End Times")

fig.suptitle("Profile Attributes")
plt.tight_layout()
plt.subplots_adjust(top=0.85)

plt.savefig(args.output, dpi=100, format=args.format)
