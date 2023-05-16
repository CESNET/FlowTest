# pylint: disable=protected-access
"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Unit tests for ftprofiler - flow.py.
"""
import copy
import random
import socket
import struct
from ipaddress import IPv4Address, IPv6Address
from typing import List, Union
from unittest.mock import patch

import hypothesis.strategies as st
import pytest
from ftprofiler.flow import Flow
from hypothesis import HealthCheck, assume, given, settings


class GenerateFlows:
    """Helper class for flow generation and process"""

    MILI = 1000
    DEFAULT_ACTIVE = 300 * MILI
    DEFAULT_INACTIVE = 30 * MILI

    # pylint: disable=unnecessary-lambda
    @staticmethod
    def strategy_one_flow():
        """Hypothesis strategies for generating one flow.
        s_addr and d_addr return list consisting of IPv4 and IPv6 address."""
        return {
            "t_start": st.integers(0, 1000000),
            "t_end": st.integers(0, 1000000),
            "proto": st.integers(0, 255),
            "s_addr": st.lists(st.ip_addresses(), min_size=2, max_size=2, unique_by=lambda x: type(x)),
            "d_addr": st.lists(st.ip_addresses(), min_size=2, max_size=2, unique_by=lambda x: type(x)),
            "s_port": st.integers(0, 65535),
            "d_port": st.integers(0, 65535),
            "pkts": st.integers(0, 1000000),
            "bts": st.integers(0, 1000000000),
        }

    @staticmethod
    def strategy_two_flows():
        """Hypothesis strategies for generating two flows - flows should differ"""
        return {
            "t_start": st.integers(0, 1000000),
            "t_end": st.integers(0, 1000000),
            "proto": st.integers(0, 255),
            "s_addr": st.lists(st.ip_addresses(), min_size=2, max_size=2, unique_by=lambda x: type(x)),
            "d_addr": st.lists(st.ip_addresses(), min_size=2, max_size=2, unique_by=lambda x: type(x)),
            "s_port": st.integers(0, 65535),
            "d_port": st.integers(0, 65535),
            "pkts": st.integers(0, 1000000),
            "bts": st.integers(0, 1000000000),
            "f2_t_start": st.integers(0, 1000000),
            "f2_t_end": st.integers(0, 1000000),
            "f2_proto": st.integers(0, 255),
            "f2_s_addr": st.lists(st.ip_addresses(), min_size=2, max_size=2, unique_by=lambda x: type(x)),
            "f2_d_addr": st.lists(st.ip_addresses(), min_size=2, max_size=2, unique_by=lambda x: type(x)),
            "f2_s_port": st.integers(0, 65535),
            "f2_d_port": st.integers(0, 65535),
            "f2_pkts": st.integers(0, 1000000),
            "f2_bts": st.integers(0, 1000000000),
        }

    @staticmethod
    def gen_random_flow() -> Flow:
        """Generate random Flow with features:
        * IP addresses of the same type (IPv4, IPv6) given that source and destination addresses differ
        * IPv4 addresses are more likely to be generated than IPv6 addresses
        * UDP (17) and TCP (6) protocols are more likely to be generated
        * End time of flow is always larger than start time of flow."""

        def gen_ip(ver: int = 4) -> str:
            if ver == 4:
                return socket.inet_ntop(socket.AF_INET, struct.pack(">I", random.randint(1, 0xFFFFFFFF)))
            if ver == 6:
                return socket.inet_ntop(
                    socket.AF_INET6, struct.pack(">QQ", random.getrandbits(64), random.getrandbits(64))
                )
            assert False, "Unsupported v param!"

        t_start = random.randint(0, 1000000)
        t_end = t_start + random.randint(0, 1000000)
        proto_rnd = random.random()
        if proto_rnd < 0.4:
            proto = 6
        elif 0.9 < proto_rnd >= 0.4:
            proto = 17
        else:
            proto = random.randint(0, 255)
        ipver = 4 if random.random() < 0.7 else 6
        ipver_type = IPv4Address if ipver == 4 else IPv6Address
        s_addr = gen_ip(ipver)
        d_addr = gen_ip(ipver)
        while s_addr == d_addr:
            d_addr = gen_ip(ipver)
        s_port = random.randint(0, 65535)
        d_port = random.randint(0, 65535)
        pkts = random.randint(1, 1000000)
        bts = random.randint(64, 1000000)
        return MockedFlow(ipver_type, t_start, t_end, proto, s_addr, d_addr, s_port, d_port, pkts, bts).flow

    @staticmethod
    def create_one_flow_and_dict(ipver, **kwargs):
        """Create one MockedFlow. Return one flow and dict of IPv4 or IPv6 based on ipver"""
        if len(kwargs) == 9:  # one flow
            mocked_flow = MockedFlow(ipver, **kwargs)
            return mocked_flow.flow, mocked_flow.flow_dict
        assert False, "Wrong number of kwargs parameters!"

    @staticmethod
    def create_two_flows_and_dicts(ipver, **kwargs):
        """Create two MockedFlow. Return two flow and dict of IPv4 or IPv6 based on ipver"""
        if len(kwargs) == 18:  # two flows
            mocked_flow1 = MockedFlow(
                ipver,
                kwargs["t_start"],
                kwargs["t_end"],
                kwargs["proto"],
                kwargs["s_addr"],
                kwargs["d_addr"],
                kwargs["s_port"],
                kwargs["d_port"],
                kwargs["pkts"],
                kwargs["bts"],
            )
            mocked_flow2 = MockedFlow(
                ipver,
                kwargs["f2_t_start"],
                kwargs["f2_t_end"],
                kwargs["f2_proto"],
                kwargs["f2_s_addr"],
                kwargs["f2_d_addr"],
                kwargs["f2_s_port"],
                kwargs["f2_d_port"],
                kwargs["f2_pkts"],
                kwargs["f2_bts"],
            )
            return mocked_flow1.flow, mocked_flow1.flow_dict, mocked_flow2.flow, mocked_flow2.flow_dict
        assert False, "Wrong number of kwargs parameters!"


class MockedFlow:
    """Create instance of Flow and dictionary that has been used for its creation"""

    # pylint: disable=too-many-arguments
    def __init__(
        self,
        ipver: Union[IPv4Address, IPv6Address],
        t_start: int,
        t_end: int,
        proto: int,
        s_addr: Union[List[Union[IPv4Address, IPv6Address]], str],
        d_addr: Union[List[Union[IPv4Address, IPv6Address]], str],
        s_port: int,
        d_port: int,
        pkts: int,
        bts: int,
    ):
        self.ipver = ipver
        self.s_addr_list = s_addr
        self.d_addr_list = d_addr
        self.flow_dict = {
            "t_start": t_start,
            "t_end": t_end + t_start,
            "proto": proto,
            "s_addr": self._get_addr("s_addr"),
            "d_addr": self._get_addr("d_addr"),
            "s_port": s_port,
            "d_port": d_port,
            "pkts": pkts,
            "bts": bts,
        }
        self.flow = Flow(**self.flow_dict)
        # src and dst IP address of the flow must be unique
        assume(self.flow_dict["s_addr"] != self.flow_dict["d_addr"])

    def _get_addr(self, which: str) -> str:
        """Get IPv4 or IPv6 address from s_addr or d_addr"""
        if which == "s_addr":
            if isinstance(self.s_addr_list, str):
                return self.s_addr_list
            return str([addr for addr in self.s_addr_list if isinstance(addr, self.ipver)][0])
        if which == "d_addr":
            if isinstance(self.d_addr_list, str):
                return self.d_addr_list
            return str([addr for addr in self.d_addr_list if isinstance(addr, self.ipver)][0])
        assert False, "Unsupported which param!"

    @staticmethod
    def unique_dicts(flow1_dict: dict, flow2_dict: dict):
        """Constrain Hypothesis examples for specific testcases"""
        # first and second flow must have different IP address in order to be unique => false positives in _hash_eq()
        assume(flow1_dict["s_addr"] != flow2_dict["s_addr"])
        assume(flow1_dict["d_addr"] != flow2_dict["d_addr"])
        # proto and ports must be different => false positives in __eq__()
        assume(
            flow1_dict["proto"] != flow2_dict["proto"]
            and (flow1_dict["s_port"] != flow2_dict["s_port"] or (flow1_dict["d_port"] != flow2_dict["d_port"]))
        )

    @staticmethod
    def create_nonkey(flow_dict: dict) -> Flow:
        """Create new Flow where non-key variables have random values"""
        return MockedFlow(
            IPv4Address if flow_dict["s_addr"] is IPv4Address else IPv6Address,
            random.randint(0, 1000000),
            random.randint(0, 1000000),
            flow_dict["proto"],
            flow_dict["s_addr"],
            flow_dict["d_addr"],
            flow_dict["s_port"],
            flow_dict["d_port"],
            random.randint(1, 1000000),
            random.randint(64, 1000000),
        ).flow

    @staticmethod
    def create_rev(flow_dict: dict) -> Flow:
        """Create new reversed Flow"""
        return MockedFlow(
            IPv4Address if flow_dict["s_addr"] is IPv4Address else IPv6Address,
            flow_dict["t_start"],
            flow_dict["t_end"],
            flow_dict["proto"],
            flow_dict["d_addr"],
            flow_dict["s_addr"],
            flow_dict["d_port"],
            flow_dict["s_port"],
            flow_dict["pkts"],
            flow_dict["bts"],
        ).flow


class TestFlow:
    """Class for pytest flow.py unit tests."""

    # Register and use Hypothesis profile for all testcases
    # Testcases with monkeypatch must have disabled healthcheck monkeypatch fixture - function scope fixture
    # In some cases example generation is slow and Hypothesis fails with that healthcheck error
    settings.register_profile(
        "ci",
        max_examples=15,
        suppress_health_check=[HealthCheck.too_slow, HealthCheck.function_scoped_fixture, HealthCheck.data_too_large],
    )
    settings.load_profile("ci")

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_init_proto(ipver, **kwargs):
        """Test protocol cases during Flow init - if proto == 6 or 17, s_port and d_port must be kept, otherwise it
        must be set to 0. Generated protocols can be from range 0 - 255."""
        flow, flow_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)

        if flow_dict["proto"] in [6, 17]:
            if flow_dict["s_addr"] > flow_dict["d_addr"]:
                assert flow.swap is True
                assert flow.key[2] == flow_dict["d_port"]
                assert flow.key[3] == flow_dict["s_port"]
            else:
                assert flow.swap is False
                assert flow.key[2] == flow_dict["s_port"]
                assert flow.key[3] == flow_dict["d_port"]
        else:
            assert flow.key[2] == 0
            assert flow.key[3] == 0
        assert flow.key[4] == flow_dict["proto"]
        assert flow._rev_packets == 0
        assert flow._rev_bytes == 0

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_init_ipver(ipver, **kwargs):
        """Test IPv4/IPv6 protocol is correctly identified from IP address."""
        flow, _ = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        ip_check = 4 if ipver is IPv4Address else 6
        assert flow._l3 == ip_check

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_two_flows())
    def test_flow_comparison(ipver, **kwargs):
        """Test __hash__() and __eq__() methods for random, reverse and nonkey flows"""

        def assert_hash_eq(lhs: Flow, rhs: Flow, oper: str):
            if oper == "==":
                assert hash(lhs) == hash(rhs)
                assert lhs == rhs
                assert rhs == lhs
            elif oper == "!=":
                assert hash(lhs) != hash(rhs)
                assert lhs != rhs
                assert rhs != lhs
            else:
                assert False, "Unsupported op param!"

        flow1, flow1_dict, flow2, flow2_dict = GenerateFlows.create_two_flows_and_dicts(ipver, **kwargs)
        MockedFlow.unique_dicts(flow1_dict, flow2_dict)
        flow1_nonkey, flow1_rev = MockedFlow.create_nonkey(flow1_dict), MockedFlow.create_rev(flow1_dict)
        flow2_nonkey, flow2_rev = MockedFlow.create_nonkey(flow2_dict), MockedFlow.create_rev(flow2_dict)

        assert_hash_eq(flow1, flow1_nonkey, "==")
        assert_hash_eq(flow2, flow2_nonkey, "==")
        assert_hash_eq(flow1, flow1_rev, "==")
        assert_hash_eq(flow2, flow2_rev, "==")

        assert_hash_eq(flow1, flow2, "!=")
        assert_hash_eq(flow1, flow2_nonkey, "!=")
        assert_hash_eq(flow1, flow2_rev, "!=")

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_two_flows())
    def test_update_noneq_flows(ipver, **kwargs):
        """Test update() method - using non-equal flows must return false (no update)"""
        flow1, flow1_dict, flow2, flow2_dict = GenerateFlows.create_two_flows_and_dicts(ipver, **kwargs)
        MockedFlow.unique_dicts(flow1_dict, flow2_dict)
        flow2_nonkey, flow2_rev = MockedFlow.create_nonkey(flow2_dict), MockedFlow.create_rev(flow2_dict)

        with patch("ftprofiler.flow.Flow._merge") as mocked_merge:
            assert not flow1.update(flow2, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_not_called()
            assert not flow1.update(flow2_nonkey, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_not_called()
            assert not flow1.update(flow2_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_not_called()

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_update_first_rev_flow_processed(ipver, **kwargs):
        """Test update() on first flow in opposite direction. Test that function _is_reverse() is called."""
        flow1, flow1_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        flow1_rev = MockedFlow.create_rev(flow1_dict)
        assert flow1.update(flow1_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_update_first_rev_flow(monkeypatch, ipver, **kwargs):
        """Test update() on first flow in opposite direction.
        Mock _is_reverse to True to ensure flows are merged because it is first flow in opposite direction."""
        monkeypatch.setattr(Flow, "_is_reverse", lambda *_: True)
        flow1, flow1_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        flow1_rev = MockedFlow.create_rev(flow1_dict)

        with patch("ftprofiler.flow.Flow._merge") as mocked_merge:
            assert flow1.update(flow1_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_called_once()

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_update_exported_flow(monkeypatch, ipver, **kwargs):
        """Test update() on flow that was exported due to hash collision (1-packet flow with special start time)"""
        monkeypatch.setattr(Flow, "_is_reverse", lambda *_: False)
        flow1, flow1_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        flow1_rev = MockedFlow.create_rev(flow1_dict)
        flow1_rev.packets = 1
        flow1_rev.start_time = flow1.end_time - GenerateFlows.DEFAULT_INACTIVE / 2

        with patch("ftprofiler.flow.Flow._merge") as mocked_merge:
            assert flow1.update(flow1_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_called_once()

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_update_short_flow_duration(monkeypatch, ipver, **kwargs):
        """Test update() on original flow with short duration (flow was not split by active timeout)."""
        monkeypatch.setattr(Flow, "_is_reverse", lambda *_: False)
        flow1, flow1_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        flow1_rev = MockedFlow.create_rev(flow1_dict)
        flow1_rev.packets += 2
        flow1.start_time = 0
        flow1.end_time = GenerateFlows.DEFAULT_ACTIVE - 2 * GenerateFlows.DEFAULT_INACTIVE

        with patch("ftprofiler.flow.Flow._merge") as mocked_merge:
            assert not flow1.update(flow1_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_not_called()

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_update_flow_after_inactive_timeout(monkeypatch, ipver, **kwargs):
        """Test update() on flow that started "inactive timeout" seconds after end of original flow.
        It is considered new flow and thus flows should not be merged."""
        monkeypatch.setattr(Flow, "_is_reverse", lambda *_: False)
        flow1, flow1_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        flow1_rev = MockedFlow.create_rev(flow1_dict)
        flow1_rev.packets += 2
        flow1.start_time = 0
        flow1.end_time = 3 * GenerateFlows.DEFAULT_ACTIVE
        flow1_rev.start_time = flow1.end_time + 2 * GenerateFlows.DEFAULT_INACTIVE

        with patch("ftprofiler.flow.Flow._merge") as mocked_merge:
            assert not flow1.update(flow1_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_not_called()

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_one_flow())
    def test_update_flow_extension(monkeypatch, ipver, **kwargs):
        """Test update() on flow that is an extension of current flow."""
        monkeypatch.setattr(Flow, "_is_reverse", lambda *_: False)
        flow1, flow1_dict = GenerateFlows.create_one_flow_and_dict(ipver, **kwargs)
        flow1_rev = MockedFlow.create_rev(flow1_dict)
        flow1_rev.packets += 2
        flow1.start_time = 0
        flow1.end_time = 3 * GenerateFlows.DEFAULT_ACTIVE
        flow1_rev.start_time = flow1.end_time - GenerateFlows.DEFAULT_INACTIVE

        with patch("ftprofiler.flow.Flow._merge") as mocked_merge:
            assert flow1.update(flow1_rev, GenerateFlows.DEFAULT_INACTIVE, GenerateFlows.DEFAULT_ACTIVE)
            mocked_merge.assert_called_once()

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @pytest.mark.parametrize("swap", [True, False])
    @given(**GenerateFlows.strategy_two_flows())
    def test_is_reverse_same_swap(ipver, swap, **kwargs):
        """Test _is_reverse() with both flows having same swap flag. Such flows cannot be reverse by definition."""
        flow1, _, flow2, _ = GenerateFlows.create_two_flows_and_dicts(ipver, **kwargs)
        flow1.swap = swap
        flow2.swap = swap

        assert not flow1._is_reverse(flow2, GenerateFlows.DEFAULT_INACTIVE)
        assert not flow2._is_reverse(flow1, GenerateFlows.DEFAULT_INACTIVE)

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_two_flows())
    def test_is_reverse_high_delay(ipver, **kwargs):
        """Test _is_reverse() with flow starting "inactive timeout" seconds after end time of original flow.
        It is considered new flow and thus it cannot be reverse of original flow."""
        flow1, _, flow2, _ = GenerateFlows.create_two_flows_and_dicts(ipver, **kwargs)
        flow1.swap = True
        flow2.swap = False
        flow1.start_time = 0
        flow1.end_time = 3 * GenerateFlows.DEFAULT_INACTIVE
        flow2.start_time = flow1.end_time + 2 * GenerateFlows.DEFAULT_INACTIVE

        assert not flow1._is_reverse(flow2, GenerateFlows.DEFAULT_INACTIVE)
        assert not flow2._is_reverse(flow1, GenerateFlows.DEFAULT_INACTIVE)

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @given(**GenerateFlows.strategy_two_flows())
    def test_is_reverse_success(ipver, **kwargs):
        """Test _is_reverse() with flows that are reverse of each other."""
        flow1, _, flow2, _ = GenerateFlows.create_two_flows_and_dicts(ipver, **kwargs)
        flow1.swap = False
        flow2.swap = True
        flow1.start_time = 0
        flow1.end_time = 3 * GenerateFlows.DEFAULT_INACTIVE
        flow2.start_time = flow1.end_time - GenerateFlows.DEFAULT_INACTIVE

        assert flow1._is_reverse(flow2, GenerateFlows.DEFAULT_INACTIVE)
        assert flow2._is_reverse(flow1, GenerateFlows.DEFAULT_INACTIVE)

    @staticmethod
    @pytest.mark.parametrize("ipver", [IPv4Address, IPv6Address])
    @pytest.mark.parametrize(("f1_swap", "f2_swap"), [[True, True], [True, False], [False, True], [False, False]])
    @given(**GenerateFlows.strategy_two_flows())
    def test_merge(ipver, f1_swap, f2_swap, **kwargs):
        """Test merge()"""

        def assert_flows(flow1: Flow, flow2: Flow, f1_pkts: int, f1_bytes: int, f1_revpkts: int, f1_revbytes: int):
            assert flow1.start_time == min(flow1.start_time, flow2.start_time)
            assert flow1.end_time == max(flow1.end_time, flow2.end_time)
            if f1_swap == f2_swap:
                assert flow1.packets == f1_pkts + flow2.packets
                assert flow1.bytes == f1_bytes + flow2.bytes
                assert flow1._rev_packets == 0
                assert flow1._rev_bytes == 0
            else:
                assert flow1._rev_packets == f1_revpkts + flow2.packets
                assert flow1._rev_bytes == f1_revbytes + flow2.bytes
                assert flow1.packets == 0
                assert flow1.bytes == 0

        flow1, _, flow2, _ = GenerateFlows.create_two_flows_and_dicts(ipver, **kwargs)
        flow1.swap = f1_swap
        flow2.swap = f2_swap

        f2_copy = copy.deepcopy(flow2)
        if f1_swap != f2_swap:
            flow1.packets = 0
            flow1.bytes = 0
        else:
            flow1._rev_packets = 0
            flow1._rev_bytes = 0
        flow1_packets = flow1.packets
        flow1_bytes = flow1.bytes
        flow1_revpackets = flow1._rev_packets
        flow1_revbytes = flow1._rev_bytes

        flow1._merge(flow2)
        assert_flows(flow1, flow2, flow1_packets, flow1_bytes, flow1_revpackets, flow1_revbytes)

        # try to merge it again
        flow1_packets = flow1.packets
        flow1_bytes = flow1.bytes
        flow1_revpackets = flow1._rev_packets
        flow1_revbytes = flow1._rev_bytes

        flow1._merge(flow2)
        assert_flows(flow1, flow2, flow1_packets, flow1_bytes, flow1_revpackets, flow1_revbytes)

        # check that flow2 has not changed
        assert flow2.start_time == f2_copy.start_time
        assert flow2.end_time == f2_copy.end_time
        assert flow2.packets == f2_copy.packets
        assert flow2.bytes == f2_copy.bytes
        assert flow2._rev_packets == f2_copy._rev_packets
        assert flow2._rev_bytes == f2_copy._rev_bytes
        assert flow2 == f2_copy
