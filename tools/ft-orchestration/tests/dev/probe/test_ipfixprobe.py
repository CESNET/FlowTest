"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for ipfixprobe connector.
"""

import logging
import os
import time
from collections import namedtuple
import re

import docker
import invoke
import pytest
from src.config.common import InterfaceCfg
from src.host.host import Host
from src.host.storage import RemoteStorage
from src.probe.interface import ProbeException
from src.probe.ipfixprobe import IpfixprobeDpdk, IpfixprobeNdp, IpfixprobeRaw
from src.probe.probe_target import ProbeTarget

DockerContainer = namedtuple("DockerContainer", ["ip", "host", "context"])


# pylint: disable=too-few-public-methods
# pylint: disable=abstract-method
class FakeRunner(invoke.Runner):
    """Dummy invoke runner object for IpfixprobeHostStub."""

    def __init__(self) -> None:
        super().__init__(None)

    @property
    def process_is_finished(self):
        """Used when cmd running async. Always finished."""

        return True


class FakePromise(invoke.Promise):
    """Dummy invoke promise object for IpfixprobeHostStub."""

    # pylint: disable=super-init-not-called
    def __init__(self, result: invoke.runners.Result) -> None:
        self._result = result

    def join(self):
        """Used when cmd running async. Return invoke result."""

        return self._result

    @property
    def runner(self):
        """Used when cmd running async. Fake runner object."""

        return FakeRunner()


class IpfixprobeHostStub(Host):
    """Object acts like Host. Used to checking last ran command."""

    def __init__(self, host="localhost"):  # pylint: disable=super-init-not-called
        """Init fake host."""

        self._connection = None
        self._storage = None
        self._host = host

        if host == "localhost":
            self._local = True
        else:
            self._local = False

        self.last_command = ""

    # pylint: disable=unused-argument
    def run(
        self,
        command,
        asynchronous=False,
        check_rc=True,
        timeout=None,
        path_replacement=True,
    ):
        """Run a command."""

        self.last_command = command
        if asynchronous:
            return FakePromise(invoke.runners.Result())
        return invoke.runners.Result()


@pytest.fixture(scope="class", name="fake_host")
def fixture_fake_host():
    """Create fake host object."""

    return IpfixprobeHostStub()


def run_docker_container(image: str, bind_ssh=True) -> DockerContainer:
    """Run docker container by image name.

    Parameters
    ----------
    image : str
        Image name.
    bind_ssh : bool, optional
        Bind container on port 22:22, by default True.

    Returns
    -------
    DockerContainer
        Structure holding infos about new container.
    """

    with open(os.path.expanduser("~/.ssh/id_rsa.pub"), "r", encoding="ascii") as file:
        pub_key = file.read().strip()

    client = docker.from_env()
    container = client.containers.run(
        image,
        detach=True,
        tty=True,
        remove=True,
        ports={"22/tcp": 22} if bind_ssh else {},
        environment=[f"SSH_PUB_KEY={pub_key}"],
        privileged=True,
    )
    time.sleep(5)
    ip_ = container.exec_run("hostname -I").output.decode(encoding="ascii").strip()
    host_ = Host("127.0.0.1", RemoteStorage("127.0.0.1", user="test"), user="test") if bind_ssh else None

    return DockerContainer(ip_, host_, container)


@pytest.fixture(scope="class", name="docker_ipfixprobe")
def fixture_docker_ipfixprobe(request):
    """Run docker container with installed ipfixprobe. Connect with SSH."""

    container = run_docker_container("thesablecz/ipfixprobe")

    def stop_container():
        container.context.stop()

    request.addfinalizer(stop_container)

    return container


@pytest.fixture(scope="class", name="docker_ssh")
def fixture_docker_ssh(request):
    """Run docker container. Connect with SSH."""

    container = run_docker_container("thesablecz/debian-ssh")

    def stop_container():
        container.context.stop()

    request.addfinalizer(stop_container)

    return container


@pytest.fixture(scope="class", name="docker_pinger")
def fixture_docker_pinger(request):
    """Run docker container to ping another containers."""

    container = run_docker_container("debian:10", bind_ssh=False)

    def stop_container():
        container.context.stop()

    request.addfinalizer(stop_container)

    return container


class TestWithFakeHost:
    """Group of tests which uses fake remote host."""

    @staticmethod
    def test_prepare_cmd_minimal(fake_host):
        """Test command format with minimal config."""

        probe = IpfixprobeRaw(fake_host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("eno1", 10)])

        probe.start()
        probe.stop()

        regex = 'ipfixprobe -i "raw;ifc=eno1" -o "ipfix;h=127.0.0.1;p=4739" |& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log'
        assert re.search(regex, fake_host.last_command)

    @staticmethod
    def test_assert_without_interfaces(fake_host):
        """Test assertion if no interface specified."""

        with pytest.raises(AssertionError):
            IpfixprobeRaw(fake_host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [])

        with pytest.raises(AssertionError):
            IpfixprobeDpdk(fake_host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [], False, core_mask=1)

    @staticmethod
    def test_prepare_cmd_plugins(fake_host):
        """Test command format with all protocols enabled."""

        probe = IpfixprobeRaw(
            fake_host,
            ProbeTarget("127.0.0.1", 4739, "tcp"),
            ["eth", "tcp", "ipv4", "ipv6", "dns", "http", "tls"],
            [InterfaceCfg("eno1", 10)],
        )

        probe.start()
        probe.stop()

        regex = (
            'ipfixprobe -i "raw;ifc=eno1" -o "ipfix;h=127.0.0.1;p=4739"'
            ' -p "basicplus" -p "dns" -p "http" -p "tls" |& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log'
        )
        assert re.search(regex, fake_host.last_command)

    @staticmethod
    def test_prepare_cmd_raw_plugin(fake_host):
        """Test command format with raw input plugin parameters."""

        kwargs = {"fanout": True, "fanout_id": 242, "blocks": 2048, "packets": 64}

        probe = IpfixprobeRaw(
            fake_host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("eno1", 10)], False, **kwargs
        )

        probe.start()
        probe.stop()

        regex = (
            'ipfixprobe -i "raw;ifc=eno1;f=242;b=2048;p=64" -o "ipfix;h=127.0.0.1;p=4739"'
            " |& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log"
        )
        assert re.search(regex, fake_host.last_command)

        probe = IpfixprobeRaw(
            fake_host,
            ProbeTarget("127.0.0.1", 4739, "tcp"),
            [],
            [InterfaceCfg("eno1", 10), InterfaceCfg("eno2", 10), InterfaceCfg("eno3", 10)],
            False,
            **kwargs,
        )

        probe.start()
        probe.stop()

        regex = (
            'ipfixprobe -i "raw;ifc=eno1;f=242;b=2048;p=64" -i "raw;ifc=eno2;f=242;b=2048;p=64"'
            ' -i "raw;ifc=eno3;f=242;b=2048;p=64" -o "ipfix;h=127.0.0.1;p=4739"'
            " |& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log"
        )
        assert re.search(regex, fake_host.last_command)

    @staticmethod
    def test_prepare_cmd_dpdk_plugin(fake_host):
        """Test command format with dpdk input plugin parameters."""

        kwargs = {
            "core_mask": 0x01,
            "memory": 2048,
            "file_prefix": "ipfixprobe",
            "queues_count": 4,
            "mbuf_size": 128,
            "mempool_size": 4096,
        }

        probe = IpfixprobeDpdk(
            fake_host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("0000:01:00.0", 10)], False, **kwargs
        )

        probe.start()
        probe.stop()

        regex = (
            'ipfixprobe -i "dpdk;p=0;q=4;b=128;m=4096;e=-c 1 -m 2048 --file-prefix ipfixprobe -a 0000:01:00.0"'
            ' -i "dpdk" -i "dpdk" -i "dpdk" -o "ipfix;h=127.0.0.1;p=4739" |& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log'
        )
        assert re.search(regex, fake_host.last_command)

        # Only 1 input interface is supported by ipfixprobe-dpdk at the time.
        with pytest.raises(ProbeException):
            probe = IpfixprobeDpdk(
                fake_host,
                ProbeTarget("127.0.0.1", 4739, "tcp"),
                [],
                [InterfaceCfg("0000:01:00.0", 10), InterfaceCfg("0000:01:00.1", 10), InterfaceCfg("0000:01:00.2", 10)],
                False,
                **kwargs,
            )

    @staticmethod
    def test_prepare_cmd_ndp_plugin(fake_host):
        """Test command format with ndp input plugin parameters."""

        probe = IpfixprobeNdp(fake_host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("/dev/nfb0:0", 10)])

        probe.start()
        probe.stop()

        regex = (
            'ipfixprobe -i "ndp;dev=/dev/nfb0:0" -o "ipfix;h=127.0.0.1;p=4739" '
            "|& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log"
        )
        assert re.search(regex, fake_host.last_command)

        probe = IpfixprobeNdp(
            fake_host,
            ProbeTarget("127.0.0.1", 4739, "tcp"),
            [],
            [InterfaceCfg("/dev/nfb0:0", 10), InterfaceCfg("/dev/nfb0:1", 10), InterfaceCfg("/dev/nfb0:2", 10)],
        )

        probe.start()
        probe.stop()

        regex = (
            'ipfixprobe -i "ndp;dev=/dev/nfb0:0" -i "ndp;dev=/dev/nfb0:1" -i "ndp;dev=/dev/nfb0:2"'
            ' -o "ipfix;h=127.0.0.1;p=4739" |& tee -i /tmp/[0-0a-zA-Z]+/ipfixprobe.log'
        )
        assert re.search(regex, fake_host.last_command)


def get_ipfixprobe_pid(host):
    """Get pid of running ipfixprobe instance."""

    ps_aux = host.run("ps aux | grep -i '[i]pfixprobe'").stdout
    return int(ps_aux.split()[1])


@pytest.mark.dev_docker
class TestWithDockerIpfixprobe:
    """Group of tests which use ipfixprobe docker container."""

    @staticmethod
    def test_run_in_docker(docker_ipfixprobe):
        """Run ipfixprobe with connector in docker."""

        probe = IpfixprobeRaw(
            docker_ipfixprobe.host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("eth0", 10)], False, True
        )

        probe.start()
        time.sleep(30)
        probe.stop()

    @staticmethod
    def test_run_in_docker_statistics(docker_ipfixprobe, docker_pinger):
        """Run ipfixprobe with connector in docker."""

        probe = IpfixprobeRaw(
            docker_ipfixprobe.host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("eth0", 10)], False, True
        )

        probe.start()
        ping_res = docker_pinger.context.exec_run(f"ping {docker_ipfixprobe.ip} -c 30")
        logging.getLogger().info("Ping res: %s", ping_res.output.decode(encoding="ascii").strip())
        time.sleep(2)
        probe.stop()

        stats = probe.last_run_stats
        assert stats.input[0]["packets"] >= 30
        assert stats.output[0]["packets"] >= 30
        assert stats.output[0]["biflows"] >= 1

    @staticmethod
    def test_check_plugin(docker_ipfixprobe):
        """Test raise of ProbeException when input plugin not compiled in ipfixprobe binary."""

        with pytest.raises(ProbeException, match="Plugin 'dpdk' not found by ipfixprobe binary."):
            IpfixprobeDpdk(
                docker_ipfixprobe.host,
                ProbeTarget("127.0.0.1", 4739, "tcp"),
                [],
                [InterfaceCfg("0000:01:00.0", 10)],
                False,
                True,
                core_mask=0x01,
            )

    @staticmethod
    def test_kill_ipfixprobe(docker_ipfixprobe):
        """Test killing ipfixprobe instance before start."""

        docker_ipfixprobe.host.run('sudo ipfixprobe -i "raw;ifc=eth0" -o "ipfix;h=127.0.0.1;p=4739"', asynchronous=True)
        time.sleep(2)
        old_pid = get_ipfixprobe_pid(docker_ipfixprobe.host)

        probe = IpfixprobeRaw(
            docker_ipfixprobe.host, ProbeTarget("127.0.0.1", 4739, "tcp"), [], [InterfaceCfg("eth0", 10)], False, True
        )

        probe.start()
        new_pid = get_ipfixprobe_pid(docker_ipfixprobe.host)
        time.sleep(30)
        probe.stop()

        assert old_pid != new_pid


@pytest.mark.dev_docker
class TestWithDockerSsh:
    """Group of tests which use ssh docker container."""

    @staticmethod
    def test_check_binary(docker_ssh):
        """Test raise of ProbeException when ipfixprobe not installed on remote host."""

        with pytest.raises(ProbeException, match="Unable to locate or execute ipfixprobe binary."):
            IpfixprobeRaw(
                docker_ssh.host,
                ProbeTarget("127.0.0.1", 4739, "tcp"),
                [],
                [InterfaceCfg("eth0", 10), False],
                False,
                True,
            )
