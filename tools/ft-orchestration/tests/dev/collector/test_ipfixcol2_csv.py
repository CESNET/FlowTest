"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests of ipfixcol2 connector with save_csv feature provided by fdsdump.
"""

import os
import subprocess
import tempfile
import time

import pytest
import yaml
from lbr_testsuite.executable import Executor, RemoteExecutor, Rsync, Tool
from scapy.layers.inet import IP, TCP, Ether
from scapy.utils import wrpcap
from src.collector.ipfixcol2 import Ipfixcol2
from src.common.tool_is_installed import assert_tool_is_installed

HOST = os.environ.get("PYTEST_TEST_HOST")
USERNAME = os.environ.get("PYTEST_TEST_HOST_USERNAME")
LOCAL_CSV = os.path.join(os.path.dirname(os.path.realpath(__file__)), "saved_flows.csv")
FILES_DIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "files")


def create_executor():
    """Create remote executor."""

    if not HOST:
        pytest.skip("PYTEST_TEST_HOST env variable must be defined.")

    if USERNAME:
        executor = RemoteExecutor(HOST, USERNAME)
    else:
        executor = RemoteExecutor(HOST)
    return executor


@pytest.fixture(name="executor")
def fixture_executor():
    """Fixture for connecting to remote."""

    return create_executor()


def check_csv(csv, number_of_flows):
    """Assert test."""

    # count lines in large file,
    # https://stackoverflow.com/questions/9629179/python-counting-lines-in-a-huge-10gb-file-as-fast-as-possible

    res = subprocess.run(f"wc -l {csv}", check=True, shell=True, capture_output=True)
    lines = int(res.stdout.split()[0])
    assert (lines - 1) == number_of_flows


def generate_pcap(executor: Executor, number_of_flows: int, pcap_file: str):
    """Generate big pcap with big number of flows on a remote host."""

    assert_tool_is_installed("ft-replay", executor)

    rsync = Rsync(executor)
    remote_dir = rsync.get_data_directory()

    # create pcap with 1 packet
    payload = "xxxxxxxxxxxxxxxxxxx"
    packets = [Ether() / IP(src="11.11.11.11", dst="10.0.0.1") / TCP() / payload]

    with tempfile.TemporaryDirectory() as temp_dir:
        tmp = os.path.join(temp_dir, "tmp.pcap")
        wrpcap(tmp, packets)
        rsync.push_path(tmp)
        tmp_pcap = os.path.join(remote_dir, "tmp.pcap")

        # replicate with ft-replay
        config = {"units": [{"srcip": "None"}], "loop": {"dstip": "addOffset(1)"}}

        tmp = os.path.join(temp_dir, "cfg.yaml")
        with open(tmp, "w", encoding="ascii") as tmp_fd:
            yaml.safe_dump(config, tmp_fd)
        rsync.push_path(tmp)
        config_file = os.path.join(remote_dir, "cfg.yaml")

    stdout, stderr = Tool(
        f"ft-replay -c {config_file} -l {number_of_flows} -i {tmp_pcap} -o 'pcapFile:file={pcap_file}'",
        executor=executor,
    ).run()
    print(stdout)
    print(stderr)


@pytest.mark.dev
@pytest.mark.parametrize("file, number_of_flows", [("ipv6-neighbor-discovery.pcap", 2), ("http_get.pcap", 2)])
def test_save_csv(request: pytest.FixtureRequest, executor: Executor, file: str, number_of_flows: int):
    """Test basic usage of save_csv. On host must be installed ipfixprobe with PCAP input plugin and ipfixcol2."""

    def finalize():
        ipfixcol2.cleanup()
        os.remove(f"{FILES_DIR}/{file}.csv")

    request.addfinalizer(finalize)

    ipfixcol2 = Ipfixcol2(executor, input_plugin="tcp")
    ipfixcol2.start()

    rsync = Rsync(executor)
    rsync.push_path(f"{FILES_DIR}/{file}")
    remote_dir = rsync.get_data_directory()
    # executor does not support concurrently running commands in this version of testsuite (4.15)
    executor_copy = create_executor()
    stdout, _ = Tool(
        f"ipfixprobe -i 'pcap;file={remote_dir}/{file}' -o 'ipfix;host=localhost;port=4739'", executor=executor_copy
    ).run()
    print(stdout)

    ipfixcol2.stop()

    fdsdump = ipfixcol2.get_reader()
    fdsdump.save_csv(f"{FILES_DIR}/{file}.csv")

    check_csv(f"{FILES_DIR}/{file}.csv", number_of_flows)


@pytest.mark.dev
@pytest.mark.parametrize("file, number_of_flows", [("/tmp/big.pcap", 40000000)])
def test_save_csv_big(request: pytest.FixtureRequest, executor: Executor, file: str, number_of_flows: int):
    """Test usage of save_csv with large pcap.
    On host must be installed ipfixprobe with PCAP input plugin and ipfixcol2."""

    def finalize():
        ipfixcol2.cleanup()
        os.remove(f"{FILES_DIR}/big.csv")

    request.addfinalizer(finalize)

    generate_pcap(executor, number_of_flows, file)

    ipfixcol2 = Ipfixcol2(executor, input_plugin="tcp")
    ipfixcol2.start()

    # executor does not support concurrently running commands in this version of testsuite (4.15)
    executor_copy = create_executor()
    stdout, _ = Tool(
        f"ipfixprobe -i 'pcap;file={file}' -o 'ipfix;host=localhost;port=4739'", executor=executor_copy
    ).run()
    print(stdout)

    ipfixcol2.stop()

    fdsdump = ipfixcol2.get_reader()

    start = time.time()
    fdsdump.save_csv(f"{FILES_DIR}/big.csv")
    end = time.time()
    size = os.stat(f"{FILES_DIR}/big.csv").st_size / (1024 * 1024)

    print(f"Downloaded flows with size {size:.2f} MB in {end - start:.2f} seconds.")

    check_csv(f"{FILES_DIR}/big.csv", number_of_flows)
