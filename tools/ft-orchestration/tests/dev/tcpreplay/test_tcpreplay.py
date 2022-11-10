"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Simple development tests of TcpReplay class.
"""

import os

import invoke
import pytest
from src.host.host import Host
from src.host.storage import RemoteStorage
from src.tcpreplay.tcpreplay import TcpReplay

HOST = os.environ.get("PYTEST_TEST_HOST")
STORAGE = RemoteStorage(HOST) if HOST else None
PCAP_FILE = os.path.join(os.path.dirname(os.path.realpath(__file__)), "NTP_sync.pcap")

# pylint: disable=unused-argument
@pytest.mark.dev
def test_tcpreplay_local(require_root):
    """Test tcpreplay on local machine."""

    tcpreplay = TcpReplay(Host())
    res = tcpreplay.start(f"-i lo -t -K {PCAP_FILE}")
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote(require_root):
    """Test tcpreplay on remote machine."""

    tcpreplay = TcpReplay(Host(HOST, STORAGE))
    res = tcpreplay.start(f"-i eth0 -t -K {PCAP_FILE}")
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_asynchronous(require_root):
    """Test tcpreplay on remote machine with asynchronous mode."""

    tcpreplay = TcpReplay(Host(HOST, STORAGE))
    res = tcpreplay.start(f"-i eth0 -t -K {PCAP_FILE}", asynchronous=True)
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_asynchronous_stop(require_root):
    """Test manual stop of tcpreplay started on remote machine in asynchronous mode."""

    tcpreplay = TcpReplay(Host(HOST, STORAGE))
    res = tcpreplay.start(f"-i eth0 -t -K {PCAP_FILE}", asynchronous=True, check_rc=False)
    tcpreplay.stop(res)
    res = res.join()

    # Command is terminated via CTRL+C
    assert res.stdout == "^C"
    assert res.stderr == ""


@pytest.mark.dev
def test_tcpreplay_local_timeout(require_root):
    """Test termination of local tcpreplay via timeout."""

    tcpreplay = TcpReplay(Host())
    with pytest.raises(invoke.exceptions.CommandTimedOut):
        tcpreplay.start(f"-i lo -t --loop=9199999 -K {PCAP_FILE}", timeout=2.5)


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_timeout(require_root):
    """Test termination of remote tcpreplay via timeout."""

    tcpreplay = TcpReplay(Host(HOST, STORAGE))
    with pytest.raises(invoke.exceptions.CommandTimedOut):
        tcpreplay.start(f"-i eth0 -t --loop=9199999 -K {PCAP_FILE}", timeout=2.5)


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_remote_timeout_asynchronous(require_root):
    """Test termination of asynchronous remote tcpreplay via timeout."""

    tcpreplay = TcpReplay(Host(HOST, STORAGE))
    with pytest.raises(invoke.exceptions.CommandTimedOut):
        res = tcpreplay.start(f"-i eth0 -t --loop=9199999 -K {PCAP_FILE}", timeout=2.5, asynchronous=True)
        res.join()


@pytest.mark.dev
def test_tcpreplay_edit_local(require_root):
    """Test tcpreplay-edit on local machine."""

    tcpreplay = TcpReplay(Host(), tcp_edit=True)
    res = tcpreplay.start(
        f"-i lo -t -K --enet-vlan=add --enet-vlan-tag=1 --enet-vlan-cfi=0 --enet-vlan-pri=0 {PCAP_FILE}"
    )
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315 + stats[0] * 4  # include 4B VLAN header for every packet


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="remote host not defined")
def test_tcpreplay_edit_remote(require_root):
    """Test tcpreplay-edit on remote machine."""

    tcpreplay = TcpReplay(Host(HOST, STORAGE), tcp_edit=True)
    res = tcpreplay.start(
        f"-i eth0 -t -K --enet-vlan=add --enet-vlan-tag=1 --enet-vlan-cfi=0 --enet-vlan-pri=0 {PCAP_FILE}"
    )
    stats = tcpreplay.stats(res)
    assert stats[0] == 32
    assert stats[1] == 3315 + stats[0] * 4  # include 4B VLAN header for every packet
