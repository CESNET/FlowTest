"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Core logic of the flowtest profiler. Parse arguments, initialize other components,
acquire flows from input reader and pass them to flow cache and then to the writer.
"""

import argparse
import logging

from ftprofiler.cache import FlowCache
from ftprofiler.flow import Flow
from ftprofiler.readers import InputException, InputInterface, flow_readers, init_reader
from ftprofiler.writer import OutputException, ProfileWriter

LOGGING_FORMAT = "%(asctime)-15s,%(name)s,[%(levelname)s],%(filename)s:%(funcName)s - %(message)s"
LOGGING_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"
logging.basicConfig(level=logging.DEBUG, format=LOGGING_FORMAT, datefmt=LOGGING_DATE_FORMAT)


def process_flows(reader: InputInterface, writer: ProfileWriter, inact: int, act: int, limit: int) -> None:
    """Acquire new flows from the reader. Put flows into a flow cache. Pass flows from flow cache to the writer.

    Parameters
    ----------
    reader : InputInterface
        Reader to be used for acquiring new flows.
    writer : ProfileWriter
        Writer where anonymous flows are written.
    inact : int
        Inactive timeout.
    act : int
        Active timeout.
    limit : int
        Maximum number of flows which can be stored into a cache.

    Raises
    ------
    InputException
        If the input reader plugin encountered an unrecoverable error.
    OutputException
        If the writer encountered an unrecoverable error.
    """
    cache = FlowCache(inact, act, limit)
    input_reader = iter(reader)
    try:
        while True:
            flow = next(input_reader)
            # Immediately write flows which were return by the cache due to hash collision or size limit.
            for export_flow in cache.add_flow(flow):
                writer.write(export_flow)
    except KeyboardInterrupt:
        reader.terminate()
    except StopIteration:
        # Get content of the flow cache and write it to the output.
        for export_flow in cache.remove_flows():
            writer.write(export_flow)


def main() -> int:
    """Entry point.

    Returns
    -------
    int
        Exit code.
    """
    parser = argparse.ArgumentParser(prog="Profiler")
    parser.add_argument("-V", "--version", action="version", version="%(prog)s 1.0")
    parser.add_argument(
        "-o",
        "--output",
        required=True,
        type=str,
        help="path to an output file where the profile is to be written",
    )
    parser.add_argument(
        "-g",
        "--gzip",
        action="store_true",
        default=False,
        help="compress output using gzip algorithm",
    )
    parser.add_argument(
        "-a",
        "--active",
        type=int,
        default=300,
        help="value of the active timeout in seconds (default: 300s)",
    )
    parser.add_argument(
        "-i",
        "--inactive",
        type=int,
        default=30,
        help="value of the inactive timeout in seconds (default: 30s)",
    )
    parser.add_argument(
        "-m",
        "--memory",
        type=int,
        default=2048,
        help="limit the maximum number of MiB consumed by the flow cache (default: 2048 MiB)",
    )
    subparsers = parser.add_subparsers(dest="reader", help="Flow Readers")

    for reader in flow_readers:
        reader.add_argument_parser(subparsers)

    args = parser.parse_args()

    try:
        reader = init_reader(args.reader, args)
        with ProfileWriter(args.output, Flow.FLOW_CSV_FORMAT, compress=args.gzip) as writer:
            process_flows(reader, writer, args.inactive * 1000, args.active * 1000, args.memory * 1048576 // Flow.SIZE)
    except (InputException, OutputException) as err:
        logging.getLogger().error(err)
        return 1

    return 0
