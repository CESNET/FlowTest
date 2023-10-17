"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for ft-generator connector.
"""

import filecmp
import os
import re
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest
from lbr_testsuite.executable import Executor, RemoteExecutor, Rsync, Tool
from src.generator.ft_generator import FtGenerator, FtGeneratorConfig

HOST = os.environ.get("PYTEST_TEST_HOST")
USERNAME = os.environ.get("PYTEST_TEST_HOST_USERNAME")
FILE_DIR = os.path.dirname(os.path.realpath(__file__))
INPUT_DIR = os.path.join(FILE_DIR, "input")
REF_DIR = os.path.join(FILE_DIR, "ref")


@pytest.fixture(name="executor", scope="function")
def fixture_executor():
    """Fixture for connecting to remote."""

    if not HOST:
        pytest.skip("PYTEST_TEST_HOST env variable must be defined.")

    if USERNAME:
        executor = RemoteExecutor(HOST, user=USERNAME)
    else:
        executor = RemoteExecutor(HOST)
    yield executor
    Rsync(executor).wipe_data_directory()
    executor.close()


# pylint: disable=too-few-public-methods
class ExecutorStub(RemoteExecutor):
    """Fake executor class. It does not run ft-generator, only returns predefined files."""

    def __init__(
        self,
        res_pcap,
        res_csv,
        *args,
        **kwargs,
    ):
        super().__init__(*args, **kwargs)
        self._res_pcap = res_pcap
        self._res_csv = res_csv

    def run(self, cmd, netns=None, sudo=False, **options):
        """Modified run method."""

        if isinstance(cmd, list):
            cmd = shlex.join(cmd)

        if cmd.startswith("ft-generator"):
            res = re.search(r"^ft-generator\s+-p\s+\S+\s+-o\s+(\S+)\s+-r\s+(\S+)", cmd)
            pcap_path, csv_path = res.groups()

            rsync = Rsync(self)

            rsync.push_path(self._res_pcap)
            rsync.push_path(self._res_csv)

            tmp_path = os.path.join(rsync.get_data_directory(), os.path.basename(self._res_pcap))
            Tool(f"mv -f {tmp_path} {pcap_path}", executor=self).run()

            tmp_path = os.path.join(rsync.get_data_directory(), os.path.basename(self._res_csv))
            Tool(f"mv -f {tmp_path} {csv_path}", executor=self).run()

            return None

        return super().run(cmd, netns, sudo, **options)


@pytest.fixture(name="executor_stub", scope="function")
def fixture_executor_stub(request: pytest.FixtureRequest):
    """Fixture for getting fake executor object."""

    res_pcap, res_csv = request.param

    if not HOST:
        pytest.skip("PYTEST_TEST_HOST env variable must be defined.")

    if USERNAME:
        executor = ExecutorStub(res_pcap, res_csv, HOST, user=USERNAME)
    else:
        executor = ExecutorStub(res_pcap, res_csv, HOST)
    yield executor
    Rsync(executor).wipe_data_directory()
    executor.close()


def host_file_exist(executor: Executor, filename: str) -> bool:
    """Check if file exist on host with executor."""

    cmd = Tool(["test", "-f", filename], executor=executor, failure_verbosity="silent")
    cmd.run()
    return cmd.returncode() == 0


def test_generate(executor: Executor):
    """Test of generate method with basic profile and no configuration."""

    generator = FtGenerator(executor)
    pcap, csv = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap.endswith(".pcap")
    assert csv.endswith(".csv")
    assert Path(pcap).stem == Path(csv).stem
    # names of generated files contains profile name as prefix
    assert "csv_basic_profile" in pcap and "csv_basic_profile" in csv


def test_generate_cache(executor: Executor):
    """Test of generator cache. The same profile and the same config must result in the same PCAP.
    The same profile with modified configuration must result in different PCAP.
    """

    generator = FtGenerator(executor)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap1 == pcap2 and csv1 == csv2

    # force load from index pickle file
    del generator
    generator = FtGenerator(executor)
    pcap3, csv3 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap1 == pcap2 == pcap3 and csv1 == csv2 == csv3

    config = FtGeneratorConfig.from_dict(
        {
            "encapsulation": [
                {
                    "layers": [{"type": "vlan", "id": 10}],
                    "probability": "10%",
                }
            ]
        }
    )
    pcap4, csv4 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"), config)

    assert pcap1 != pcap4 and csv1 != csv4

    config = FtGeneratorConfig.from_dict(
        {
            "encapsulation": [
                {
                    "layers": [{"type": "vlan", "id": 10}],
                    "probability": "10%",
                }
            ],
            "ipv6": {"ip_range": "0123:4567:89ab:cdef::/64"},
        }
    )
    pcap5, csv5 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"), config)

    assert pcap4 != pcap5 and csv4 != csv5


def test_generate_cache_path(request: pytest.FixtureRequest, executor: Executor):
    """Test of generator cache when custom cache path is defined."""

    def finalizer():
        Tool(f"rm -rf {temp_path}", executor=executor).run()

    request.addfinalizer(finalizer)

    temp_path = Tool("mktemp -d", executor=executor).run()[0].strip()

    generator = FtGenerator(executor, temp_path)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap1) == os.path.dirname(csv1) == temp_path
    assert pcap1 == pcap2 and csv1 == csv2
    assert "csv_basic_profile" in pcap1 and "csv_basic_profile" in csv1

    # force new instance
    del generator
    generator = FtGenerator(executor, temp_path)
    pcap3, csv3 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap3) == os.path.dirname(csv3) == temp_path
    assert pcap1 == pcap3 and csv1 == csv3


def test_generate_cache_changed_profile(executor: Executor):
    """Test of generator cache. When content of profile file is modified, different PCAP must be returned."""

    with tempfile.TemporaryDirectory() as temp_path:
        temp_profile = os.path.join(temp_path, "csv_basic_profile.csv")

        shutil.copy(os.path.join(INPUT_DIR, "csv_basic_profile.csv"), temp_profile)

        generator = FtGenerator(executor)
        pcap1, csv1 = generator.generate(temp_profile)

        # modify profile file, delete last row in temp_profile file
        subprocess.run(f"head -n -1 {temp_profile} > tmp.csv && mv tmp.csv {temp_profile}", check=True, shell=True)

        pcap2, csv2 = generator.generate(temp_profile)

        assert os.path.dirname(pcap1) == os.path.dirname(pcap2) == os.path.dirname(csv1) == os.path.dirname(csv2)
        assert pcap1 != pcap2 and csv1 != csv2


def test_generate_cache_changed_filename(executor: Executor):
    """Test of generator cache. Renamed profile file has the same hash but profile name is part of the key,
    so new files will be generated."""

    with tempfile.TemporaryDirectory() as temp_path:
        temp_profile = os.path.join(temp_path, "renamed_profile.csv")
        shutil.copy(os.path.join(INPUT_DIR, "csv_basic_profile.csv"), temp_profile)

        generator = FtGenerator(executor)
        pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
        pcap2, csv2 = generator.generate(temp_profile)

        assert pcap1 != pcap2 and csv1 != csv2
        assert "csv_basic_profile" in pcap1 and "csv_basic_profile" in csv1
        assert "renamed_profile" in pcap2 and "renamed_profile" in csv2


def test_generate_cache_consistency(executor: Executor):
    """Test of generator cache. When generated PCAP or CSV file changed or missing, files are regenerated."""

    generator = FtGenerator(executor)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    # modify result CSV file, remove last line
    Tool(f"head -n -1 {csv1} > tmp.csv && mv tmp.csv {csv1}", executor=executor).run()

    # regenerate files because CSV hash does not match
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap1 != pcap2 and csv1 != csv2
    assert not host_file_exist(executor, pcap1)
    assert not host_file_exist(executor, csv1)
    assert host_file_exist(executor, pcap2)
    assert host_file_exist(executor, csv2)

    # modify result PCAP file, remove last line
    Tool(f"head -n -1 {pcap2} > tmp.csv && mv tmp.csv {pcap2}", executor=executor).run()

    # regenerate files because PCAP hash does not match
    pcap3, csv3 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap2 != pcap3 and csv2 != csv3
    assert not host_file_exist(executor, pcap2)
    assert not host_file_exist(executor, csv2)
    assert host_file_exist(executor, pcap3)
    assert host_file_exist(executor, csv3)

    # remove CSV file
    Tool(f"rm {csv3}", executor=executor).run()
    # regenerate files because CSV missing
    pcap4, csv4 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    assert pcap3 != pcap4 and csv3 != csv4

    # remove PCAP file
    Tool(f"rm {pcap4}", executor=executor).run()
    # regenerate files because PCAP missing
    pcap5, csv5 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    assert pcap4 != pcap5 and csv4 != csv5


def test_generate_cache_move_dir(request: pytest.FixtureRequest, executor: Executor):
    """Test of generator cache. Cache dir could be moved, path in index are relative."""

    def finalizer():
        Tool(f"rm -rf {temp_path1}", executor=executor).run()
        Tool(f"rm -rf {temp_path2}", executor=executor).run()

    request.addfinalizer(finalizer)

    temp_path1 = Tool("mktemp -d", executor=executor).run()[0].strip()
    temp_path2 = Tool("mktemp -d", executor=executor).run()[0].strip()

    generator = FtGenerator(executor, temp_path1)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap1) == os.path.dirname(csv1) == temp_path1

    # move PCAP cache directory
    Tool(f"cp {temp_path1}/* {temp_path2}", executor=executor).run()
    Tool(f"rm -rf {temp_path1}", executor=executor).run()

    generator = FtGenerator(executor, temp_path2)
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap2) == os.path.dirname(csv2) == temp_path2
    assert os.path.basename(pcap1) == os.path.basename(pcap2) and os.path.basename(csv1) == os.path.basename(csv2)
    assert host_file_exist(executor, pcap2)
    assert host_file_exist(executor, csv2)


@pytest.mark.parametrize(
    "executor_stub",
    [(os.path.join(INPUT_DIR, "dummy.pcap"), os.path.join(INPUT_DIR, "csv_basic_report.csv"))],
    indirect=True,
)
def test_process_output(request: pytest.FixtureRequest, executor_stub: ExecutorStub):
    """Test of CSV report processing. Biflows are splitted and column mapping for StatisticalModel is used."""

    def finalizer():
        os.remove(os.path.join(FILE_DIR, os.path.basename(csv1)))

    request.addfinalizer(finalizer)

    generator = FtGenerator(executor_stub)
    _, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    Rsync(executor_stub).pull_path(csv1, FILE_DIR)

    assert filecmp.cmp(
        os.path.join(FILE_DIR, os.path.basename(csv1)), os.path.join(REF_DIR, "csv_basic_report_ref.csv")
    )
