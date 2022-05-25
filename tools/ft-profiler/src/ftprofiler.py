#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Script entry-point.
"""

import sys


if __name__ == "__main__":
    from ftprofiler.core import main

    sys.exit(main())
