"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""

import os
from datetime import datetime, timezone

import pytest
from ftanalyzer.models import PreciseModel as PMod
from ftanalyzer.models import (
    SMMetric,
    SMMetricType,
    SMRule,
    SMSubnetSegment,
    SMTimeSegment,
)

BASE_PATH = os.path.dirname(os.path.realpath(__file__))
FLOWS_PATH = os.path.join(BASE_PATH, "flows")
REF_PATH = os.path.join(BASE_PATH, "references")


def test_shuffled():
    """Test basic functionality on the same data but shuffled."""

    model = PMod(os.path.join(FLOWS_PATH, "base_shuffle.csv"), os.path.join(REF_PATH, "base.csv"), 300)
    report = model.validate_precise()
    report.print_results()

    assert report.is_passing() is True


def test_subnet_segment():
    """Test basic functionality in a specified subnet segment on the same data but shuffled."""

    model = PMod(os.path.join(FLOWS_PATH, "base_shuffle.csv"), os.path.join(REF_PATH, "base.csv"), 300)
    segment1 = SMSubnetSegment(source="192.168.51.0/24", dest="81.2.248.0/24", bidir=True)
    segment2 = SMSubnetSegment(dest="ff02::2")
    segment3 = SMSubnetSegment(source="10.100.56.132", dest="37.187.104.44", bidir=True)
    report = model.validate_precise([None, segment1, segment2, segment3])
    report.print_results()

    assert report.is_passing() is True


def test_time_segment():
    """Test basic functionality in a specified time segment on the same data but shuffled."""

    model = PMod(os.path.join(FLOWS_PATH, "base_shuffle.csv"), os.path.join(REF_PATH, "base.csv"), 300)

    tstart = datetime(2023, 3, 8, 21, 50, 00, 0, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 55, 0, 0, timezone.utc)

    segment1 = SMTimeSegment(start=tstart, end=tend)
    segment2 = SMTimeSegment(start=tstart)
    segment3 = SMTimeSegment(end=tend)
    report = model.validate_precise([None, segment1, segment2, segment3])
    report.print_results()

    assert report.is_passing() is True


def test_missing():
    """Test that missing flows are correctly reported."""

    model = PMod(os.path.join(FLOWS_PATH, "small_missing.csv"), os.path.join(REF_PATH, "small.csv"), 300)

    segment1 = SMSubnetSegment(source="10.100.40.0/24", dest="37.186.104.0/24")
    segment2 = SMSubnetSegment(source="10.100.40.0/24", dest="37.187.104.0/24")
    segment3 = SMSubnetSegment(source="10.100.40.0/24", dest="37.188.104.0/24")
    report = model.validate_precise([None, segment1, segment2, segment3])
    report.print_results()

    assert report.is_passing() is False
    assert len(report.get_test().missing) == 2
    assert report.get_test(segment1).is_passing() is True
    assert len(report.get_test(segment2).missing) == 1
    assert len(report.get_test(segment3).missing) == 1


def test_unexpected():
    """Test that unexpected flows are correctly reported."""

    model = PMod(os.path.join(FLOWS_PATH, "small_unexpected.csv"), os.path.join(REF_PATH, "small.csv"), 300)

    segment1 = SMSubnetSegment(source="10.100.40.0/24", dest="37.186.104.0/24")
    segment2 = SMSubnetSegment(source="10.100.40.0/24", dest="37.187.104.0/24")
    segment3 = SMSubnetSegment(source="10.100.40.0/24", dest="37.189.104.0/24")
    report = model.validate_precise([None, segment1, segment2, segment3])
    report.print_results()

    assert report.is_passing() is False
    assert len(report.get_test().unexpected) == 2
    assert report.get_test(segment1).is_passing() is True
    assert len(report.get_test(segment2).unexpected) == 1
    assert len(report.get_test(segment3).unexpected) == 1


def test_incorrect_values():
    """Test that flows which have missmatch in packet / bytes are reported."""

    model = PMod(os.path.join(FLOWS_PATH, "small_incorrect.csv"), os.path.join(REF_PATH, "small.csv"), 300)
    report = model.validate_precise()
    report.print_results()

    assert report.is_passing() is False
    assert len(report.get_test().scaled) == 4


def test_incorrect_timestamps():
    """
    Test that maximum time difference to pair reference flow and flow from probe is working correctly.
    Test that flows which timestamps differ more than allowed threshold (but not pairing limit) are reported.
    """

    model = PMod(os.path.join(FLOWS_PATH, "timestamps.csv"), os.path.join(REF_PATH, "small.csv"), 300)
    report = model.validate_precise()
    report.print_results()

    assert report.is_passing() is False
    test = report.get_test()
    assert len(test.shifted) == 4

    report = model.validate_precise(ok_time_diff=10)
    report.print_results()

    assert report.is_passing() is False
    test = report.get_test()
    assert len(test.shifted) == 5

    report = model.validate_precise(ok_time_diff=10000)
    report.print_results()

    assert report.is_passing() is True


def test_mixed():
    """Test statistical validation on the precise model."""

    model = PMod(os.path.join(FLOWS_PATH, "base_shuffle.csv"), os.path.join(REF_PATH, "base.csv"), 300)
    metrics = [
        SMMetric(SMMetricType.PACKETS, 0),
        SMMetric(SMMetricType.BYTES, 0),
        SMMetric(SMMetricType.FLOWS, 0),
    ]

    report = model.validate([SMRule(metrics=metrics)])
    report.print_results()

    assert report.is_passing() is True


def test_same_segment():
    """Test adding the same segment multiple times."""

    model = PMod(os.path.join(FLOWS_PATH, "base_shuffle.csv"), os.path.join(REF_PATH, "base.csv"), 300)
    with pytest.raises(ValueError):
        model.validate_precise([None, None])

    segment1 = SMSubnetSegment(source="192.168.51.0/24", dest="81.2.248.0/24", bidir=True)
    with pytest.raises(ValueError):
        model.validate_precise([segment1, segment1])

    tstart = datetime(2023, 3, 8, 21, 50, 00, 0, timezone.utc)
    tend = datetime(2023, 3, 8, 21, 55, 0, 0, timezone.utc)
    segment2 = SMTimeSegment(start=tstart, end=tend)
    with pytest.raises(ValueError):
        model.validate_precise([segment2, segment2])

    with pytest.raises(ValueError):
        model.validate_precise([None, segment1, segment2, None])


def test_ip_version_compare():
    """Test situation when IPv4 is internally compared with IPv6."""

    model = PMod(
        os.path.join(FLOWS_PATH, "ip_version_compare.csv"),
        os.path.join(REF_PATH, "ip_version_compare.csv"),
        300,
        1692691223212,
    )
    report = model.validate_precise()
    report.print_results()

    assert report.is_passing() is False
