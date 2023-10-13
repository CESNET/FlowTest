#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Script entry-point.
"""

import sys

if __name__ == "__main__":
    from fttrimmer.fttrimmer import main

    sys.exit(main())
