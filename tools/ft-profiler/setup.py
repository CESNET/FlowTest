#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Setup script with necessary information to create python package.
"""

from setuptools import setup

setup(
    name="ft-profiler",
    version="1.0.0",
    description="Tool for creating network profile from netflow data in a specified time interval.",
    author="Flowmon Networks a.s.",
    python_requires=">=3.6",
    packages=["profiler"],
    package_dir={"profiler": "src"},
)
