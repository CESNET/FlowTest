"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""

import os
from datetime import datetime, timezone

import pytest
from ftanalyzer.models import (
    SMException,
    SMMetric,
    SMMetricType,
    SMRule,
    SMSubnetSegment,
    SMTimeSegment,
)
from ftanalyzer.models import StatisticalModel as SMod

BASE_PATH = os.path.dirname(os.path.realpath(__file__))
FLOWS_PATH = os.path.join(BASE_PATH, "flows")
REF_PATH = os.path.join(BASE_PATH, "references")


def test_basic():
    """Test basic functionality on the same data but shuffled."""
    model = SMod(os.path.join(FLOWS_PATH, "shuffled_basic.csv"), os.path.join(REF_PATH, "basic.csv"))
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
    model = SMod(os.path.join(FLOWS_PATH, "long_flows_split.csv"), os.path.join(REF_PATH, "long_flows.csv"), merge=True)
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
    model = SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "basic.csv"))
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
    model = SMod(os.path.join(FLOWS_PATH, "long_flows_split.csv"), os.path.join(REF_PATH, "long_flows.csv"), merge=True)
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
        os.path.join(FLOWS_PATH, "long_flows_missing.csv"), os.path.join(REF_PATH, "long_flows.csv"), merge=True
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
    model = SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "basic.csv"))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    tstart = datetime(2023, 3, 8, 21, 51, 20, 0, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 51, 22, 0, timezone.utc)
    segment = SMTimeSegment(start=tstart, end=tend)
    report = model.validate([SMRule(metrics, segment)])
    report.print_results()
    assert report.is_passing() is True

    report = model.validate([SMRule(metrics)])
    report.print_results()
    assert report.is_passing() is False


def test_source_file_error():
    """Test using multiple same metrics in a single rule."""
    with pytest.raises(SMException):
        SMod(os.path.join(FLOWS_PATH, "non-existent.csv"), os.path.join(REF_PATH, "basic.csv"))

    with pytest.raises(SMException):
        SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "non-existent.csv"))

    with pytest.raises(SMException):
        SMod(os.path.join(FLOWS_PATH, "malformed.csv"), os.path.join(REF_PATH, "basic.csv"))


def test_multiple_same_metrics():
    """Test using multiple same metrics in a single rule."""
    model = SMod(os.path.join(FLOWS_PATH, "basic_missing.csv"), os.path.join(REF_PATH, "basic.csv"))
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


def test_relative_time():
    """Test correct update of reference flows when start time is provided."""
    model = SMod(
        os.path.join(FLOWS_PATH, "relative_time.csv"),
        os.path.join(REF_PATH, "relative_time.csv"),
        start_time=1678312157497,
    )
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    tstart = datetime(2023, 3, 8, 21, 49, 22, 275000, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 49, 22, 483000, timezone.utc)

    segment = SMTimeSegment(start=tstart, end=tend)
    report = model.validate([SMRule(metrics, segment)])
    report.print_results()
    assert report.is_passing() is True

    segment = SMTimeSegment(start=tstart)
    report = model.validate([SMRule(metrics, segment)])
    report.print_results()
    assert report.is_passing() is True

    tend = datetime(2023, 3, 8, 21, 49, 27, 962000, timezone.utc)
    segment = SMTimeSegment(end=tend)
    report = model.validate([SMRule(metrics, segment)])
    report.print_results()
    assert report.is_passing() is True


def test_subnet_check_complement():
    """Test dividing input data into segments by subnets. Check that complement is empty."""

    model = SMod(os.path.join(FLOWS_PATH, "long_flows_split.csv"), os.path.join(REF_PATH, "long_flows.csv"), merge=True)
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    segment1 = SMSubnetSegment(source="192.168.187.0/24", dest="212.24.128.0/24", bidir=True)
    segment2 = SMSubnetSegment(source="5.180.196.0/24", bidir=True)
    report = model.validate([SMRule(metrics, segment1), SMRule(metrics, segment2)], check_complement=True)
    report.print_results()
    assert report.is_passing() is True


def test_subnet_check_complement_failure():
    """Test dividing input data into segments by subnets. Complement is not empty."""

    model = SMod(os.path.join(FLOWS_PATH, "long_flows_split.csv"), os.path.join(REF_PATH, "long_flows.csv"), merge=True)
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    segment = SMSubnetSegment(source="192.168.187.0/24", dest="212.24.128.0/24", bidir=True)
    report = model.validate([SMRule(metrics, segment)], check_complement=True)
    report.print_results()
    assert report.is_passing() is False


def test_time_segment_check_complement():
    """Test dividing input data into segments by time intervals. Check that complement is empty."""

    model = SMod(os.path.join(REF_PATH, "basic.csv"), os.path.join(REF_PATH, "basic.csv"))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    tstart = datetime(2023, 3, 8, 21, 46, 29, 0, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 52, 37, 0, timezone.utc)
    segment1 = SMTimeSegment(start=tstart, end=tend)
    tstart = datetime(2023, 3, 8, 21, 45, 13, 0, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 51, 20, 0, timezone.utc)
    segment2 = SMTimeSegment(start=tstart, end=tend)
    report = model.validate([SMRule(metrics, segment1), SMRule(metrics, segment2)], check_complement=True)
    report.print_results()
    assert report.is_passing()


def test_time_segment_check_complement_failure():
    """Test dividing input data into segments by time intervals. Complement is not empty."""

    model = SMod(os.path.join(REF_PATH, "basic.csv"), os.path.join(REF_PATH, "basic.csv"))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    tstart = datetime(2023, 3, 8, 21, 46, 29, 0, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 52, 37, 0, timezone.utc)
    segment1 = SMTimeSegment(start=tstart, end=tend)
    report = model.validate([SMRule(metrics, segment1)], check_complement=True)
    report.print_results()
    assert report.is_passing() is False


def test_check_complement_all_data():
    """Irrelevant but supported usage of check complement.
    Complement of ALL DATA is always empty.
    """

    model = SMod(os.path.join(FLOWS_PATH, "shuffled_basic.csv"), os.path.join(REF_PATH, "basic.csv"))
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    report = model.validate([SMRule(metrics=metrics)], check_complement=True)
    report.print_results()
    assert report.is_passing() is True
