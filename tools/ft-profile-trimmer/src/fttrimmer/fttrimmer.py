"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

ProfileTrimmer class and main for manipulating profile with CSV.
PTStatistics class for printing statistics about original and trimmed profile.
"""

import argparse
import logging
from typing import List, Optional, Tuple, Union

import numpy as np
import pandas as pd

LOGGING_FORMAT = "%(asctime)-15s,%(name)s,[%(levelname)s],%(filename)s:%(funcName)s - %(message)s"
LOGGING_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"
logging.basicConfig(level=logging.DEBUG, format=LOGGING_FORMAT, datefmt=LOGGING_DATE_FORMAT)


class ProfileTrimmerException(Exception):
    """General exception used in Profile Trimmer."""


class ProfileTrimmer:
    """Class for trimming flows.

    Read CSV with profiles and trim it according to supplied parameters.

    Attributes
    ----------
    main_left : int
        Start time of main interval for flow trimming in seconds
    main_right : int
        End time of main interval for flow trimming in seconds
    tolerance_left : int
        Start time of left tolerance interval
    tolerance_right : int
        End time of right tolerance interval
    df_original : pd.DataFrame
        DataFrame with original profile
    df_trimmed : pd.DataFrame
        DataFrame with trimmed profile
    stats : PTStatistics
        Instance of PTStatistics
    """

    CSV_COLUMN_TYPES = {
        "START_TIME": np.int64,
        "END_TIME": np.int64,
        "L3_PROTO": np.uint8,
        "L4_PROTO": np.uint8,
        "SRC_PORT": np.uint16,
        "DST_PORT": np.uint16,
        "PACKETS": np.uint64,
        "BYTES": np.uint64,
        "PACKETS_REV": np.uint64,
        "BYTES_REV": np.uint64,
    }
    MILLISECONDS = 1000

    def __init__(
        self, input_path: str, output_path: str, tolerance: int, main_or_window: Union[int, Tuple[int, int]]
    ) -> None:
        """Main function of the class, which trims the profile.

        Parameters
        ----------
        input_path : str
            Path with input CSV file with profiles.
        output_path : str
            Path with output CSV file with trimmed profiles.
        tolerance : int
            Tolerance interval in seconds for flows before and after main interval
        main_or_window : int, tuple(int, int)
            Either a main interval in seconds or a tuple with start and end time for trimming in seconds.
        """
        self.df_original = self.read_csv(input_path)

        if isinstance(main_or_window, int):
            self.check_params(tolerance, main_interval=main_or_window)
            self.calculate_intervals(tolerance * self.MILLISECONDS, main_interval=main_or_window * self.MILLISECONDS)
        elif isinstance(main_or_window, tuple):
            self.check_params(tolerance, main_left=main_or_window[0], main_right=main_or_window[1])
            self.calculate_intervals(
                tolerance * self.MILLISECONDS,
                main_left=main_or_window[0] * self.MILLISECONDS,
                main_right=main_or_window[1] * self.MILLISECONDS,
            )
        else:
            raise ProfileTrimmerException("Parameter main_or_window must be int or a tuple.")

        try:
            self.df_trimmed, self.stats = self.trim()
        except Exception as err:
            raise ProfileTrimmerException("Error when trimming flows") from err

        self.save_csv(self.df_trimmed, output_path)

        try:
            self.stats.statistics(self.df_original, self.df_trimmed)
        except Exception as err:
            raise ProfileTrimmerException("Error when calculating statistics") from err

    @classmethod
    def read_csv(cls, csv_path: str) -> pd.DataFrame:
        """Read input CSV file with profiles into DataFrame.

        Parameters
        ----------
        csv_path : str
            Path with input CSV file with profiles.

        Returns
        -------
        pd.DataFrame
            DataFrame object with profiles.

        Raises
        ------
        ProfileTrimmerException
            Problem with opening or reading CSV file.
        """
        logging.getLogger().debug("reading csv profile=%s", csv_path)
        try:
            return pd.read_csv(csv_path, engine="pyarrow", dtype=cls.CSV_COLUMN_TYPES)
        except Exception as err:
            raise ProfileTrimmerException(f"Unable to read CSV file: {csv_path}") from err

    @classmethod
    def save_csv(cls, trimmed_df: pd.DataFrame, csv_path: str) -> None:
        """Save DataFrame with trimmed profiles as CSV file.

        Parameters
        ----------
        trimmed_df : pd.DataFrame
            DataFrame with trimmed profiles.
        csv_path : str
            Path with output CSV file with trimmed profiles.

        Raises
        ------
        ProfileTrimmerException
            Problem with saving output CSV file.
        """
        logging.getLogger().debug("saving trimmed csv profile=%s", csv_path)
        try:
            with open(csv_path, "w", encoding="ascii") as csv_file:
                trimmed_df.to_csv(csv_file, index=False)
        except Exception as err:
            raise ProfileTrimmerException(f"Unable to save CSV file: {csv_path}") from err

    def calculate_intervals(
        self,
        tolerance: int,
        main_interval: Optional[int] = None,
        main_left: Optional[int] = None,
        main_right: Optional[int] = None,
    ) -> None:
        """Calculate main and tolerance intervals.
        If main interval is provided via -m parameter, a middle of all flows is calculated.
        Then left and right times of main intervals are calculated.
        Finally left and right times of tolerance intervals are calculated.

        Parameters
        ----------
        tolerance : int
            Tolerance interval
        main_interval : None, int
            Main interval provided by -m parameter in milliseconds
        main_left : None, int
            Start time interval provided by -s parameter in milliseconds
        main_right : None, int
            End time interval provided by -e parameter in milliseconds

        Raises
        ------
        ProfileTrimmerException
            Time range of the CSV profile is less or equal to 0.
        """
        time_start = self.df_original["START_TIME"].min()
        time_end = self.df_original["END_TIME"].max()
        time_range = time_end - time_start
        logging.getLogger().info("Minimal START_TIME in the profile: %d", time_start)
        logging.getLogger().info("Maximal END_TIME in the profile: %d", time_end)
        logging.getLogger().info("Time range of the profile: %d", time_range)
        if time_range <= 0:
            raise ProfileTrimmerException("Time range of the profile must be greater than 0.")

        if main_interval:
            if (2 * tolerance) + main_interval > time_range:
                logging.getLogger().warning("Provided main or tolerance interval is too large!")

            time_middle = time_start + (time_end - time_start) / 2
            self.main_left = time_middle - (main_interval) / 2
            self.main_right = time_middle + (main_interval) / 2
        else:
            self.main_left = main_left
            self.main_right = main_right

        self.tolerance_left = self.main_left - tolerance
        self.tolerance_right = self.main_right + tolerance
        logging.getLogger().info("Left and right main interval: %d %d", self.main_left, self.main_right)
        logging.getLogger().info("Left and right tolerance interval: %d %d", self.tolerance_left, self.tolerance_right)
        if self.main_left < time_start or self.tolerance_left < time_start:
            logging.getLogger().warning("Left main/tolerance interval is before minimal START_TIME!")
        if self.main_right > time_end or self.tolerance_right > time_end:
            logging.getLogger().warning("Right main/tolerance interval is after maximal END_TIME!")

    def trim(self) -> Tuple[pd.DataFrame, "PTStatistics"]:
        """Select four subsets from original profile:
        * flows inside main interval (subset_nonaltered)
        * flows exclusively inside left tolerance interval or right tolerance interval (subset_tolerance)
            * randomly sample half of the flows
        * flows starting before left tolerance interval ending before main interval or flows starting after
        main interval and ending after right tolerance interval (subset_dropped)
            * exclude those flows from trimmed profile
        * flows not present in any of the three previous subsets (subset_scaled)
            * shift START_TIME and END_TIME of those flows and scale packets and bytes (+rev) appropriately
        Trimmed profile consists of union (concatenation) of subsets:
        * subset_nonaltered
        * subset_tolerance
        * subset_scaled

        Calculate statistics from original and trimmed profile.

        Returns
        -------
        tuple
            DataFrame with trimmed profile and PTStatistics instance
        """
        # flows with start/end timestamp inside main interval - use flow as is
        subset_nonaltered = self.df_original[
            (self.df_original["START_TIME"] >= self.main_left) & (self.df_original["END_TIME"] <= self.main_right)
        ]

        # flows start and end in left/right tolerance interval
        subset_tolerance = self.df_original[
            (
                (self.df_original["START_TIME"] >= self.tolerance_left)  # flow starts inside left tolerance interval
                & (self.df_original["END_TIME"] <= self.main_left)  # flow ends inside left tolerance interval
                & (self.df_original["START_TIME"] != self.main_left)  # flow is not duplicate of subset_nonaltered
            )
            | (
                (self.df_original["START_TIME"] >= self.main_right)
                & (self.df_original["END_TIME"] <= self.tolerance_right)  # or dtto, but for right tolerance interval
                & (self.df_original["END_TIME"] != self.main_right)
            )
        ]
        # filter approximately half of the flows
        subset_tolerance_sampled = subset_tolerance.sample(frac=0.5)
        dropped_from_tolerance = len(subset_tolerance) - len(subset_tolerance_sampled)

        # Automatically dropped flows
        # Flows starting before left tolerance interval ending before main interval
        # or flows starting after main interval and ending after right tolerance interval
        subset_dropped = self.df_original[
            ((self.df_original["START_TIME"] < self.tolerance_left) & (self.df_original["END_TIME"] < self.main_left))
            | (
                (self.df_original["START_TIME"] > self.main_right)
                & (self.df_original["END_TIME"] > self.tolerance_right)
            )
        ]

        # rest of the flows to be used and scaled, should be:
        # Flows starting before main interval and ending in main interval or after main interval
        # or flows starting in main interval and ending after main interval
        subset_scaled = (
            self.df_original.drop(index=subset_nonaltered.index)
            .drop(index=subset_tolerance.index)
            .drop(index=subset_dropped.index)
        )
        # shift and scale all flows in the subset
        subset_scaled = subset_scaled.apply(self._shift_and_scale, axis=1)
        # drop flagged flows with START_TIME greater or equal to END_TIME
        subset_scaled_valid = subset_scaled.dropna()
        dropped_from_scaled = len(subset_scaled) - len(subset_scaled_valid)
        logging.getLogger().debug(
            "Number of dropped flows with START_TIME greater or equal to END_TIME: %d", dropped_from_scaled
        )

        # calculate dropped flows for statistics
        dropped_flows = dropped_from_tolerance + len(subset_dropped) + dropped_from_scaled

        return pd.concat([subset_nonaltered, subset_tolerance_sampled, subset_scaled_valid]).astype(
            dtype=self.CSV_COLUMN_TYPES
        ), PTStatistics(len(subset_nonaltered), len(subset_scaled), dropped_flows)

    def _shift_and_scale(self, row: pd.core.series.Series) -> pd.core.series.Series:
        """Shift START_TIME and/or END_TIME of flow and arithmetically
        scale BYTES and PACKETS (+ reverse) columns.
        This function is slow. But we need to iterate through every
        row and shift and scale columns which depends on each other.
        This can be probably enhanced. On testing data, the whole
        subset had maximally 30k rows and took 4 seconds to process.

        Parameters
        ----------
        row : pd.core.series.Series
            Row from DataFrame to be processed.

        Returns
        -------
        pd.core.series.Series
            Processed row.
        """
        original_dur = row["END_TIME"] - row["START_TIME"]

        # shift START_TIME if it's before main interval
        if row["START_TIME"] < self.main_left:
            row["START_TIME"] = int(
                np.random.uniform(np.maximum(self.tolerance_left, row["START_TIME"]), self.main_left)
            )

        # shift END_TIME if it's after main interval
        if row["END_TIME"] > self.main_right:
            row["END_TIME"] = int(np.random.uniform(self.main_right, np.minimum(self.tolerance_right, row["END_TIME"])))

        # flag and then drop flows with START_TIME greater or equal to END_TIME - edge case
        if row["START_TIME"] >= row["END_TIME"]:
            row["START_TIME"] = np.NaN
            return row

        # scale bytes and packets in both directions
        shifted_dur = row["END_TIME"] - row["START_TIME"]
        ratio = min(round(shifted_dur / original_dur, 3), 1)
        for flow_packets, flow_bytes in (("PACKETS", "BYTES"), ("PACKETS_REV", "BYTES_REV")):
            if row[flow_packets] > 0 or row[flow_bytes] > 0:
                row[flow_packets] = int(row[flow_packets] * ratio)
                row[flow_bytes] = int(row[flow_bytes] * ratio)
                if row[flow_packets] == 0 or row[flow_bytes] < 40:
                    row[flow_packets] = 1
                    row[flow_bytes] = max(40, row[flow_bytes])

        # flow contains only one packet, set flow duration to 0
        if row["PACKETS"] + row["PACKETS_REV"] == 1:
            row["END_TIME"] = row["START_TIME"]

        return row

    @staticmethod
    def check_params(
        tolerance: int,
        main_interval: Optional[int] = None,
        main_left: Optional[int] = None,
        main_right: Optional[int] = None,
    ) -> None:
        """Check if passed parameters have expected values.

        Parameters
        ----------
        tolerance : int
            Tolerance interval
        main_interval : None, int
            Main interval provided by -m parameter in milliseconds
        main_left : None, int
            Start time interval provided by -s parameter in milliseconds
        main_right : None, int
            End time interval provided by -e parameter in milliseconds

        Raises
        ------
        ProfileTrimmerException
            Wrongly passed parameters.
        """
        if not isinstance(tolerance, int) or tolerance < 0:
            raise ProfileTrimmerException("Parameter -t must be integer greater or equal to 0.")

        if main_interval is None and main_left is None:
            raise ProfileTrimmerException("Parameter -m or parameters -s with -e must be used.")

        if main_interval and main_left:
            raise ProfileTrimmerException("Parameter -m cannot be used together with -s and -e.")

        if (main_left is not None and main_right is None) or (main_left is None and main_right is not None):
            raise ProfileTrimmerException("Parameters -s and -e must be used together.")

        if main_left is not None and (not isinstance(main_left, int) or not isinstance(main_right, int)):
            raise ProfileTrimmerException("Parameter -s or parameters -e must be integers.")

        if main_left is not None and main_left > main_right:
            raise ProfileTrimmerException("Parameters -s cannot be after -e and vice versa.")

        if main_interval is not None and not isinstance(main_interval, int):
            raise ProfileTrimmerException("Parameter -m must be integer.")

        if main_interval is not None and main_interval <= 0:
            raise ProfileTrimmerException("Parameter -m must be positive non zero number.")


class PTStatistics:
    """Statistics about original DataFrame and trimmed DataFrame

    Attributes
    ----------
    flows_nonaltered : int
        Flows which has not been altered in the trimmed DataFrame.
    flows_altered : int
        Flows which has been altered in the trimmed DataFrame - START_TIME and/or END_TIME, PACKETS, BYTES + reverse.
    flows_dropped : int
        Flows which has been dropped from trimmed DataFrame.
    """

    def __init__(self, flows_nonaltered: int, flows_altered: int, flows_dropped: int):
        """Initialize ProfileTrimmer Statistics.

        Attributes
        ----------
        flows_nonaltered : int
            Flows not altered in the trimmed DataFrame.
        flows_altered : int
            Flows altered in the trimmed DataFrame - START_TIME and/or END_TIME, PACKETS, BYTES + reverse.
        flows_dropped : int
            Flows dropped from trimmed DataFrame.
        """
        self.flows_nonaltered = flows_nonaltered
        self.flows_altered = flows_altered
        self.flows_dropped = flows_dropped

    def prepare_data(self, df_original: pd.DataFrame, df_trimmed: pd.DataFrame) -> List[List[Union[int, int, float]]]:
        """Prepare data for statistics

        Attributes
        ----------
        df_original : pd.DataFrame
            DataFrame with original profile
        df_trimmed : pd.DataFrame
            DataFrame with trimmed profile

        Returns
        -------
        List[List[Union[int, int, float]]]
            Data for DataFrame with statistics according to columns.
        """
        nr_flows_orig = len(df_original)
        nr_flows_trim = len(df_trimmed)
        nr_bytes_orig = df_original["BYTES"].sum()
        nr_packets_orig = df_original["PACKETS"].sum()
        nr_bytes_rev_orig = df_original["BYTES_REV"].sum()
        nr_packets_rev_orig = df_original["PACKETS_REV"].sum()
        nr_bytes_trim = df_trimmed["BYTES"].sum()
        nr_packets_trim = df_trimmed["PACKETS"].sum()
        nr_bytes_rev_trim = df_trimmed["BYTES_REV"].sum()
        nr_packets_rev_trim = df_trimmed["PACKETS_REV"].sum()

        def round_stats(orig: int, trimmed: int) -> float:
            """Return rounded difference"""
            if orig == 0:  # packets_rev/bytes_rev could be 0
                return 0
            return round(trimmed / orig, 2)

        return [
            [nr_flows_orig, nr_flows_trim, round_stats(nr_flows_orig, nr_flows_trim)],
            [0, self.flows_nonaltered, round_stats(nr_flows_orig, self.flows_nonaltered)],
            [0, self.flows_altered, round_stats(nr_flows_orig, self.flows_altered)],
            [0, self.flows_dropped, round_stats(nr_flows_orig, self.flows_dropped)],
            [nr_bytes_orig, nr_bytes_trim, round_stats(nr_bytes_orig, nr_bytes_trim)],
            [nr_packets_orig, nr_packets_trim, round_stats(nr_packets_orig, nr_packets_trim)],
            [nr_bytes_rev_orig, nr_bytes_rev_trim, round_stats(nr_bytes_rev_orig, nr_bytes_rev_trim)],
            [nr_packets_rev_orig, nr_packets_rev_trim, round_stats(nr_packets_rev_orig, nr_packets_rev_trim)],
        ]

    def statistics(self, df_original: pd.DataFrame, df_trimmed: pd.DataFrame) -> pd.DataFrame:
        """Print statistics from original DataFrame and trimmed DataFrame.
        For each DataFrame, statistics are printed:
        * number of flows + difference
        * number of nonaltered/altered/dropped flows + difference
        * number of packets and bytes + difference

        Attributes
        ----------
        df_original: pd.DataFrame
            DataFrame with original profile
        df_trimmed: pd.DataFrame
            DataFrame with trimmed profile

        Returns
        -------
        pd.DataFrame
            DataFrame with statistics about original and trimmed DataFrame
        """
        time_start = df_trimmed["START_TIME"].min()
        time_end = df_trimmed["END_TIME"].max()
        time_range = time_end - time_start
        logging.getLogger().info("Minimal START_TIME in the trimmed profile: %d", time_start)
        logging.getLogger().info("Maximal END_TIME in the trimmed profile: %d", time_end)
        logging.getLogger().info("Time range of the trimmed profile: %d", time_range)

        df_stats = pd.DataFrame(
            data=self.prepare_data(df_original, df_trimmed),
            columns=[
                "Original profile",
                "Trimmed profile",
                "Difference",
            ],
            index=[
                "Number of flows",
                "Number of nonaltered flows",
                "Number of altered flows",
                "Number of dropped flows",
                "Number of packets",
                "Number of bytes",
                "Number of reverse packets",
                "Number of reverse bytes",
            ],
        )
        # recast DataFrame columns to correct types
        df_stats = df_stats.astype(
            dtype={
                "Original profile": np.int64,
                "Trimmed profile": np.int64,
                "Difference": np.float64,
            }
        )

        logging.getLogger().info("Statistics:\n%s", df_stats)

        return df_stats


def main() -> int:
    """Entry point.

    Returns
    -------
    int
        Exit code.
    """
    parser = argparse.ArgumentParser(prog="ProfileTrimmer")
    parser.add_argument(
        "-i",
        "--input",
        required=True,
        type=str,
        help="path to an input csv file where the network profile is stored",
    )
    parser.add_argument(
        "-o",
        "--output",
        required=True,
        type=str,
        help="path to an output csv file which has been trimmed",
    )
    parser.add_argument(
        "-t",
        "--tolerance",
        required=True,
        type=int,
        help="tolerance interval in seconds for flows before and after main interval. Could be 0.",
    )
    parser.add_argument(
        "-m",
        "--main",
        type=int,
        default=None,
        help="main interval in seconds for flow trimming. Must be positive non zero number.",
    )
    parser.add_argument(
        "-s",
        "--start",
        type=int,
        default=None,
        help="start time for flow trimming in seconds. Could be positive or negative number.",
    )
    parser.add_argument(
        "-e",
        "--end",
        type=int,
        default=None,
        help="end time for flow trimming in seconds. Could be positive or negative number.",
    )
    args = parser.parse_args()

    try:
        ProfileTrimmer(
            args.input, args.output, args.tolerance, args.main if args.main is not None else (args.start, args.end)
        )
    except ProfileTrimmerException as err:
        logging.getLogger().error(err)
        return 1

    return 0
