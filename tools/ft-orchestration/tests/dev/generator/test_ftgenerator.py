"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for ft-generator connector.
"""

import filecmp
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest
from src.generator.ft_generator import FtGenerator, FtGeneratorConfig
from src.host.common import get_real_user
from src.host.host import Host
from src.host.storage import RemoteStorage

HOST = os.environ.get("PYTEST_TEST_HOST")
USERNAME = os.environ.get("PYTEST_TEST_HOST_USERNAME")
FILE_DIR = os.path.dirname(os.path.realpath(__file__))
INPUT_DIR = os.path.join(FILE_DIR, "input")
REF_DIR = os.path.join(FILE_DIR, "ref")


@pytest.fixture(name="host", scope="function")
def fixture_host():
    """Fixture for connecting to remote."""

    if not HOST:
        pytest.skip("PYTEST_TEST_HOST env variable must be defined.")

    if USERNAME:
        storage = RemoteStorage(HOST, user=USERNAME)
        host = Host(HOST, storage, user=USERNAME)
    else:
        storage = RemoteStorage(HOST)
        host = Host(HOST, storage)
    yield host
    host.get_storage().remove_all()
    host.get_storage().close()
    host.close()


# pylint: disable=too-few-public-methods
class HostStub(Host):
    """Fake host class. It does not run ft-generator, only returns predefined files."""

    def __init__(
        self,
        res_pcap,
        res_csv,
        host="localhost",
        storage=None,
        user=get_real_user(),
        password=None,
        key_filename=None,
    ):
        super().__init__(host, storage, user, password, key_filename)
        self._res_pcap = res_pcap
        self._res_csv = res_csv

    def run(self, command: str, asynchronous=False, check_rc=True, timeout=None, path_replacement=True):
        """Modified run method."""

        if command.startswith("ft-generator"):
            res = re.search(r"^ft-generator\s+-p\s+\S+\s+-o\s+(\S+)\s+-r\s+(\S+)", command)
            pcap_path, csv_path = res.groups()

            if self.is_local():
                shutil.copy(self._res_pcap, pcap_path)
                shutil.copy(self._res_csv, csv_path)
            else:
                self.get_storage().push(self._res_pcap)
                self.get_storage().push(self._res_csv)

                tmp_path = os.path.join(self.get_storage().get_remote_directory(), os.path.basename(self._res_pcap))
                self.run(f"mv -f {tmp_path} {pcap_path}")

                tmp_path = os.path.join(self.get_storage().get_remote_directory(), os.path.basename(self._res_csv))
                self.run(f"mv -f {tmp_path} {csv_path}")

            return None

        return super().run(command, asynchronous, check_rc, timeout, path_replacement)


@pytest.fixture(name="host_stub", scope="function")
def fixture_host_stub(request: pytest.FixtureRequest):
    """Fixture for getting fake host object."""

    res_pcap, res_csv = request.param

    if not HOST:
        pytest.skip("PYTEST_TEST_HOST env variable must be defined.")

    if USERNAME:
        storage = RemoteStorage(HOST, user=USERNAME)
        host = HostStub(res_pcap, res_csv, HOST, storage, user=USERNAME)
    else:
        storage = RemoteStorage(HOST)
        host = HostStub(res_pcap, res_csv, HOST, storage)
    yield host
    host.get_storage().remove_all()
    host.get_storage().close()
    host.close()


def test_generate(host: Host):
    """Test of generate method with basic profile and no configuration."""

    generator = FtGenerator(host)
    pcap, csv = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap.endswith(".pcap")
    assert csv.endswith(".csv")
    assert Path(pcap).stem == Path(csv).stem
    # names of generated files contains profile name as prefix
    assert "csv_basic_profile" in pcap and "csv_basic_profile" in csv


def test_generate_cache(host: Host):
    """Test of generator cache. The same profile and the same config must result in the same PCAP.
    The same profile with modified configuration must result in different PCAP.
    """

    generator = FtGenerator(host)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap1 == pcap2 and csv1 == csv2

    # force load from index pickle file
    del generator
    generator = FtGenerator(host)
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


def test_generate_cache_path(request: pytest.FixtureRequest, host: Host):
    """Test of generator cache when custom cache path is defined."""

    def finalizer():
        host.run(f"rm -rf {temp_path}", path_replacement=False)

    request.addfinalizer(finalizer)

    temp_path = host.run("mktemp -d").stdout.strip()

    generator = FtGenerator(host, temp_path)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap1) == os.path.dirname(csv1) == temp_path
    assert pcap1 == pcap2 and csv1 == csv2
    assert "csv_basic_profile" in pcap1 and "csv_basic_profile" in csv1

    # force new instance
    del generator
    generator = FtGenerator(host, temp_path)
    pcap3, csv3 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap3) == os.path.dirname(csv3) == temp_path
    assert pcap1 == pcap3 and csv1 == csv3


def test_generate_cache_changed_profile(host: Host):
    """Test of generator cache. When content of profile file is modified, different PCAP must be returned."""

    with tempfile.TemporaryDirectory() as temp_path:
        temp_profile = os.path.join(temp_path, "csv_basic_profile.csv")

        shutil.copy(os.path.join(INPUT_DIR, "csv_basic_profile.csv"), temp_profile)

        generator = FtGenerator(host)
        pcap1, csv1 = generator.generate(temp_profile)

        # modify profile file, delete last row in temp_profile file
        subprocess.run(f"head -n -1 {temp_profile} > tmp.csv && mv tmp.csv {temp_profile}", check=True, shell=True)

        pcap2, csv2 = generator.generate(temp_profile)

        assert os.path.dirname(pcap1) == os.path.dirname(pcap2) == os.path.dirname(csv1) == os.path.dirname(csv2)
        assert pcap1 != pcap2 and csv1 != csv2


def test_generate_cache_changed_filename(host: Host):
    """Test of generator cache. Renamed profile file has the same hash but profile name is part of the key,
    so new files will be generated."""

    with tempfile.TemporaryDirectory() as temp_path:
        temp_profile = os.path.join(temp_path, "renamed_profile.csv")
        shutil.copy(os.path.join(INPUT_DIR, "csv_basic_profile.csv"), temp_profile)

        generator = FtGenerator(host)
        pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
        pcap2, csv2 = generator.generate(temp_profile)

        assert pcap1 != pcap2 and csv1 != csv2
        assert "csv_basic_profile" in pcap1 and "csv_basic_profile" in csv1
        assert "renamed_profile" in pcap2 and "renamed_profile" in csv2


def test_generate_cache_consistency(host: Host):
    """Test of generator cache. When generated PCAP or CSV file changed or missing, files are regenerated."""

    generator = FtGenerator(host)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    # modify result CSV file, remove last line
    host.run(f"head -n -1 {csv1} > tmp.csv && mv tmp.csv {csv1}")

    # regenerate files because CSV hash does not match
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap1 != pcap2 and csv1 != csv2
    assert host.run(f"test -f {pcap1}", check_rc=False).exited > 0
    assert host.run(f"test -f {csv1}", check_rc=False).exited > 0
    assert host.run(f"test -f {pcap2}", check_rc=False).exited == 0
    assert host.run(f"test -f {csv2}", check_rc=False).exited == 0

    # modify result PCAP file, remove last line
    host.run(f"head -n -1 {pcap2} > tmp.csv && mv tmp.csv {pcap2}")

    # regenerate files because PCAP hash does not match
    pcap3, csv3 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert pcap2 != pcap3 and csv2 != csv3
    assert host.run(f"test -f {pcap2}", check_rc=False).exited > 0
    assert host.run(f"test -f {csv2}", check_rc=False).exited > 0
    assert host.run(f"test -f {pcap3}", check_rc=False).exited == 0
    assert host.run(f"test -f {csv3}", check_rc=False).exited == 0

    # remove CSV file
    host.run(f"rm {csv3}")
    # regenerate files because CSV missing
    pcap4, csv4 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    assert pcap3 != pcap4 and csv3 != csv4

    # remove PCAP file
    host.run(f"rm {pcap4}")
    # regenerate files because PCAP missing
    pcap5, csv5 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))
    assert pcap4 != pcap5 and csv4 != csv5


def test_generate_cache_move_dir(request: pytest.FixtureRequest, host: Host):
    """Test of generator cache. Cache dir could be moved, path in index are relative."""

    def finalizer():
        host.run(f"rm -rf {temp_path1}", path_replacement=False)
        host.run(f"rm -rf {temp_path2}", path_replacement=False)

    request.addfinalizer(finalizer)

    temp_path1 = host.run("mktemp -d").stdout.strip()
    temp_path2 = host.run("mktemp -d").stdout.strip()

    generator = FtGenerator(host, temp_path1)
    pcap1, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap1) == os.path.dirname(csv1) == temp_path1

    # move PCAP cache directory
    host.run(f"cp {temp_path1}/* {temp_path2}", path_replacement=False)
    host.run(f"rm -rf {temp_path1}", path_replacement=False)

    generator = FtGenerator(host, temp_path2)
    pcap2, csv2 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    assert os.path.dirname(pcap2) == os.path.dirname(csv2) == temp_path2
    assert os.path.basename(pcap1) == os.path.basename(pcap2) and os.path.basename(csv1) == os.path.basename(csv2)
    assert host.run(f"test -f {pcap2}", check_rc=False).exited == 0
    assert host.run(f"test -f {csv2}", check_rc=False).exited == 0


@pytest.mark.parametrize(
    "host_stub",
    [(os.path.join(INPUT_DIR, "dummy.pcap"), os.path.join(INPUT_DIR, "csv_basic_report.csv"))],
    indirect=True,
)
def test_process_output(request: pytest.FixtureRequest, host_stub: HostStub):
    """Test of CSV report processing. Biflows are splitted and column mapping for StatisticalModel is used."""

    def finalizer():
        os.remove(os.path.join(FILE_DIR, os.path.basename(csv1)))

    request.addfinalizer(finalizer)

    generator = FtGenerator(host_stub)
    _, csv1 = generator.generate(os.path.join(INPUT_DIR, "csv_basic_profile.csv"))

    if host_stub.is_local():
        shutil.copy(csv1, FILE_DIR)
    else:
        host_stub.get_storage().pull(csv1, FILE_DIR)

    assert filecmp.cmp(
        os.path.join(FILE_DIR, os.path.basename(csv1)), os.path.join(REF_DIR, "csv_basic_report_ref.csv")
    )
