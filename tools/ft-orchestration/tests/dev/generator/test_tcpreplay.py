"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Simple development tests of TcpReplay class.
"""

import os
import time

import pytest
from lbr_testsuite.executable import RemoteExecutor
from lbr_testsuite.executable.remote_executor import ssh_agent_enabled
from src.generator.interface import GeneratorException
from src.generator.tcpreplay import TcpReplay

HOST = os.environ.get("PYTEST_TEST_HOST")
HOST_INTERFACE = (
    os.environ.get("PYTEST_TEST_HOST_INTERFACE") if os.environ.get("PYTEST_TEST_HOST_INTERFACE") else "eth0"
)
USERNAME = os.environ.get("PYTEST_TEST_HOST_USERNAME")
PASSWORD = os.environ.get("PYTEST_TEST_HOST_PASSWORD")
KEY = os.environ.get("PYTEST_TEST_HOST_KEY")
PCAP_FILE = os.path.join(os.path.dirname(os.path.realpath(__file__)), "NTP_sync.pcap")


def parametrize_remote_connection():
    """Parametrize remote connections.

    Provide Host and RemoteStorage objects.
    """

    combinations = []

    if HOST is None:
        combinations.append(pytest.param(None, marks=pytest.mark.skip(reason="host not defined")))
    else:
        if USERNAME is not None:
            user_args = {"user": USERNAME}
        else:
            user_args = {}

        if KEY is None:
            combinations.append(pytest.param(None, marks=pytest.mark.skip(reason="ssh key not defined")))
        else:
            executor = RemoteExecutor(HOST, key_filename=KEY, **user_args)
            combinations.append({"executor": executor})

        if PASSWORD is None:
            combinations.append(pytest.param(None, marks=pytest.mark.skip(reason="password not defined")))
        else:
            executor = RemoteExecutor(HOST, password=PASSWORD, **user_args)
            combinations.append({"executor": executor})

        if not ssh_agent_enabled():
            combinations.append(pytest.param(None, marks=pytest.mark.skip(reason="ssh agent not defined")))
        else:
            executor = RemoteExecutor(HOST, **user_args)
            combinations.append({"executor": executor})

    return combinations


def connection_ids():
    """Provide identification for parametrize_remote_connection() function.

    This provides better readability for parametrized tests.
    """

    ids = []

    if HOST is None:
        ids.append("")
    else:
        ids.append("ssh_key")
        ids.append("password")
        ids.append("ssh_agent")

    return ids


# pylint: disable=unused-argument
@pytest.mark.dev
def test_tcpreplay_local(require_root):
    """Test tcpreplay on local machine."""

    tcpreplay = TcpReplay()
    tcpreplay.add_interface("lo")
    tcpreplay.start(PCAP_FILE)
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_tcpreplay_remote(connection_parameters, require_root):
    """Test tcpreplay on remote machine."""

    tcpreplay = TcpReplay(connection_parameters["executor"])
    tcpreplay.add_interface(HOST_INTERFACE)
    tcpreplay.start(PCAP_FILE)
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_tcpreplay_remote_stop(connection_parameters, require_root):
    """Test tcpreplay on remote machine with stop."""

    tcpreplay = TcpReplay(connection_parameters["executor"])
    tcpreplay.add_interface(HOST_INTERFACE)
    tcpreplay.start(PCAP_FILE)
    time.sleep(2)  # wait for process to finish
    tcpreplay.stop()
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315


@pytest.mark.dev
def test_tcpreplay_local_timeout(require_root):
    """Test termination of local tcpreplay via timeout."""

    tcpreplay = TcpReplay()
    tcpreplay.add_interface("lo")
    tcpreplay.start(PCAP_FILE, loop_count=9199999)

    with pytest.raises(GeneratorException):
        tcpreplay.stop(timeout=2.5)


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_tcpreplay_remote_timeout(connection_parameters, require_root):
    """Test termination of remote tcpreplay via timeout."""

    tcpreplay = TcpReplay(connection_parameters["executor"])
    tcpreplay.add_interface(HOST_INTERFACE)
    tcpreplay.start(PCAP_FILE, loop_count=9199999)

    with pytest.raises(GeneratorException):
        tcpreplay.stop(timeout=2.5)


@pytest.mark.dev
def test_tcpreplay_edit_local(require_root):
    """Test tcpreplay-edit on local machine."""

    tcpreplay = TcpReplay(add_vlan=1)
    tcpreplay.add_interface("lo")
    tcpreplay.start(PCAP_FILE)
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315 + stats.packets * 4  # include 4B VLAN header for every packet


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_tcpreplay_edit_remote(connection_parameters, require_root):
    """Test tcpreplay-edit on remote machine."""

    tcpreplay = TcpReplay(connection_parameters["executor"], add_vlan=1)
    tcpreplay.add_interface(HOST_INTERFACE)
    tcpreplay.start(PCAP_FILE)
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315 + stats.packets * 4  # include 4B VLAN header for every packet


@pytest.mark.dev
def test_tcpreplay_edit_local_dst_mac(require_root):
    """Test tcpreplay-edit on local machine."""

    tcpreplay = TcpReplay()
    tcpreplay.add_interface("lo", "11:11:11:11:11:11")
    tcpreplay.start(PCAP_FILE)
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_tcpreplay_edit_remote_dst_mac(connection_parameters, require_root):
    """Test tcpreplay-edit on remote machine."""

    tcpreplay = TcpReplay(connection_parameters["executor"])
    tcpreplay.add_interface(HOST_INTERFACE, "11:11:11:11:11:11")
    tcpreplay.start(PCAP_FILE)
    stats = tcpreplay.stats()
    assert stats.packets == 32
    assert stats.bytes == 3315
