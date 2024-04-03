"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for FlowReplicator tool. Tool is used to replicate flows in reference CSV file.
"""

import filecmp
import os

import pytest
from ftanalyzer.replicator import FlowReplicator
from ftanalyzer.replicator.flow_replicator import FlowReplicatorException

BASE_PATH = os.path.dirname(os.path.realpath(__file__))
SOURCE_PATH = os.path.join(BASE_PATH, "source")
REF_PATH = os.path.join(BASE_PATH, "ref")
TMP_CSV = os.path.join(BASE_PATH, "tmp.csv")


def test_basic():
    """Test of FlowReplicator. Source contains three flows with real unix timestamps."""

    config = {
        "units": [{"srcip": "addConstant(10)"}, {"dstip": "addConstant(256)"}],
        "loop": {"dstip": "addOffset(30)"},
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "basic.csv"), loops=3)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "basic.csv"))


def test_merging():
    """Test of merging replicated flows within one loop. If the replication units do not change source
    or destination ip address, packets belong to the same flow.
    """

    # three flows from the source must be preserved, bytes and packets must be aggregated (sum)

    config = {
        "units": [{"srcip": "None"}, {}],
        "loop": {},
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "basic.csv"), loops=2)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "merge.csv"))


def test_merging_across_loops():
    """Test of feature that provides merging flows across replay loops. This feature simulates a situation
    where the probe is unable to detect the end of the flow: the flow continues into next loop. Flows are
    merged according to the flow key, but the inactive timeout is taken into account.
    """

    # one flow in source csv, no loop/replicator unit modifier and no time gap between flows
    #   ==> 1 merged output flow

    config = {
        "units": [{}],
        "loop": {},
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "merge_across_simple.csv"), loops=4, merge_across_loops=True)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "merge_across_simple.csv"))

    # one flow source has no gap between loops ==> single flow
    # second flow has gap 30s = inactive timeout ==> leave flow unmerged

    config = {"units": [{}]}
    replicator = FlowReplicator(config)
    res = replicator.replicate(
        os.path.join(SOURCE_PATH, "merge_across_timeout.csv"), 4, merge_across_loops=True, inactive_timeout=30
    )
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "merge_across_timeout.csv"))


def test_merging_across_loops_headache():
    """Complex test of merging replicated flows where flow keys interfere across replication loops.

    Three flows with consecutive source ip addresses are replicated and the source ip address is incremented.
    See graphical representation below.
    """

    #    loop      1.   2.   3.
    # 10.0.0.1 : |----|    |    |
    # 10.0.0.2 : |  --|----|    |
    # 10.0.0.3 : |--- |  --|----|
    # 10.0.0.4 : |    |--- |  --|
    # 10.0.0.5 : |    |    |--- |
    #
    # it can be seen that the flows "10.0.0.3" and "10.0.0.4" have a time gap greater
    # than the inactive timeout (3 ticks = 30s), so the flows will be unmerged

    config = {
        "units": [{}],
        "loop": {"srcip": "addOffset(1)"},
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(
        os.path.join(SOURCE_PATH, "merge_across_headache.csv"), 3, merge_across_loops=True, inactive_timeout=29
    )
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "merge_across_headache.csv"))


def test_bad_config():
    """Test of raise FlowReplicatorException when configuration dict has unknown key or unsupported modifier is used."""

    config = {
        "loop": {"dstip": "addOffset(30)"},
        "bad_conf_key": [1, 2, 3],
    }

    with pytest.raises(FlowReplicatorException):
        FlowReplicator(config)

    config = {
        "units": [{"srcip": "addConstant(10)"}, {"dstip": "addCounter(10, 2)"}],
        "loop": {"dstip": "addOffset(30)"},
    }

    with pytest.raises(FlowReplicatorException):
        FlowReplicator(config)


def test_loop_only():
    """Test with replication unit that is not active in all loops."""

    config = {
        "units": [
            {"srcip": "addConstant(10)", "loopOnly": [0, 1]},
            {"dstip": "addConstant(256)"},
        ],
        "loop": {"dstip": "addOffset(30)"},
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "basic.csv"), loops=3)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "basic_loop_only.csv"))


def test_ignore_loop():
    """Test of ignoring (skipping) loop. Unsupported modifiers may be used."""

    config = {
        "units": [
            {"srcip": "addCounter(10,2)", "loopOnly": [0]},
            {"dstip": "addConstant(256)"},
        ],
        "loop": {"dstip": "addOffset(30)"},
    }
    replicator = FlowReplicator(config, ignore_loops=[0])
    res = replicator.replicate(os.path.join(SOURCE_PATH, "basic.csv"), loops=3)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "basic_ignore_loop.csv"))


def test_ipv6_adding():
    """Test of custom IPv6 address adding. Only first 4 bytes must be edited."""

    config = {
        "units": [
            {
                "srcip": f"addConstant({2**30})",
                "dstip": "addConstant(2)",
            }
        ]
    }

    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "ipv6.csv"), loops=1)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "ipv6.csv"))


def test_speed_multiplier_2():
    """Test of timestamps multiplication. 2.0 means that flows will take half the time."""

    config = {
        "units": [{"srcip": "addConstant(10)"}, {"dstip": "addConstant(256)"}],
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "basic.csv"), loops=3, speed_multiplier=2.0)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "basic_time_2.0.csv"))


def test_speed_multiplier_0_8():
    """Test of timestamps multiplication. 0.8 means that flows will take 1.25 as long."""

    config = {
        "units": [{"srcip": "addConstant(10)"}, {"dstip": "addConstant(256)"}],
    }
    replicator = FlowReplicator(config)
    res = replicator.replicate(os.path.join(SOURCE_PATH, "basic.csv"), loops=2, speed_multiplier=0.8)
    res.to_csv(TMP_CSV, index=False)

    assert filecmp.cmp(TMP_CSV, os.path.join(REF_PATH, "basic_time_0.8.csv"))
