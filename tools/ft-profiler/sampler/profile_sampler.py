#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Script utilizing genetic algorithm to create a subset of biflows
without changing key profile metrics.
The following profile metrics are kept when performing the sampling:
    packets to bytes ratio
    biflows to packets ratio
    biflows to bytes ratio
    IPv6 to IPv4 ratio
    proportional representation of L4 protocols in the profile (top 5)
    proportional representation of source and destination ports in the profile (top 10)
    proportional representation of average packet sizes in biflows
"""

import argparse
import logging
import signal
import sys
import time

from types import FrameType
from typing import Optional, Tuple

import pandas as pd
import numpy as np
import pygad

LOGGING_FORMAT = "%(asctime)-15s,[%(levelname)s] - %(message)s"
LOGGING_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"
LOGGING_LEVEL = logging.INFO

CSV_COLUMN_TYPES = {
    "START_TIME": np.uint32,
    "END_TIME": np.uint32,
    "L3_PROTO": np.uint8,
    "L4_PROTO": np.uint16,
    "SRC_PORT": np.uint16,
    "DST_PORT": np.uint16,
    "PACKETS": np.uint64,
    "BYTES": np.uint64,
    "PACKETS_REV": np.uint64,
    "BYTES_REV": np.uint64,
}

TOP_L4_PROTO_CNT = 5
TOP_L7_PROTO_CNT = 10


def signal_handler(sig: int, frame: Optional[FrameType]) -> None:
    """Catch SIGINT and SIGTERM. Exit immediately.

    Parameters
    ----------
    sig : int
        NOT USED.
    frame : FrameType, None
        NOT USED.
    """
    del sig, frame
    sys.exit(0)


# pylint: disable=too-few-public-methods
# pylint: disable=too-many-instance-attributes
class Globals:
    """Contain global variables useful in GA functions.

    Attributes
    ----------
    seed : int
        Seed to initialize the random number generator.
    rng : np.random.default_rng
        Random number generator.
    sample_path : str
        Path to where the sample should be written.
    stats_path : str
        Path to where the sample statistics should be written.
    deviation : float
        Percentual deviation for each metric.
    max_biflows : int
        Maximum number of biflows in the sample.
    min_biflows : int
        Minimum number of biflows in the sample.
    profile : pd.DataFrame
        Original profile.
    profile_stats : Metrics
        Metrics of the original profile.
    gen : int
        Number of GA generations.
    population_size : int
        Population size in the GA.
    best_fit : float
        Best fitness value so far.
    """

    def __init__(
        self,
        profile: pd.DataFrame,
        min_sampling: float,
        max_sampling: float,
        deviation: float,
        sample_path: str,
        stats_path: str,
        generations: int,
        population_size: int,
        seed: Optional[int] = None,
    ) -> None:
        """Setup global configuration."""
        self.seed = seed or int(time.time())
        self.rng = np.random.default_rng(self.seed)
        self.sample_path = sample_path
        self.stats_path = stats_path
        self.deviation = deviation
        self.max_biflows = int(len(profile.index) * min(1.0, max_sampling))
        self.min_biflows = int(len(profile.index) * max(0.0, min_sampling))
        self.profile = profile
        self.profile_stats = Metrics(profile)
        self.gen = generations
        self.population_size = population_size
        self.best_fit = 0.0


# pylint: disable=too-few-public-methods
class Metrics:
    """Container for metrics of a profile or its sample.

    Attributes
    ----------
    biflows_cnt : int
        Number of biflows in the sample.
    pkts_cnt : int
        Number of packets in the sample.
    bts_cnt : int
        Number of bytes in the sample.
    pbr : float
        Packets to bytes ratio.
    fpr : float
        Biflows to bytes packets.
    fbr : float
        Biflows to bytes ratio.
    ipvr : float
        IPv6 to IPv4 ratio.
    l4_proto : pd.DataFrame
        Proportional representation of L4 protocols in the profile sample.
    ports : pd.DataFrame
        Proportional representation of ports in the profile sample.
    avg_pkt_size : pd.DataFrame
        Proportional representation of average packet sizes in biflows in the profile sample.
    """

    def __init__(self, data: pd.DataFrame) -> None:
        """Compute metrics from the provided data frame.

        Parameters
        ----------
        data : pd.DataFrame
            Data frame with biflows.
        """
        self.biflows_cnt = len(data.index)
        self.pkts_cnt = data["PACKETS"].sum() + data["PACKETS_REV"].sum()
        self.bts_cnt = data["BYTES"].sum() + data["BYTES"].sum()
        self.pbr = self.pkts_cnt / self.bts_cnt
        self.fpr = self.biflows_cnt / self.pkts_cnt
        self.fbr = self.biflows_cnt / self.bts_cnt
        l3_proto = data["L3_PROTO"].value_counts(normalize=True)

        if l3_proto.get(6) is None:
            self.ipvr = 0
        else:
            self.ipvr = l3_proto.get(6) / l3_proto.get(4)

        self.l4_proto = data.value_counts(subset=["L4_PROTO"], normalize=True)
        self.ports = pd.concat([data["SRC_PORT"], data["DST_PORT"]]).value_counts(normalize=True)
        avg_packet_sizes = (data["BYTES"] + data["BYTES_REV"]) / (data["PACKETS"] + data["PACKETS_REV"])
        self.avg_pkt_size = pd.cut(avg_packet_sizes, [0, 128, 512, 1024, 9000]).value_counts(normalize=True)


class MetricsDiff:
    """Deviations for individual profile metrics.

    Attributes
    ----------
    sol : Metrics
        Solution.
    ref : Metrics
        Original profile.
    pbr : float
        Packets to bytes ratio deviation (%).
    fpr : float
        Biflows to bytes packets deviation (%).
    fbr : float
        Biflows to bytes ratio deviation (%).
    ipvr : float
        IPv6 to IPv4 ratio deviation (%).
    l4_proto : pd.DataFrame
        Proportional representation of L4 protocols deviation (%).
    ports : pd.DataFrame
        Proportional representation of ports deviation (%).
    pkt_size : pd.DataFrame
        Proportional representation of average packet sizes in biflows deviation (%).
    """

    def __init__(self, sol: Metrics, ref: Metrics) -> None:
        """Initialize the object by computing and normalizing differences for each of the profile metrics.

        Parameters
        ----------
        sol : Metrics
            Metrics of the solution.
        ref : Metrics
            Metrics of the whole profile.
        """
        self.sol = sol
        self.ref = ref
        self.pbr = np.abs(ref.pbr - sol.pbr) * 100 / ref.pbr
        self.fpr = np.abs(ref.fpr - sol.fpr) * 100 / ref.fpr
        self.fbr = np.abs(ref.fbr - sol.fbr) * 100 / ref.fbr
        self.ipvr = 0.0 if ref.ipvr == 0 else np.abs(ref.ipvr - sol.ipvr) * 100 / ref.ipvr

        l4_subset = sol.l4_proto.loc[ref.l4_proto.head(TOP_L4_PROTO_CNT).index.values]
        self.l4_proto = (
            (ref.l4_proto.head(TOP_L4_PROTO_CNT) - l4_subset).abs() * 100 / ref.l4_proto.head(TOP_L4_PROTO_CNT)
        )

        ports_subset = sol.ports.loc[ref.ports.head(TOP_L7_PROTO_CNT).index.values]
        self.ports = (ref.ports.head(TOP_L7_PROTO_CNT) - ports_subset).abs() * 100 / ref.ports.head(TOP_L7_PROTO_CNT)

        self.pkt_size = (ref.avg_pkt_size - sol.avg_pkt_size).abs() * 100 / ref.avg_pkt_size

    def fitness(self) -> float:
        """Compute the fitness value of the individual.

        Fitness value of an individual is set to 100 - percentual deviation of every metric from the original metrics.
        Solutions which have superb results in some metrics and terrible in other metrics are considered worse
        than solutions where all metric deviations are mediocre.
        To further penalize solutions where some metrics deviate more than 1% from the original,
        all metric deviations are squared.
        Generally, it is expected that deviations start to get below 1% in solutions with fitness above 94.

        Returns
        -------
        float
            Fitness value of the individual in range 0 - 100 (the more the merrier).
        """
        fit = 100 - np.square(self.pbr) - np.square(self.fpr) - np.square(self.fbr) - np.square(self.ipvr)
        fit -= self.l4_proto.pow(2).sum()
        fit -= self.ports.pow(2).sum()
        fit -= self.pkt_size.pow(2).sum()
        return max(fit, 0.0)

    def is_acceptable(self, deviation: float) -> bool:
        """Check if all metrics are below the deviation limit for acceptance.

        Parameters
        ----------
        deviation : float
            Allowed deviation (%).

        Returns
        -------
        bool
            True - solution is acceptable, False otherwise.
        """
        if self.pbr > deviation or self.fpr > deviation or self.fbr > deviation or self.ipvr > deviation:
            return False

        if (self.l4_proto > deviation).any() or (self.ports > deviation).any() or (self.pkt_size > deviation).any():
            return False

        return True

    def __str__(self) -> str:
        """Create a report of the solution metrics with respect to the original metrics.

        Returns
        -------
        str
            Solution stats report.
        """
        res = f"SAMPLING RANGE: {args.min_sampling:.3f} - {args.max_sampling:.3f}\n"
        res += f"GENERATIONS: {glob.gen} POPULATION: {glob.population_size} FITNESS: {self.fitness():.4f}\n"
        res += f"SEED: {glob.seed}\n"
        res += (
            f"SOLUTION SIZE: BIFLOWS({(self.sol.biflows_cnt / self.ref.biflows_cnt):.3f}), "
            f"PACKETS({(self.sol.pkts_cnt / self.ref.pkts_cnt):.3f}), "
            f"BYTES({(self.sol.bts_cnt / self.ref.bts_cnt):.3f})\n"
        )
        res += f"{'METRIC':<17}{'ORIGINAL':<18}{'SOLUTION':<18}{'DIFF (%)':<18}\n"
        res += f"{'PACKETS/BYTES':<17}{self.ref.pbr:.6f}{'':<10}{self.sol.pbr:.6f}{'':<10}{self.pbr:.2}\n"
        res += f"{'BIFLOWS/PACKETS':<17}{self.ref.fpr:.6f}{'':<10}{self.sol.fpr:.6f}{'':<10}{self.fpr:.2}\n"
        res += f"{'BIFLOWS/BYTES':<17}{self.ref.fbr:.6f}{'':<10}{self.sol.fbr:.6f}{'':<10}{self.fbr:.2}\n"
        res += f"{'IPv6/IPv4':<17}{self.ref.ipvr:.6f}{'':<10}{self.sol.ipvr:.6f}{'':<10}{self.ipvr:.2}\n"

        l4_proto = self.ref.l4_proto.head(TOP_L4_PROTO_CNT)
        l4_proto_sol = self.sol.l4_proto.loc[l4_proto.index.values]
        l4_res = pd.concat([l4_proto, l4_proto_sol, self.l4_proto], axis=1, ignore_index=False)
        l4_res = l4_res.rename(columns={0: "ORIGINAL", 1: "SOLUTION", 2: "DIFF (%)"}).rename_axis("L4 PROTOCOL")
        res += str(l4_res) + "\n"

        ports = self.ref.ports.head(TOP_L7_PROTO_CNT)
        ports_sol = self.sol.ports.loc[ports.index.values]
        ports_res = pd.concat([ports, ports_sol, self.ports], axis=1, ignore_index=False)
        ports_res = ports_res.rename(columns={0: "ORIGINAL", 1: "SOLUTION", 2: "DIFF (%)"}).rename_axis("PORT")
        res += str(ports_res) + "\n"

        pkt_size_res = pd.concat(
            [self.ref.avg_pkt_size, self.sol.avg_pkt_size, self.pkt_size], axis=1, ignore_index=False
        )
        pkt_size_res = pkt_size_res.rename(columns={0: "ORIGINAL", 1: "SOLUTION", 2: "DIFF (%)"}).rename_axis(
            "AVG PACKET SIZE"
        )
        res += str(pkt_size_res) + "\n"

        return res


def fitness(solution: np.array, solution_idx: int) -> float:
    """Compute the fitness value of the individual.

    Parameters
    ----------
    solution : np.array
        Individual from the population.
    solution_idx : int
        NOT USED.

    Returns
    -------
    float
        Fitness value of the individual.
    """
    del solution_idx
    if not glob.min_biflows <= np.sum(solution) <= glob.max_biflows:
        return 0.0

    # Select only profile rows which are in the current solution.
    solution_df = glob.profile.iloc[np.nonzero(solution)[0]]
    return MetricsDiff(Metrics(solution_df), glob.profile_stats).fitness()


def initialize_population(size: int, gene_len: int, min_genes: int, max_genes: int) -> np.array:
    """Randomly initialize the population.

    Parameters
    ----------
    size : int
        Population size.
    gene_len : int
        Length of the genome.
    min_genes : int
        Minimum amount of genes to be activated.
    max_genes : int
        Maximum amount of genes to be activated.

    Returns
    -------
    np.array
        Initialized population.
    """
    population = np.zeros(shape=(size, gene_len), dtype=np.uint8)
    for individual in population:
        indexes = glob.rng.choice(gene_len, glob.rng.integers(min_genes, max_genes, 1), replace=False)
        np.put(individual, indexes, 1)

    return population


def mutation_func(offspring: Tuple[np.array, float, int], gai: pygad.GA) -> np.array:
    """Mutate an individual by randomly shuffling portion of its genome.

    The shuffled portion of the genome gets smaller if the fitness of the best individual is above 90.

    Parameters
    ----------
    offspring : tuple
        Solution to be mutated.
    gai : pygad.GA
        Genetic algorithm instance.

    Returns
    -------
    np.array
        Mutated individual.
    """
    gene_len = len(offspring[0])
    best_fit = gai.best_solution()[1] if gai.generations_completed > 0 else fitness(offspring[0], 0)

    # Shuffle either 5% of the genome or 0.02% of the genome based on the best fitness in the population.
    mutation_gene_cnt = gene_len // 20 if best_fit < 90 else gene_len // 5000

    mut_idx = glob.rng.choice(gene_len - mutation_gene_cnt, 1, replace=False)[0]
    shuffle_section = offspring[0][mut_idx : mut_idx + mutation_gene_cnt]

    glob.rng.shuffle(shuffle_section)
    offspring[0][mut_idx : mut_idx + mutation_gene_cnt] = shuffle_section

    return offspring


def on_generation(gai: pygad.GA) -> None:
    """Check if the best solution in the current generation is an acceptable solution.
    If yes, it saves the solution and its statistics into configured files and exits the program.

    Parameters
    ----------
    gai : pygad.GA
        Genetic algorithm instance.
    """

    logging.getLogger().info(
        "generation %d: best fit: %.4f, mean fit: %.4f",
        gai.generations_completed,
        gai.best_solution()[1],
        np.mean(gai.last_generation_fitness),
    )
    if gai.best_solution()[1] > glob.best_fit:
        glob.best_fit = gai.best_solution()[1]
        solution = glob.profile.iloc[np.nonzero(gai.best_solution()[0])[0]]
        diff = MetricsDiff(Metrics(solution), glob.profile_stats)
        if diff.is_acceptable(glob.deviation):
            logging.getLogger().info("accepted solution:\n%s", diff)
            try:
                solution.to_csv(glob.sample_path, sep=",")
            except OSError as exc:
                logging.getLogger().info("unable to write solution to file: %s, reason: %s", glob.sample_path, str(exc))
                sys.exit(1)

            try:
                with open(glob.stats_path, mode="w", encoding="ascii") as stats:
                    stats.write(str(diff))
            except OSError as exc:
                logging.getLogger().info(
                    "unable to write solution statistics to file: %s, reason: %s", glob.stats_path, str(exc)
                )
                sys.exit(1)

            # gai.plot_fitness()
            sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="profile sampler")
    parser.add_argument("-V", "--version", action="version", version="%(prog)s 1.0.0")
    parser.add_argument(
        "-a",
        "--max-sampling",
        type=float,
        required=True,
        help="maximum sampling value (must be between 0 and 1)",
    )
    parser.add_argument(
        "-i",
        "--min-sampling",
        type=float,
        required=True,
        help="minimum sampling value (must be between 0 and 1)",
    )
    parser.add_argument(
        "-d",
        "--deviation",
        type=float,
        default=1.0,
        help="acceptable deviation (%%) for each solution metric from the original profile metric (default: 1.0)",
    )
    parser.add_argument(
        "-p",
        "--profile",
        required=True,
        type=str,
        help="path to the CSV profile file",
    )
    parser.add_argument(
        "-r",
        "--result",
        required=True,
        type=str,
        help="path to a file where the result (profile sample) is to be written",
    )
    parser.add_argument(
        "-m",
        "--metrics",
        required=True,
        type=str,
        help="path to a file where result metrics are to be written",
    )
    parser.add_argument(
        "-e",
        "--seed",
        type=int,
        default=None,
        help="seed to be passed to the random number generator (default: None)",
    )
    parser.add_argument(
        "-g",
        "--generations",
        type=int,
        default=500,
        help="number of generations (default: 500)",
    )
    parser.add_argument(
        "-s",
        "--size",
        type=int,
        default=30,
        help="number of solutions per generation (default: 30)",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="do not print regular information",
    )

    args = parser.parse_args()
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    if args.min_sampling >= args.max_sampling:
        logging.getLogger().error(
            "minimum sampling value (%f) must be < maximum sampling value (%f)", args.min_sampling, args.max_sampling
        )
        sys.exit(1)

    if args.quiet:
        LOGGING_LEVEL = logging.WARN

    logging.basicConfig(level=LOGGING_LEVEL, format=LOGGING_FORMAT, datefmt=LOGGING_DATE_FORMAT)

    logging.getLogger().info("loading profile file=%s ...", args.profile)
    try:
        df = pd.read_csv(args.profile, engine="pyarrow", dtype=CSV_COLUMN_TYPES)
    except OSError as err:
        logging.getLogger().error("unable to load profile file=%s, error=%s", args.profile, str(err))
        sys.exit(1)

    logging.getLogger().info("parsing original stats ...")
    glob = Globals(
        df,
        args.min_sampling,
        args.max_sampling,
        args.deviation,
        args.result,
        args.metrics,
        args.generations,
        args.size,
        seed=args.seed,
    )
    logging.getLogger().info("using seed=%d", glob.seed)

    logging.getLogger().info("creating initial population ...")
    pop = initialize_population(args.size, glob.profile_stats.biflows_cnt, glob.min_biflows, glob.max_biflows)

    logging.getLogger().info("initiating genetic algorithm ...")
    evolution = pygad.GA(
        num_generations=glob.gen,
        num_parents_mating=4,
        initial_population=pop,
        fitness_func=fitness,
        gene_type=np.uint8,
        parent_selection_type="sus",
        crossover_type="two_points",
        crossover_probability=0.3,
        mutation_type=mutation_func,
        on_generation=on_generation,
        random_seed=glob.seed,
    )

    logging.getLogger().info("starting genetic algorithm ...")
    evolution.run()

    logging.getLogger().info("sampler was not able to find acceptable solution.")

    sys.exit(0)
