"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Simple development tests of Host and Storage class.
"""

import os

import pytest
from src.host.common import ssh_agent_enabled
from src.host.host import Host
from src.host.storage import RemoteStorage

HOST = os.environ.get("PYTEST_TEST_HOST")
FILES_DIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "files")
USERNAME = os.environ.get("PYTEST_TEST_HOST_USERNAME")
PASSWORD = os.environ.get("PYTEST_TEST_HOST_PASSWORD")
KEY = os.environ.get("PYTEST_TEST_HOST_KEY")


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
            storage = RemoteStorage(HOST, key_filename=KEY, **user_args)
            host = Host(HOST, storage, key_filename=KEY, **user_args)
            combinations.append({"host": host, "storage": storage})

        if PASSWORD is None:
            combinations.append(pytest.param(None, marks=pytest.mark.skip(reason="password not defined")))
        else:
            storage = RemoteStorage(HOST, password=PASSWORD, **user_args)
            host = Host(HOST, storage, password=PASSWORD, **user_args)
            combinations.append({"host": host, "storage": storage})

        if not ssh_agent_enabled():
            combinations.append(pytest.param(None, marks=pytest.mark.skip(reason="ssh agent not defined")))
        else:
            storage = RemoteStorage(HOST, **user_args)
            host = Host(HOST, storage, **user_args)
            combinations.append({"host": host, "storage": storage})

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
def test_host_local_ls_single_file(require_root):
    """Test 'ls' command on single file on localhost."""

    host = Host()
    res = host.run(f"ls {FILES_DIR}/a.txt | xargs echo")
    assert host.is_local()
    assert host.get_storage() is None
    assert res.stdout.strip() == f"{FILES_DIR}/a.txt"


@pytest.mark.dev
def test_host_local_ls_single_file_wildcard(require_root):
    """Test 'ls' command on single file on localhost."""

    host = Host()
    wc_path = FILES_DIR.replace("tests/dev/host/files", "t?sts/*/h*s?/*****")
    res = host.run(f"ls {wc_path}/a.txt | xargs echo")
    assert res.stdout.strip() == f"{FILES_DIR}/a.txt"


@pytest.mark.dev
def test_host_local_cat_single_file(require_root):
    """Test 'cat' command on single file on localhost."""

    host = Host()
    res = host.run(f"cat {FILES_DIR}/a.txt")
    assert res.stdout.strip() == "a"


@pytest.mark.dev
def test_host_local_cat_two_files(require_root):
    """Test 'cat' command on two files on localhost."""

    host = Host()
    res = host.run(f"cat {FILES_DIR}/a.txt {FILES_DIR}/b.txt")
    # Remove any newlines from output
    assert res.stdout.replace("\r\n", "").replace("\n", "") == "ab"


@pytest.mark.dev
def test_host_local_ls_dir(require_root):
    """Test 'ls' command on directory on localhost."""

    host = Host()
    res = host.run(f"ls {FILES_DIR}")
    assert res.stdout.strip() == "a.txt  b.txt  c.txt"


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_ls_single_file(connection_parameters, require_root):
    """Test 'ls' command on single file on remote host."""

    host = connection_parameters["host"]
    storage = connection_parameters["storage"]

    res = host.run(f"ls {FILES_DIR}/a.txt | xargs echo")
    assert not host.is_local()
    assert host.get_storage() is not None
    assert res.stdout.strip() == storage.get_remote_directory() + "/a.txt"


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_ls_single_file_wildcard(connection_parameters, require_root):
    """Test 'ls' command on single file on remote host."""

    host = connection_parameters["host"]
    storage = connection_parameters["storage"]

    wc_path = FILES_DIR.replace("tests/dev/host/files", "t?sts/*/h*s?/*****")
    res = host.run(f"ls {wc_path}/a.txt | xargs echo")
    assert res.stdout.strip() == storage.get_remote_directory() + "/a.txt"


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_cat_single_file(connection_parameters, require_root):
    """Test 'cat' command on single file on remote host."""

    host = connection_parameters["host"]

    res = host.run(f"cat {FILES_DIR}/a.txt")
    assert res.stdout.strip() == "a"


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_cat_two_files(connection_parameters, require_root):
    """Test 'cat' command on two files on remote host."""

    host = connection_parameters["host"]

    res = host.run(f"cat {FILES_DIR}/a.txt {FILES_DIR}/b.txt")
    assert res.stdout.strip() == "a\r\nb"


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_ls_dir(connection_parameters, require_root):
    """Test 'ls' command on directory on remote host."""

    host = connection_parameters["host"]

    res = host.run(f"ls {FILES_DIR}")
    assert res.stdout.strip() == "a.txt  b.txt  c.txt"


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_previous_test_files_exist(connection_parameters, require_root):
    """Test that files used in previous tests exist on remote host/storage."""

    host = connection_parameters["host"]
    storage = connection_parameters["storage"]

    res = host.run(f"ls {storage.get_remote_directory()}")
    assert "a.txt" in res.stdout
    assert "b.txt" in res.stdout
    assert "files" in res.stdout

    res = host.run(f"ls {storage.get_remote_directory()}/files")
    assert "a.txt" in res.stdout
    assert "b.txt" in res.stdout
    assert "c.txt" in res.stdout


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_delete_files(connection_parameters, require_root):
    """Test deletion of files on remote host/storage."""

    host = connection_parameters["host"]
    storage = connection_parameters["storage"]

    storage.remove("b.txt")
    res = host.run(f"ls {storage.get_remote_directory()}")
    assert "a.txt" in res.stdout
    assert "b.txt" not in res.stdout
    assert "files" in res.stdout

    storage.remove("files/b.txt")
    res = host.run(f"ls {storage.get_remote_directory()}/files")
    assert "a.txt" in res.stdout
    assert "b.txt" not in res.stdout
    assert "c.txt" in res.stdout


@pytest.mark.dev
@pytest.mark.parametrize("connection_parameters", parametrize_remote_connection(), ids=connection_ids())
def test_host_remote_delete_all_files(connection_parameters, require_root):
    """Test deletion of all files on remote host/storage."""

    host = connection_parameters["host"]
    storage = connection_parameters["storage"]

    storage.remove_all()
    res = host.run(f"ls {storage.get_remote_directory()}")
    assert res.stdout == ""


@pytest.mark.dev
@pytest.mark.skipif(HOST is None, reason="host not defined")
@pytest.mark.skipif(not ssh_agent_enabled(), reason="ssh agent not defined")
def test_host_remote_custom_directory(require_root):
    """Test for using custom working directory for remote host/storage."""

    work_dir = "/tmp/test_host_remote_custom_directory"

    # Create custom directory on remote
    host = Host(HOST, storage=RemoteStorage(HOST))
    host.run(f"mkdir -p {work_dir}", path_replacement=False)

    # Redefine host with new remote storage
    host = Host(HOST, RemoteStorage(HOST, work_dir=work_dir))
    assert host.get_storage().get_remote_directory() == work_dir

    res = host.run(f"ls {FILES_DIR}/a.txt | xargs echo")
    assert res.stdout.strip() == f"{work_dir}/a.txt"

    host.get_storage().remove_all()
    res = host.run(f"ls {work_dir}")
    assert res.stdout == ""
    res = host.run(f"ls {host.get_storage().get_remote_directory()}")
    assert res.stdout == ""
