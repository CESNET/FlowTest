"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Simple development tests of TcpReplay class.
"""

import os

import invoke
from src.tcpreplay.tcpreplay import TcpReplay

import pytest

HOST = os.environ.get("PYTEST_TEST_HOST")


# pylint: disable=unused-argument
@pytest.mark.dev
def test_tcpreplay_local(require_root):
    """Test tcpreplay on local machine."""

    tcpreplay = TcpReplay()
    res = tcpreplay.start("-i lo -t -K tests/dev/tcpreplay/NTP_sync.pcap")
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote(require_root):
    """Test tcpreplay on remote machine."""

    tcpreplay = TcpReplay(HOST)
    res = tcpreplay.start("-i eth0 -t -K tests/dev/tcpreplay/NTP_sync.pcap")
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_asynchronous(require_root):
    """Test tcpreplay on remote machine with asynchronous mode."""

    tcpreplay = TcpReplay(HOST)
    res = tcpreplay.start("-i eth0 -t -K tests/dev/tcpreplay/NTP_sync.pcap", asynchronous=True)
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_asynchronous_stop(require_root):
    """Test manual stop of tcpreplay started on remote machine in asynchronous mode."""

    tcpreplay = TcpReplay(HOST)
    res = tcpreplay.start("-i eth0 -t -K tests/dev/tcpreplay/NTP_sync.pcap", asynchronous=True, check_rc=False)
    tcpreplay.stop(res)
    res = res.join()

    # Since command killed immediately, not output is expected
    assert res.stdout == ""
    assert res.stderr == ""


@pytest.mark.dev
def test_tcpreplay_local_timeout(require_root):
    """Test termination of local tcpreplay via timeout."""

    tcpreplay = TcpReplay()
    with pytest.raises(invoke.exceptions.CommandTimedOut):
        tcpreplay.start("-i lo -t --loop=9199999 -K tests/dev/tcpreplay/NTP_sync.pcap", timeout=2.5)


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_timeout(require_root):
    """Test termination of remote tcpreplay via timeout."""

    tcpreplay = TcpReplay(HOST)
    with pytest.raises(invoke.exceptions.CommandTimedOut):
        tcpreplay.start("-i eth0 -t --loop=9199999 -K tests/dev/tcpreplay/NTP_sync.pcap", timeout=2.5)


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_timeout_asynchronous(require_root):
    """Test termination of asynchronous remote tcpreplay via timeout."""

    tcpreplay = TcpReplay(HOST)
    with pytest.raises(invoke.exceptions.CommandTimedOut):
        res = tcpreplay.start(
            "-i eth0 -t --loop=9199999 -K tests/dev/tcpreplay/NTP_sync.pcap", timeout=2.5, asynchronous=True
        )
        res.join()
