#!/usr/bin/env python3
# -*- coding: utf-8 -*-

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
