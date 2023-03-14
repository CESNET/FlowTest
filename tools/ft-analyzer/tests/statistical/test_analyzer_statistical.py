"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""
import os

import pytest
from ftanalyzer.models import (
    StatisticalModel as SMod,
    SMRule,
    SMMetric,
    SMMetricType,
    SMSubnetSegment,
    SMTimeSegment,
    SMException,
)

BASE_PATH = os.path.dirname(os.path.realpath(__file__))
FLOWS_PATH = os.path.join(BASE_PATH, "flows")
REF_PATH = os.path.join(BASE_PATH, "references")


def test_basic():
    """Test basic functionality on the same data but shuffled."""
    model = SMod(os.path.join(FLOWS_PATH, "shuffled_basic.csv"), os.path.join(REF_PATH, "basic.csv"), (300, 30))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    report = model.validate([SMRule(metrics=metrics)])
    report.print_results()
    assert report.is_passing() is True


def test_long_flows():
    """Test the ability to merge flows which were divided by active timeout."""
    model = SMod(os.path.join(FLOWS_PATH, "long_flows_split.csv"), os.path.join(REF_PATH, "long_flows.csv"), (300, 30))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    report = model.validate([SMRule(metrics=metrics)])
    report.print_results()
    assert report.is_passing() is True


def test_incomplete():
    """Test different acceptable relative differences when some data is missing."""
    model = SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "basic.csv"), (300, 30))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0.1),
        SMMetric(SMMetricType.BYTES, 0.1),
        SMMetric(SMMetricType.FLOWS, 0.01),
    ]

    report = model.validate([SMRule(metrics)])
    report.print_results()
    assert report.is_passing() is False

    metrics = [
        SMMetric(SMMetricType.PACKETS, 0.1),
        SMMetric(SMMetricType.BYTES, 0.1),
        SMMetric(SMMetricType.FLOWS, 0.1),
    ]

    report = model.validate([SMRule(metrics)])
    report.print_results()
    assert report.is_passing() is True


def test_subnet_segment():
    """Test dividing input data into segments by subnets."""
    model = SMod(os.path.join(FLOWS_PATH, "long_flows_split.csv"), os.path.join(REF_PATH, "long_flows.csv"), (300, 30))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    segment = SMSubnetSegment(source="192.168.187.0/24", dest="212.24.128.0/24", bidir=True)
    report = model.validate([SMRule(metrics, segment), SMRule(metrics)])
    report.print_results()
    assert report.is_passing() is True


def test_single_subnet():
    """Test scenario when we only need to apply metrics on a specific subnet, because other subnets
    can have tons of missing data.
    """
    model = SMod(
        os.path.join(FLOWS_PATH, "long_flows_missing.csv"), os.path.join(REF_PATH, "long_flows.csv"), (300, 30)
    )
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    report = model.validate([SMRule(metrics)])
    report.print_results()
    assert report.is_passing() is False
    assert report.get_test(SMMetricType.PACKETS).is_passing() is False
    assert report.get_test(SMMetricType.BYTES).is_passing() is False
    assert report.get_test(SMMetricType.FLOWS).is_passing() is True

    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    segment = SMSubnetSegment(source="192.168.187.0/24", dest="212.24.128.0/24", bidir=True)
    report = model.validate([SMRule(metrics, segment)])
    report.print_results()
    assert report.is_passing() is True


def test_time_segment():
    """Test dividing input data into segments by time intervals."""
    model = SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "basic.csv"), (300, 30))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    segment = SMTimeSegment(start="2023-03-08 21:51:20.000", end="2023-03-08 21:51:22.000")
    report = model.validate([SMRule(metrics, segment)])
    report.print_results()
    assert report.is_passing() is True

    report = model.validate([SMRule(metrics)])
    report.print_results()
    assert report.is_passing() is False


def test_source_file_error():
    """Test using multiple same metrics in a single rule."""
    with pytest.raises(SMException):
        SMod(os.path.join(FLOWS_PATH, "non-existent.csv"), os.path.join(REF_PATH, "basic.csv"), (300, 30))

    with pytest.raises(SMException):
        SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "non-existent.csv"), (300, 30))

    with pytest.raises(SMException):
        SMod(os.path.join(FLOWS_PATH, "malformed.csv"), os.path.join(REF_PATH, "basic.csv"), (300, 30))


def test_multiple_same_metrics():
    """Test using multiple same metrics in a single rule."""
    model = SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "basic.csv"), (300, 30))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.PACKETS, 0.1),
    ]

    with pytest.raises(SMException):
        model.validate([SMRule(metrics)])


def test_wrong_difference():
    """Test values of metrics relative difference which are not within the 0.0 - 1.0 interval."""
    with pytest.raises(ValueError):
        SMMetric(SMMetricType.PACKETS, -1.0)

    with pytest.raises(ValueError):
        SMMetric(SMMetricType.PACKETS, 1.1)
