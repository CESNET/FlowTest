# pylint: disable=invalid-name
# pylint: disable=protected-access
"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Unit tests for ftprofiler - cache.py.
"""

import random
import copy
from hypothesis import given, example
import hypothesis.strategies as st
from test_flow import GenerateFlows
from ftprofiler.cache import FlowCache
from ftprofiler.flow import Flow


class GenerateCache:
    """Helper class for flow cache generation"""

    MEM_1M = 1 * 1048576 // Flow.SIZE
    MEM_100M = 100 * 1048576 // Flow.SIZE
    MEM_1G = 1000 * 1048576 // Flow.SIZE
    MEM_10G = 10000 * 1048576 // Flow.SIZE

    @staticmethod
    def given_params(
        it: (int, int) = (1000, 5000000), at: (int, int) = (4000, 6000000), lim: (int, int) = (MEM_100M, MEM_10G)
    ):
        """Helper for Hypothesis generator"""
        return st.integers(it[0], it[1]), st.integers(at[0], at[1]), st.integers(lim[0], lim[1])

    @staticmethod
    def create_cache(
        t_start: int, t_end: int, proto: int, s_addr: str, d_addr: str, s_port: int, d_port: int, pkts: int, bts: int
    ) -> (str, str, Flow):
        """Helper for creating cache with addresses"""
        return s_addr, d_addr, Flow(t_start, t_end, proto, s_addr, d_addr, s_port, d_port, pkts, bts)

    @staticmethod
    def gen_unique_flow(flow: Flow = None) -> Flow:
        """Generate a unique Flow related to given one"""
        new_flow = GenerateFlows.gen_random_flow()
        if not flow:
            return new_flow
        while hash(new_flow) == hash(flow):
            new_flow = GenerateFlows.gen_random_flow()
        return new_flow

    @staticmethod
    def fill_cache_unique(flownr: int, cache: FlowCache) -> int:
        """Fill cache with unique Flows"""
        max_tend = 0
        for _ in range(0, flownr):
            flow = GenerateCache.gen_unique_flow()
            if flow.end_time > max_tend:
                max_tend = flow.end_time
            assert cache.add_flow(flow) == []
        return max_tend

    @staticmethod
    def copy_flow_nonkey(flow1: Flow) -> (Flow, int):
        """Deep copy flow and randomize its nonkey parameters"""
        flow2 = copy.deepcopy(flow1)
        flow2.start_time = flow1.start_time + random.randint(1000, 30000)
        flow2.start_time = flow2.start_time + random.randint(1000, 30000)
        flow2.packets = random.randint(1, 9999)
        flow2.bytes = random.randint(64, 99999)
        f2_hash = hash(flow2)
        assert hash(flow1) == f2_hash
        return flow2, f2_hash


class TestCache:
    """Class for pytest cache.py unit tests."""

    @staticmethod
    @given(*GenerateCache.given_params())
    @example(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, GenerateCache.MEM_1G)
    def test_init(inactive_timeout: int, active_timeout: int, lim: int):
        """Test __init__()"""
        cache = FlowCache(inactive_timeout, active_timeout, lim)
        assert cache._it == inactive_timeout
        assert cache._at == active_timeout
        assert cache._limit == lim
        assert cache._now == 0
        assert cache._cache == {}

    @staticmethod
    def test_addflow_endtime():
        """Test add_flow() and check that maximum end time across all processed flows is correctly set"""
        nr_items = 1000
        cache = FlowCache(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, GenerateCache.MEM_1G)
        max_tend = GenerateCache.fill_cache_unique(nr_items, cache)
        assert cache._now == max_tend

    @staticmethod
    def test_addflow_flow_update(monkeypatch):
        """Test add_flow() - check that Flow method update() is used when inserting
        new flow equal to already existing flow in cache."""
        monkeypatch.setattr(Flow, "update", lambda *_: True)
        cache = FlowCache(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, GenerateCache.MEM_1G)
        flow1 = GenerateCache.gen_unique_flow()
        assert cache.add_flow(flow1) == []
        flow2, f2_hash = GenerateCache.copy_flow_nonkey(flow1)
        assert f2_hash in cache._cache
        assert cache.add_flow(flow2) == []
        assert len(cache._cache) == 1

    @staticmethod
    def test_addflow_replace_flow(monkeypatch):
        """Test add_flow() - add flow, then add second flow replacing original flow in cache."""
        monkeypatch.setattr(Flow, "update", lambda *_: False)
        cache = FlowCache(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, GenerateCache.MEM_1G)
        flow1 = GenerateCache.gen_unique_flow()
        assert cache.add_flow(flow1) == []
        flow2, f2_hash = GenerateCache.copy_flow_nonkey(flow1)
        assert f2_hash in cache._cache

        add_list = cache.add_flow(flow2)
        assert len(add_list) == 1
        assert add_list[0] == flow1

        # check that flow2 is stored in cache - operation == cannot be used (uses hash)
        assert len(cache._cache) == 1
        assert cache._cache[f2_hash].packets == flow2.packets
        assert cache._cache[f2_hash].bytes == flow2.bytes

    @staticmethod
    def test_addflow_to_full_cache():
        """Test add_flow() - add one flow to full cache and expect some flows to be removed from cache."""
        max_flows = GenerateCache.MEM_1M  # 4096
        cache = FlowCache(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, max_flows)
        # add flow cache up to the limit; 4095 flows
        GenerateCache.fill_cache_unique(max_flows - 1, cache)
        assert len(cache._cache) == max_flows - 1

        # add flow over cache limit; >= 4096
        removed_flows = cache.add_flow(GenerateCache.gen_unique_flow())
        assert len(removed_flows) > 0
        assert len(cache._cache) == max_flows - len(removed_flows)

    @staticmethod
    def test_removeflows_delete_all():
        """Test remove_flows() - remove all flows from cache and get back all removed flows."""
        nr_items = 1000
        cache = FlowCache(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, GenerateCache.MEM_1G)
        GenerateCache.fill_cache_unique(nr_items, cache)
        allflows = cache.remove_flows(None)

        assert len(allflows) == nr_items
        assert cache._cache == {}

    @staticmethod
    def test_removeflows_delete_by_threshold():
        """Test remove_flows() - remove flows based on their end time threshold"""
        nr_items = 1000
        cache = FlowCache(GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE, GenerateCache.MEM_1G)
        _ = GenerateCache.fill_cache_unique(nr_items, cache)
        threshold = random.randint(600000, 1000000)
        thr_flows = cache.remove_flows(threshold)

        assert len(thr_flows) > 0, "No flows have been removed. False positive case is very unlikely to happen."
        assert {k: v for k, v in cache._cache.items() if v.end_time < threshold} == {}
        assert len({k: v for k, v in cache._cache.items() if v.end_time >= threshold}) == nr_items - len(thr_flows)
        assert len(cache._cache) == nr_items - len(thr_flows)
