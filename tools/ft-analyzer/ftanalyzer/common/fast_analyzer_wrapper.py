"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2025 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Function and classes for handling calls and data between ft-analyzer and ft-fast-analyzer.
"""

# Global variable is used for dynamic import of ft_fast_analyzer module.
# pylint: disable=global-statement

# Global variable with optional imported ft_fast_analyzer is used as type.
# pylint: disable=undefined-variable

# Import is used outside toplevel for dynamic import of ft_fast_analyzer module.
# pylint: disable=import-outside-toplevel

import datetime
import logging
import os
import sys
import sysconfig
from tempfile import TemporaryDirectory
from typing import Optional, Union

import pandas as pd
from ftanalyzer.models.sm_data_types import (
    SMException,
    SMMetric,
    SMMetricType,
    SMRule,
    SMSubnetSegment,
    SMTestOutcome,
    SMTimeSegment,
)
from ftanalyzer.reports.statistical_report import StatisticalReport

SITE_PACKAGES_PREFIX = sysconfig.get_path("stdlib")
GLOBAL_IMPORT_DIR = f"{SITE_PACKAGES_PREFIX}/site-packages/flowtest"


def fast_analyzer_available() -> bool:
    """
    Check if the ft_fast_analyzer module is available.

    Returns
    -------
    bool
        True if ft_fast_analyzer is available, False otherwise.
    """
    try:
        if os.path.exists(GLOBAL_IMPORT_DIR) and not os.path.normpath(GLOBAL_IMPORT_DIR) in [
            os.path.normpath(p) for p in sys.path
        ]:
            sys.path.append(GLOBAL_IMPORT_DIR)
        global ft_fast_analyzer
        import ft_fast_analyzer

        logging.getLogger().info("ft_fast_analyzer loaded")
        return True
    except ImportError:
        logging.getLogger().warning("ft_fast_analyzer not found")
        return False


def create_statistical_model(
    flows: str,
    reference: Union[str, pd.DataFrame],
    start_time: int = 0,
) -> "ft_fast_analyzer.StatisticalModel":
    """
    Create a statistical model from ft_fast_analyzer.

    Parameters
    ----------
    flows : str
        Path to the flows file.
    reference : Union[str, pd.DataFrame]
        Reference data for the model. Can be a file path or DataFrame.
    start_time : int, optional
        Start time for the model, by default 0.

    Returns
    -------
    ft_fast_analyzer.StatisticalModel
        The created statistical model.

    Raises
    ------
    SMException
        If the model creation fails.
    """
    with TemporaryDirectory() as tmpdir:
        if isinstance(reference, pd.DataFrame):
            ref_file = os.path.join(tmpdir, "ref.csv")
            reference.to_csv(ref_file, index=False)
            reference = ref_file

        try:
            return ft_fast_analyzer.StatisticalModel(flows, reference, start_time)
        except Exception as err:
            raise SMException("Unable to create ft_fast_analyzer StatisticalModel.") from err


def validate_statistical_model(
    model: "ft_fast_analyzer.StatisticalModel", rules: list[SMRule], check_complement: bool
) -> StatisticalReport:
    """
    Validate a statistical model using a set of rules.

    Parameters
    ----------
    model : ft_fast_analyzer.StatisticalModel
        The statistical model to validate.
    rules : list[SMRule]
        List of rules to validate the model against.
    check_complement : bool
        Whether to check the complement of the rules.

    Returns
    -------
    StatisticalReport
        The validation report.
    """
    rules2 = list(map(_convert_rule, rules))
    report = model.Validate(rules2, check_complement)
    return _convert_report(report)


def _py2metric_type(metric_type: SMMetricType) -> "ft_fast_analyzer.SMMetricType":
    if metric_type == SMMetricType.PACKETS:
        return ft_fast_analyzer.SMMetricType.PACKETS
    if metric_type == SMMetricType.BYTES:
        return ft_fast_analyzer.SMMetricType.BYTES
    return ft_fast_analyzer.SMMetricType.FLOWS


def _metric_type2py(metric_type: "ft_fast_analyzer.SMMetricType") -> SMMetricType:
    if metric_type == ft_fast_analyzer.SMMetricType.PACKETS:
        return SMMetricType.PACKETS
    if metric_type == ft_fast_analyzer.SMMetricType.BYTES:
        return SMMetricType.BYTES
    return SMMetricType.FLOWS


def _py2time_segment(segment: SMTimeSegment) -> "ft_fast_analyzer.TimeSegment":
    res = ft_fast_analyzer.SMTimeSegment()
    if isinstance(segment.start, datetime.datetime):
        res.start = int(segment.start.timestamp() * 1000)
    if isinstance(segment.end, datetime.datetime):
        res.end = int(segment.end.timestamp() * 1000)
    return res


def _py2subnet_segment(segment: SMSubnetSegment) -> "ft_fast_analyzer.SMSubnetSegment":
    res = ft_fast_analyzer.SMSubnetSegment()
    res.bidir = segment.bidir
    if isinstance(segment.source, str):
        ip, prefix = segment.source.split("/", maxsplit=1)
        res.source = ft_fast_analyzer.IPNetwork(ip, int(prefix))
    if isinstance(segment.dest, str):
        ip, prefix = segment.dest.split("/", maxsplit=1)
        res.dest = ft_fast_analyzer.IPNetwork(ip, int(prefix))
    return res


def _convert_rule(rule: SMRule) -> "ft_fast_analyzer.SMRule":
    # Check duplicated metrics.
    if len({m.key for m in rule.metrics}) != len(rule.metrics):
        raise SMException(f"Rule contains duplicated metrics: {rule.metrics}")

    metrics = []
    for m in rule.metrics:
        mm = ft_fast_analyzer.SMMetric()
        mm.key = _py2metric_type(m.key)
        mm.diff = m.diff
        metrics.append(mm)

    if isinstance(rule.segment, SMSubnetSegment):
        segment = _py2subnet_segment(rule.segment)
        return ft_fast_analyzer.SMRule(metrics, segment)
    if isinstance(rule.segment, SMTimeSegment):
        segment = _py2time_segment(rule.segment)
        return ft_fast_analyzer.SMRule(metrics, segment)
    return ft_fast_analyzer.SMRule(metrics)


def _convert_segment(segment: "ft_fast_analyzer.SMSegment") -> Optional[Union[SMSubnetSegment, SMTimeSegment, str]]:
    if isinstance(segment, ft_fast_analyzer.SMSubnetSegment):
        return SMSubnetSegment(
            source=str(segment.source) if segment.source else None,
            dest=str(segment.dest) if segment.dest else None,
            bidir=segment.bidir,
        )
    if isinstance(segment, ft_fast_analyzer.SMTimeSegment):
        return SMTimeSegment(
            start=datetime.datetime.fromtimestamp(segment.start / 1000.0) if segment.start else None,
            end=datetime.datetime.fromtimestamp(segment.end / 1000.0) if segment.end else None,
        )
    if isinstance(segment, ft_fast_analyzer.SMComplementSegment):
        return "COMPLEMENT OF SEGMENTS"
    return None


def _convert_report(report: "ft_fast_analyzer.StatisticalReport") -> StatisticalReport:
    res = StatisticalReport()
    for test in report.tests:
        metric = SMMetric(key=_metric_type2py(test.metric.key), diff=test.metric.diff)
        res.add_test(SMTestOutcome(metric, _convert_segment(test.segment), test.value, test.reference, test.diff))
    return res
