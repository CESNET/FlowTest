#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Setup script with necessary information to create python package.
"""

import setuptools

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setuptools.setup(
    name="ftanalyzer",
    version="1.0.0",
    author="Flowmon Networks a.s.",
    author_email="support@flowmon.com",
    description="Library evaluating network flows. It contains different flow validation models.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: BSD-3-Clause",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.6",
    packages=setuptools.find_packages(),
)
