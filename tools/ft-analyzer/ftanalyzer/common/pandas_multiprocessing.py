"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2024 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Run Pandas DataFrame operations in parallel with multiprocessing.
"""

import logging
import multiprocessing
import time
from functools import partial
from typing import Callable

import pandas as pd

# Max processes to create in multiprocessing pool.
# It helps to reduce RAM usage. For example,
# it can be customized to match actual processor
# cores or to run on a single node.
MAX_PROC_COUNT = 40


class PandasMultiprocessingHelper:
    """Helper class to run Pandas DataFrame operations in parallel with multiprocessing.
    It helps with processing large DataFrames, e.g. converting string ip adresses
    to python ipaddress objects.
    """

    def __init__(self) -> None:
        self._pool = None
        self._number_of_chunks = 0

    def __enter__(self):
        proc_count = int(multiprocessing.cpu_count() / 2)
        proc_count = int(min(proc_count, MAX_PROC_COUNT))
        logging.getLogger().debug("Setting up pool with %d processes.", proc_count)
        self._pool = multiprocessing.Pool(processes=proc_count)
        self._number_of_chunks = proc_count * 4
        return self

    def __exit__(self, *exc):
        if self._pool is not None:
            self._pool.close()
            self._pool.join()

    def apply(self, df: pd.DataFrame, mappings: list[tuple[str, Callable, list]]) -> None:
        """Apply functions on a DataFrame columns.

        Parameters
        ----------
        df: pd.DataFrame
            DataFrame on applying.
        mappings: list[tuple[str, Callable, list]
            Mapping describing which function should be applied to which column in the form:
                (column name, function, additional args)
            Note: additional args are the same for each apply call.
        """

        if self._pool is None:
            raise RuntimeError("Multiprocessing method called outside of with statement.")

        for column_name, func, args in mappings:
            logging.getLogger().debug("Multiprocess apply...")
            start = time.time()
            if args:
                func = partial(func, *args)

            chunksize, extra = divmod(len(df[column_name]), self._number_of_chunks)
            if extra:
                chunksize += 1
            logging.getLogger().debug("Chunksize is %d", chunksize)
            res = self._pool.imap(func, df[column_name], chunksize)

            df[column_name] = pd.Series(data=res, index=df[column_name].index)

            end = time.time()
            logging.getLogger().debug("Processes applied in %.2f seconds.", (end - start))

    def binary(self, df: pd.DataFrame, mappings: list[tuple[str, Callable, str, str, list]]) -> None:
        """Apply binary functions on a DataFrame columns.

        Parameters
        ----------
        df: pd.DataFrame
            DataFrame on applying.
        mappings: list[tuple[str, Callable, list]
            Mapping describing which function should be applied to which column in the form:
                (target column name, function, operand column1, operand column2, additional args)
            Binary function gets values from operand column 1 and 2. Result is stored to target
            column.
            Note: additional args are the same for each apply call.
        """

        if self._pool is None:
            raise RuntimeError("Multiprocessing method called outside of with statement.")

        for column_target, func, column1, column2, args in mappings:
            logging.getLogger().debug("Multiprocess apply...")
            start = time.time()
            if args:
                func = partial(func, *args)
            res = self._pool.starmap_async(func, zip(df[column1], df[column2]))

            df[column_target] = pd.Series(data=res.get(), index=df[column_target].index)

            end = time.time()
            logging.getLogger().debug("Processes applied in %.2f seconds.", (end - start))
